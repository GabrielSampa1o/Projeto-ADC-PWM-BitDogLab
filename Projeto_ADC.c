/**
 * Exemplo de Projeto - BitDogLab + Joystick + 2 LEDs PWM + LED Verde + Botões + Display SSD1306
 *
 * Requisitos atendidos:
 *   1) Uso de ADC no RP2040 para ler joystick (VRX, VRY).
 *   2) Controle via PWM dos LEDs Vermelho e Azul com base no eixo X e Y do joystick.
 *   3) LED Verde togglado pelo botão do joystick (com IRQ + debounce).
 *   4) Botão A para ativar/desativar PWM (IRQ + debounce).
 *   5) Movimentar um quadrado 8x8 no display SSD1306 conforme joystick.
 *   6) Alternar borda do display (diferentes estilos) a cada clique do joystick.
 *   7) Código comentado e estruturado.
 *   8) I2C via GPIO14 (SDA) e GPIO15 (SCL).
 *
 * Autor: [Seu Nome]
 * Data: [Data de desenvolvimento]
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include "pico/stdlib.h"
 #include "hardware/adc.h"
 #include "hardware/pwm.h"
 #include "hardware/irq.h"
 #include "hardware/i2c.h"
 
 // ---------------------------------------------------------------------
 // Configurações de pinos (BitDogLab)
 #define JOY_VRX_PIN    27       // ADC1
 #define JOY_VRY_PIN    26       // ADC0
 #define JOY_SW_PIN     22       // Botão do joystick
 
 #define LED_R_PIN      13       // LED Vermelho
 #define LED_B_PIN      12       // LED Azul
 #define LED_G_PIN      11       // LED Verde
 
 #define BUT_A_PIN      5        // Botão A
 
 // I2C do Display SSD1306
 #define I2C_SDA_PIN    14
 #define I2C_SCL_PIN    15
 #define I2C_PORT       i2c1     // Poderia ser i2c0, mas aqui escolhemos i2c1
 
 // Dimensões do display
 #define OLED_WIDTH     128
 #define OLED_HEIGHT    64
 
 // Valor central do joystick e máximo do ADC (0..4095)
 #define JOY_CENTER     2048
 #define ADC_MAX        4095
 
 // Tempo de debounce (em microssegundos)
 #define DEBOUNCE_TIME_US   200000
 
 // ---------------------------------------------------------------------
 // Estrutura e funções da biblioteca SSD1306 (resumimos aqui)
 #include "ssd1306.h"  // <--- Ajustar para o seu path real
 #include "font.h"     // <--- Ajustar para o seu path real
 
 // ---------------------------------------------------------------------
 // Variáveis globais para controle de estado
 static volatile bool g_led_green_state   = false;  // estado do LED verde
 static volatile bool g_pwm_enable        = true;   // se PWM (vermelho/azul) está ativo
 static volatile uint8_t g_border_style   = 0;      // estilo de borda que varia a cada clique do joystick
 static absolute_time_t g_last_irq_time_joy;        // controle de debounce do joystick
 static absolute_time_t g_last_irq_time_a;          // controle de debounce do botão A
 
 // Ponteiro/estrutura do display
 static ssd1306_t g_oled_dev;
 
 // ---------------------------------------------------------------------
 // Função auxiliar para inicializar PWM em um GPIO e definir seu "wrap"
 uint pwm_init_gpio(uint gpio, uint wrap) {
     gpio_set_function(gpio, GPIO_FUNC_PWM);
     uint slice_num = pwm_gpio_to_slice_num(gpio);
 
     // Define o valor máximo do contador (TOP)
     pwm_set_wrap(slice_num, wrap);
 
     // Para começar com nível 0
     pwm_set_chan_level(slice_num, pwm_gpio_to_channel(gpio), 0);
 
     // Ativa o PWM
     pwm_set_enabled(slice_num, true);
     return slice_num;
 }
 
 // ---------------------------------------------------------------------
 // Função para mapear valor do joystick [0..4095], com centro=2048 => 0 e extremos => ~4095
 // Se o joystick estiver perto do centro, o retorno é 0; quanto mais perto do extremo (0 ou 4095), maior o valor.
 static uint16_t map_joystick_to_pwm(uint16_t raw) {
     // Desloca em torno de 2048
     int32_t offset = (int32_t)raw - (int32_t)JOY_CENTER;
     // Poderíamos definir uma "zona morta" de ~50 ao redor do centro, se desejado.
     const int dead_zone = 30;  // Ajuste conforme desejado
     if (offset > -dead_zone && offset < dead_zone) {
         return 0; // dentro da zona morta => 0
     }
 
     // Intensidade proporcional à distância do centro
     int32_t magnitude = abs(offset);
 
     // Escala simples: se magnitude=2048 => PWM ~ 4096 (limitado em 4095)
     int32_t pwm_level = magnitude * 2;
     if (pwm_level > 4095) pwm_level = 4095;
 
     return (uint16_t)pwm_level;
 }
 
 // ---------------------------------------------------------------------
 // Interrupção do Joystick (SW). Vamos togglar LED Verde e mudar estilo de borda
 void irq_joy_sw(uint gpio, uint32_t events) {
     // Debounce: verifica se já passou tempo suficiente desde a última IRQ
     absolute_time_t now = get_absolute_time();
     if (absolute_time_diff_us(g_last_irq_time_joy, now) < DEBOUNCE_TIME_US) {
         return; // se não passou o tempo de debounce, ignora
     }
     g_last_irq_time_joy = now;
 
     // Toggla estado do LED verde
     g_led_green_state = !g_led_green_state;
     gpio_put(LED_G_PIN, g_led_green_state);
 
     // Muda o estilo da borda (por exemplo, 0..2)
     g_border_style = (g_border_style + 1) % 3;
 }
 
 // ---------------------------------------------------------------------
 // Interrupção do botão A. Toggla habilitação do PWM
 void irq_button_a(uint gpio, uint32_t events) {
     // Debounce
     absolute_time_t now = get_absolute_time();
     if (absolute_time_diff_us(g_last_irq_time_a, now) < DEBOUNCE_TIME_US) {
         return;
     }
     g_last_irq_time_a = now;
 
     // Toggla a flag global
     g_pwm_enable = !g_pwm_enable;
 
     // Se desativado, podemos colocar o nível 0 imediatamente
     if (!g_pwm_enable) {
         // Zera o PWM do LED vermelho e azul
         uint slice_r = pwm_gpio_to_slice_num(LED_R_PIN);
         uint slice_b = pwm_gpio_to_slice_num(LED_B_PIN);
         pwm_set_gpio_level(LED_R_PIN, 0);
         pwm_set_gpio_level(LED_B_PIN, 0);
         // (ou poderia deixar no loop principal)
     }
 }
 
 // ---------------------------------------------------------------------
 // Desenha uma borda em torno do display com estilos diferentes
 static void draw_border(ssd1306_t *disp, uint8_t style) {
     // style pode ser 0, 1 ou 2 - crie variações a gosto
     switch (style) {
         case 0:
             // borda simples
             ssd1306_rect(disp, 0, 0, disp->width, disp->height, true, false);
             break;
         case 1:
             // borda dupla? (desenhar 2 retângulos)
             ssd1306_rect(disp, 0, 0, disp->width, disp->height, true, false);
             ssd1306_rect(disp, 2, 2, disp->width-4, disp->height-4, true, false);
             break;
         case 2:
             // borda tracejada (simples, mas usando ssd1306_line)
             // Podemos "pular" pixels a cada 2
             for (uint8_t x = 0; x < disp->width; x+=2) {
                 ssd1306_pixel(disp, x, 0, true);
                 ssd1306_pixel(disp, x, disp->height-1, true);
             }
             for (uint8_t y = 0; y < disp->height; y+=2) {
                 ssd1306_pixel(disp, 0, y, true);
                 ssd1306_pixel(disp, disp->width-1, y, true);
             }
             break;
     }
 }
 
 // ---------------------------------------------------------------------
 // Programa principal
 int main() {
     // --- Inicializações básicas ---
     stdio_init_all();
 
     // Inicializa entradas do joystick (ADC)
     adc_init();
     adc_gpio_init(JOY_VRX_PIN); // GPIO27 => canal 1
     adc_gpio_init(JOY_VRY_PIN); // GPIO26 => canal 0
 
     // Pino do botão do joystick
     gpio_init(JOY_SW_PIN);
     gpio_set_dir(JOY_SW_PIN, GPIO_IN);
     gpio_pull_up(JOY_SW_PIN);
 
     // Botão A
     gpio_init(BUT_A_PIN);
     gpio_set_dir(BUT_A_PIN, GPIO_IN);
     gpio_pull_up(BUT_A_PIN);
 
     // LEDs
     gpio_init(LED_G_PIN);
     gpio_set_dir(LED_G_PIN, GPIO_OUT);
     gpio_put(LED_G_PIN, false);
 
     // Obs: LED_R_PIN e LED_B_PIN serão configurados como PWM
     uint16_t pwm_wrap = 4095; // TOP do PWM (0..4095 = 12 bits)
     pwm_init_gpio(LED_R_PIN, pwm_wrap);
     pwm_init_gpio(LED_B_PIN, pwm_wrap);
 
     // --- Configura interrupções (IRQ) para botões ---
     // Modo: IRQ na borda de descida (quando pressionado) ou nos dois?
     // Em geral, joystick SW = 0 ao pressionar. Vamos usar borda de descida.
     gpio_set_irq_enabled_with_callback(JOY_SW_PIN, GPIO_IRQ_EDGE_FALL, true, &irq_joy_sw);
     gpio_set_irq_enabled_with_callback(BUT_A_PIN,   GPIO_IRQ_EDGE_FALL, true, &irq_button_a);
     // IMPORTANTE: a partir do SDK 1.5, podemos registrar callbacks separados
     // mas se estiver usando <1.5, pode ser preciso um callback único e testar o 'gpio'.
 
     // Inicializa o tempo para debounce
     g_last_irq_time_joy = get_absolute_time();
     g_last_irq_time_a   = get_absolute_time();
 
     // --- Inicialização I2C e Display SSD1306 ---
     i2c_init(I2C_PORT, 400 * 1000); // 400kHz
     gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
     gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
     gpio_pull_up(I2C_SDA_PIN);
     gpio_pull_up(I2C_SCL_PIN);
 
     // Preenche estrutura do SSD1306
     ssd1306_init(&g_oled_dev,
                  OLED_WIDTH, 
                  OLED_HEIGHT,
                  false,        // external_vcc = false
                  0x3C,         // Endereço padrão do display
                  I2C_PORT);
     // Envia configuração inicial
     ssd1306_config(&g_oled_dev);
 
     // Limpa o display
     ssd1306_fill(&g_oled_dev, false);
     ssd1306_send_data(&g_oled_dev);
 
     // Variáveis para posição do quadrado 8x8
     uint8_t square_x = (OLED_WIDTH - 8)/2;
     uint8_t square_y = (OLED_HEIGHT - 8)/2;
 
     // Loop principal
     while (true) {
         // 1) Ler joystick (VRX => canal 1, VRY => canal 0)
         adc_select_input(1); // VRX
         uint16_t vrx_value = adc_read();
         adc_select_input(0); // VRY
         uint16_t vry_value = adc_read();
 
         // 2) Calcular valores de PWM para LED Vermelho (X) e LED Azul (Y)
         if (g_pwm_enable) {
             uint16_t pwm_red  = map_joystick_to_pwm(vrx_value);
             uint16_t pwm_blue = map_joystick_to_pwm(vry_value);
 
             pwm_set_gpio_level(LED_R_PIN, pwm_red);
             pwm_set_gpio_level(LED_B_PIN, pwm_blue);
         } else {
             // Se está desativado, manter em zero
             pwm_set_gpio_level(LED_R_PIN, 0);
             pwm_set_gpio_level(LED_B_PIN, 0);
         }
 
         // 3) Atualizar o display:
         //    - Apagamos (fill=false)
         //    - Desenhamos a borda com estilo g_border_style
         //    - Calculamos a posição do quadrado 8x8
         //      mapeando [0..4095] -> [0..(largura-8)]
         //      Obs: para ficar “no centro” quando ~2048, apenas deixamos
         //      linear. (Você pode criar um mapeamento que defina 2048 como
         //      meio do range, caso deseje).
         ssd1306_fill(&g_oled_dev, false);
 
         draw_border(&g_oled_dev, g_border_style);
 
         // Ajuste de posição do quadrado (simples):
         square_x = (vrx_value * (OLED_WIDTH-8)) / ADC_MAX;
         square_y = (vry_value * (OLED_HEIGHT-8)) / ADC_MAX;
 
         // Desenha o quadrado (8x8) preenchido
         ssd1306_rect(&g_oled_dev, square_y, square_x, 8, 8, true, true);
 
         // Envia buffer para o display
         ssd1306_send_data(&g_oled_dev);
 
         // Exemplo de debug via USB serial (opcional):
         printf("VRX=%u  VRY=%u  SW=%d  PWM_En=%d\n",
                 vrx_value, vry_value, !gpio_get(JOY_SW_PIN), g_pwm_enable);
 
         // Espera um pouco
         sleep_ms(30);
     }
 
     return 0;
 }
 