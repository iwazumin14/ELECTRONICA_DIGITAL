
#include <Arduino.h>
#include "esp_camera.h"

// ==========================================
// ESTACION TUI BILINGUE
// ESP32-CAM AI THINKER + GC2145
// ==========================================

// Mapa de pines AI Thinker validado
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// UART: solamente transmision
#define CAM_TX 14
#define UART_BAUD 115200

#define CAPTURE_INTERVAL 2000

bool cameraReady = false;
unsigned long captureCount = 0;

// ==========================================
// TRANSMISION UART
// ==========================================

void sendMessage(const char *message) {

  Serial2.println(message);
  Serial2.flush();

  Serial.println(message);
}

// ==========================================
// INICIALIZAR CAMARA
// ==========================================

bool initializeCamera() {

  bool hasPSRAM = psramFound();

  Serial.println("INICIANDO GC2145");

  Serial.printf(
    "PSRAM: %s\n",
    hasPSRAM ? "SI" : "NO"
  );

  if (!hasPSRAM) {
    sendMessage("CAM|PSRAM_WARNING");
  }

  camera_config_t config = {};

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  // Configuracion validada
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_QQVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  config.fb_location = hasPSRAM
      ? CAMERA_FB_IN_PSRAM
      : CAMERA_FB_IN_DRAM;

  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  Serial.println("Inicializando sensor...");

  esp_err_t error = esp_camera_init(&config);

  if (error != ESP_OK) {

    Serial.printf(
      "ERROR DE INICIALIZACION: 0x%x\n",
      (unsigned int)error
    );

    sendMessage("CAM|INIT_ERROR");

    return false;
  }

  sensor_t *sensor = esp_camera_sensor_get();

  if (sensor != nullptr) {

    Serial.printf(
      "PID DETECTADO: 0x%04X\n",
      sensor->id.PID
    );
  }

  Serial.println("CAMARA_INICIALIZADA_OK");

  sendMessage("CAM|INIT_OK|GC2145|RGB565");

  return true;
}

// ==========================================
// CAPTURA Y VALIDACION
// ==========================================

void captureImage() {

  camera_fb_t *fb = esp_camera_fb_get();

  captureCount++;

  if (fb == nullptr) {

    Serial.println("FALLO DE CAPTURA");

    sendMessage("CAM|FRAME_ERROR");

    return;
  }

  size_t expectedSize =
      (size_t)fb->width *
      fb->height * 2;

  bool validFrame =
      fb->format == PIXFORMAT_RGB565 &&
      fb->len == expectedSize;

  if (validFrame) {

    char message[80];

    snprintf(
      message,
      sizeof(message),
      "CAM|FRAME_OK|%lu|%u|%u|%u",
      captureCount,
      (unsigned)fb->width,
      (unsigned)fb->height,
      (unsigned)fb->len
    );

    Serial.printf(
      "CAPTURA %lu CORRECTA: %ux%u, %u bytes\n",
      captureCount,
      (unsigned)fb->width,
      (unsigned)fb->height,
      (unsigned)fb->len
    );

    sendMessage(message);

  } else {

    Serial.println("FOTOGRAMA INVALIDO");

    sendMessage("CAM|FRAME_BAD");
  }

  // Liberar memoria
  esp_camera_fb_return(fb);
}

// ==========================================
// SETUP
// ==========================================

void setup() {

  Serial.begin(115200);

  // GPIO14: TX
  // RX deshabilitado
  Serial2.begin(
    UART_BAUD,
    SERIAL_8N1,
    -1,
    CAM_TX
  );

  delay(1500);

  sendMessage("CAM|BOOT");

  cameraReady = initializeCamera();

  if (!cameraReady) {

    Serial.println("CAMARA NO DISPONIBLE");

    return;
  }

  Serial.println("SISTEMA LISTO");
}

// ==========================================
// LOOP
// ==========================================

void loop() {

  if (!cameraReady) {
    delay(3000);
    return;
  }

  captureImage();

  delay(CAPTURE_INTERVAL);
}
