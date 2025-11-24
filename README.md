PXLcam — Retro Pixel ESP32-CAM Camera

A custom-built retro-pixel camera using ESP32-CAM + OLED + SD storage + custom 3D case.

<!-- opcional, pode remover -->

📍 1. Visão Geral

A PXLcam é uma câmera digital retrô baseada no módulo ESP32-CAM, capaz de:

capturar fotos pixeladas (estilo 8-bit / lo-fi)

salvar imagens em cartão microSD

exibir mensagens e feedback em um display OLED I2C

operar em um corpo impresso em 3D com estética retrô moderna

funcionar com bateria LiPo recarregável (USB-C + TP4056)

O firmware foi projetado em arquitetura modular de alta estabilidade, incluindo:

máquina de estados (FSM)

pipeline captura → filtro → armazenamento

fallback automático caso PSRAM falhe

proteção contra GPIOs críticos

logs, validação, mensagens claras no OLED

⚡ 2. Hardware Utilizado
Componente	Função	Observação
ESP32-CAM (AI Thinker)	CPU + sensor OV2640	Requer fonte confiável
ESP32-CAM-MB (CH340G)	Programação via USB	Facilita uploads
Display OLED 0.96 I²C (128x64)	Feedback ao usuário	Endereço padrão 0x3C
Botão DS-314 (preto)	Captura da foto	Sem trava
Botão PBS12A (quadrado, com trava)	Liga/Desliga geral	Atua como chave física
Bateria LiPo 3.7V (1800mAh)	Alimentação	Excelente autonomia
Módulo TP4056 USB-C (proteção)	Carregamento da bateria	Com cutoff automático
Jumpers 10cm F/F, M/F, M/M	Cabos internos	10 cm é ideal
Cartão microSD classe 10	Armazenamento das fotos	Necessário
⚠️ 3. Cuidados Importantes (antes de montar)
✔ GPIO12 — EXTREMAMENTE IMPORTANTE

O pino GPIO12 é usado pelo ESP32 para selecionar o modo de boot.
Se estiver LOW durante o boot → o módulo não liga.

Na PXLcam:

o botão de captura NÃO DEVE usar GPIO12

o botão de power é físico → não afeta GPIO12

documente para nunca manter GPIO12 pressionado ao ligar

✔ Fonte estável

ESP32-CAM puxa picos de corrente ao inicializar a câmera

Use bateria + TP4056 (ideal)

Evite alimentar só pela USB do computador

✔ SD card

Dê preferência para Classe 10 ou superior

SD ruim = falhas de gravação / travamentos

🧩 4. Arquitetura do Firmware

A arquitetura é dividida em módulos independentes:

/src
  main.cpp                → loop principal + AppController
  app_controller.cpp      → máquina de estados
  camera_config.cpp       → inicialização da câmera
  display_service.cpp     → OLED
  storage_service.cpp     → SD / salvamento
  pixel_filter.cpp        → filtro estilo retro/pixel
/include
  *.h headers
/lib
  serviços auxiliares

🔄 5. Máquina de Estados (State Machine)
Boot
 └→ InitDisplay
      └→ InitStorage
            └→ InitCamera
                  └→ Idle
                        └→ Capture
                              └→ Filter
                                    └→ Save
                                          └→ Feedback
                                                └→ Idle
Error
 └→ Retry / Display Error

🖥 6. Pinagem Oficial da PXLcam
ESP32-CAM → OLED
OLED	ESP32-CAM
GND	GND
VCC	3.3V
SDA	GPIO 15
SCL	GPIO 14
Botão de Captura
Componente	Pino
Botão (sem trava)	GPIO 13
SD Card (interno do ESP32-CAM)

Gerenciado automaticamente pelo driver SD_MMC.

Botão Liga/Desliga (trava)

Conecta e desconecta o positivo da bateria.
Não passa pelo ESP32.

💾 7. Dependências (PlatformIO)

platformio.ini:

[env:esp32cam]
platform = espressif32
board = esp32cam
framework = arduino

board_build.flash_mode = qio
monitor_speed = 115200
upload_speed = 921600

build_flags =
  -DBOARD_HAS_PSRAM
  -mfix-esp32-psram-cache-issue

lib_deps =
  adafruit/Adafruit GFX Library @ ^1.11.9
  adafruit/Adafruit SSD1306 @ ^2.5.9
  espressif/esp32-camera @ ^2.0.4

🛠 8. Como rodar o projeto — VS Code + PlatformIO
1) Instale:

VS Code

Extensão PlatformIO IDE

2) Clone o repositório:
git clone https://github.com/SEU_USUARIO/PXLcam
cd PXLcam/firmware

3) Abra no VS Code
code .

4) Plugue o ESP32-CAM-MB e clique em:

➡ PlatformIO → Upload

📸 9. Como usar a PXLcam
1. Segure o botão de power (quadrado) para ligar
2. Tela OLED inicializa → “PXLcam Ready”
3. Pressione o botão de captura

captura frame

aplica o pixel filter

salva no SD

feedback no display

4. Arquivos ficam em:

/captures/IMG_00001.rgb

📋 10. Checklist para quando o hardware chegar
👉 Antes de ligar:

bateria carregada

SD card inserido

conexões revisadas

cabo flat da câmera bem encaixado

👉 Primeiro boot:

verificar se display inicia

verificar mensagem de SD montado

verificar se câmera OK

👉 Testes:

tirar 10 fotos seguidas

medir estabilidade

simular ausência do SD

pressionar botão rápido (debounce)

deixar ligado 10 min (temperatura)

🚀 11. Roadmap
Etapa	Status
Firmware Starter	✔ feito
README profissional	✔ feito
Arquitetura modular	✔ pronto
Testes com hardware	🔜 quando chegar
Design da carcaça 3D	🔜 próximo passo
Branding final	🔜
Release v1.0	futuro
🧢 12. Licença

MIT License — livre para uso pessoal e comercial.

🎨 13. Branding

Nome oficial: PXLcam
Conceito visual: retro-futurismo + minimalismo + pixel grid
Logotipo: armazenado em branding/logo/
