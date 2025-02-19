# Controle de LEDs, Display SSD1306 e Joystick na Raspberry Pi Pico W

## Descrição do Projeto

Este projeto utiliza uma **Raspberry Pi Pico W** e a **BitDogLab** para controlar LEDs RGB, uma matriz de LEDs WS2812 e um **display OLED SSD1306**. A interação é realizada através de um **joystick analógico** e **botões de entrada**.

## Funcionalidades
- **Leitura de Joystick** via ADC (eixos X e Y) para movimentar um quadrado no display SSD1306.
- **Controle de LEDs RGB** via PWM:
  - LED vermelho controlado pelo eixo X do joystick.
  - LED azul controlado pelo eixo Y do joystick.
  - LED verde togglado pelo botão do joystick.
- **Interrupções e debounce:**
  - Botão do joystick altera a borda do display.
  - Botão A ativa/desativa o PWM dos LEDs RGB.
- **Display OLED SSD1306:**
  - Movimentação do quadrado 8x8 conforme joystick.
  - Alternação do estilo de borda a cada clique do joystick.

## Componentes Utilizados
- **Raspberry Pi Pico W**
- **Matriz de LEDs WS2812** (5x5, totalizando 25 LEDs)
- **Joystick analógico** para controle
- **LEDs RGB** (Vermelho, Azul, Verde)
- **Display SSD1306** via I2C
- **Botões** para interação

## Configuração do Hardware

### **Pinos utilizados:**
| Componente        | Pino na Pico W |
|------------------|--------------|
| Joystick X      | GPIO27 (ADC1) |
| Joystick Y      | GPIO26 (ADC0) |
| Joystick SW     | GPIO22        |
| LED Vermelho    | GPIO13 (PWM)  |
| LED Azul        | GPIO12 (PWM)  |
| LED Verde       | GPIO11        |
| Botão A        | GPIO5         |
| Display SSD1306 | I2C1 (GPIO14, GPIO15) |

## Como Compilar e Rodar

### **1. Instale o Raspberry Pi Pico SDK**
Siga as instruções oficiais do SDK:
https://github.com/raspberrypi/pico-sdk

### **2. Clone este repositório**
```sh
    git clone https://github.com/seu-usuario/seu-repositorio.git
    cd seu-repositorio
```

### **3. Abra o VS Code e importe o projeto:**
   - Vá até a **Extensão Raspberry Pi Pico**.
   - Selecione **Import Project**.
   - Escolha a pasta do repositório clonado.
   - Clique em **Import**.

### **4. Compile o código:**
   - Utilize a opção de **Build** da extensão.

### **5. Carregue o binário na Pico**
1. Pressione e segure o **botão BOOTSEL** da Raspberry Pi Pico W.
2. Conecte-a ao PC via **USB**.
3. Copie o arquivo `.uf2` gerado para a unidade montada.

## Exemplo de Uso
1. O display SSD1306 inicia com um quadrado no centro.
2. Movimente o **joystick** para deslocar o quadrado.
3. Pressione **o botão do joystick** para alternar a borda do display.
4. Pressione **o Botão A** para ativar/desativar o PWM dos LEDs RGB.
5. O **LED verde** alterna entre ligado/desligado ao pressionar o botão do joystick.

## Possíveis Melhorias
- Implementação de exibição de caracteres e números no display OLED.
- Adição de mais efeitos visuais nos LEDs RGB.
- Controle de interface por comunicação UART.

## Autor
- [Gabriel Silva Sampaio]


demonstração:https://www.dropbox.com/scl/fi/n01qaq3quxv85by6qgdmd/VID_20250218_224615.mp4?rlkey=r8b0e3fpo4v147wzd1bev0x3s&st=r3dnvkdn&dl=0
