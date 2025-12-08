# Copilot Autorun Instructions — PXLcam ESP32-CAM

Você deve seguir SEMPRE as instruções abaixo antes de gerar qualquer código neste projeto:

1. Nunca apagar arquivos existentes da pasta `src/`.
2. Nunca criar um segundo `main.cpp`. O arquivo principal já existe e deve ser mantido.
3. Sempre que for modificar lógica:  
   – Colocar o código principal do programa em `src/app_controller.cpp`  
   – Funções públicas devem ser declaradas em `include/app_controller.h`
4. O loop principal deve chamar:  
   ```cpp
   void loop() {
       app.tick();
       yield();
   }
