
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ==========================================
// ESTACION TUI BILINGUE
// ESP32 DEVKIT V1 - CONTROLADOR MAESTRO
// ==========================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_ADDRESS 0x3C

// Pines OLED
#define OLED_SDA 21
#define OLED_SCL 22

// UART2
#define UART_RX 16
#define UART_TX 17

#define UART_BAUD 115200

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

// ==========================================
// VARIABLES
// ==========================================

char rxBuffer[128];
size_t rxIndex = 0;

bool oledReady = false;
bool everReceived = false;
bool linkLost = false;

unsigned long lastMessageTime = 0;

// ==========================================
// MOSTRAR ESTADO EN OLED
// ==========================================

void showScreen(
  const char *title,
  const char *line1,
  const char *line2
) {

  if (!oledReady) return;

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);

  display.setTextSize(1);
  display.setCursor(0, 0);

  display.println("ESTACION TUI BILINGUE");

  display.drawLine(
    0, 12,
    127, 12,
    SSD1306_WHITE
  );

  display.setTextSize(2);
  display.setCursor(0, 20);

  display.println(title);

  display.setTextSize(1);

  display.setCursor(0, 45);
  display.println(line1);

  display.setCursor(0, 56);
  display.println(line2);

  display.display();
}

// ==========================================
// PROCESAR MENSAJES
// ==========================================

void processMessage(char *message) {

  Serial.print("UART RECIBIDO: ");
  Serial.println(message);

  everReceived = true;
  linkLost = false;

  lastMessageTime = millis();

  if (strcmp(message, "CAM|BOOT") == 0) {

    showScreen(
      "INICIANDO",
      "ESP32-CAM detectado",
      "Iniciando GC2145..."
    );
  }

  else if (
    strcmp(
      message,
      "CAM|INIT_OK|GC2145|RGB565"
    ) == 0
  ) {

    showScreen(
      "SENSOR OK",
      "GC2145 detectado",
      "Formato RGB565"
    );
  }

  else if (
    strcmp(message, "CAM|INIT_ERROR") == 0
  ) {

    showScreen(
      "ERROR CAM",
      "Inicializacion fallida",
      "Revisar sensor"
    );
  }

  else if (
    strcmp(message, "CAM|PSRAM_WARNING") == 0
  ) {

    showScreen(
      "AVISO PSRAM",
      "Memoria no detectada",
      "Revisar configuracion"
    );
  }

  else if (
    strcmp(message, "CAM|FRAME_ERROR") == 0
  ) {

    showScreen(
      "ERROR",
      "Fallo de captura",
      "Fotograma nulo"
    );
  }

  else if (
    strcmp(message, "CAM|FRAME_BAD") == 0
  ) {

    showScreen(
      "ERROR",
      "Imagen incorrecta",
      "Revisar formato"
    );
  }

  else if (
    strncmp(message, "CAM|FRAME_OK|", 13) == 0
  ) {

    unsigned long count = 0;
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned long bytes = 0;

    int fields = sscanf(
      message,
      "CAM|FRAME_OK|%lu|%u|%u|%lu",
      &count,
      &width,
      &height,
      &bytes
    );

    if (fields == 4) {

      char line1[24];
      char line2[24];

      snprintf(
        line1,
        sizeof(line1),
        "GC2145 RGB565"
      );

      snprintf(
        line2,
        sizeof(line2),
        "%ux%u %lu B",
        width,
        height,
        bytes
      );

      showScreen(
        "CAMARA OK",
        line1,
        line2
      );

      Serial.printf(
        "FOTOGRAMA %lu VALIDADO\n",
        count
      );

      Serial.printf(
        "Resolucion: %ux%u\n",
        width,
        height
      );

      Serial.printf(
        "Bytes: %lu\n",
        bytes
      );
    }
  }

  else if (
    strcmp(message, "CAM|SENSOR_UNKNOWN") == 0
  ) {

    showScreen(
      "SENSOR?",
      "PID inesperado",
      "Revisar identificacion"
    );
  }
}

// ==========================================
// LEER UART
// ==========================================

void readUART() {

  while (Serial2.available()) {

    char received = Serial2.read();

    if (received == '\n') {

      rxBuffer[rxIndex] = '\0';

      if (rxIndex > 0) {
        processMessage(rxBuffer);
      }

      rxIndex = 0;
    }

    else if (received != '\r') {

      if (rxIndex < sizeof(rxBuffer) - 1) {

        rxBuffer[rxIndex++] = received;

      } else {

        // Descartar mensaje desbordado
        rxIndex = 0;
      }
    }
  }
}

// ==========================================
// SETUP
// ==========================================

void setup() {

  Serial.begin(115200);

  // UART2 del DevKit
  Serial2.begin(
    UART_BAUD,
    SERIAL_8N1,
    UART_RX,
    UART_TX
  );

  // I2C OLED
  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );

  // Inicializar OLED
  oledReady = display.begin(
    SSD1306_SWITCHCAPVCC,
    OLED_ADDRESS
  );

  if (oledReady) {

    Serial.println("OLED INICIALIZADA");

    showScreen(
      "ESPERANDO",
      "ESP32-CAM",
      "UART 115200"
    );

  } else {

    Serial.println("ERROR OLED");
  }

  Serial.println("DEVKIT LISTO");
}

// ==========================================
// LOOP
// ==========================================

void loop() {

  readUART();

  // Nunca recibimos mensajes
  if (
    !everReceived &&
    !linkLost &&
    millis() > 7000
  ) {

    showScreen(
      "SIN CAMARA",
      "GPIO14 hacia GPIO16",
      "Revisar 5V y GND"
    );

    linkLost = true;
  }

  // Perdida de comunicacion
  if (
    everReceived &&
    !linkLost &&
    millis() - lastMessageTime > 7000
  ) {

    showScreen(
      "SIN SENAL",
      "No llegan mensajes",
      "Revisar UART y GND"
    );

    linkLost = true;
  }
}

