#include <WiFi.h>
#include <HTTPClient.h>
#include <FS.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>
#include <JPEGDecoder.h>

TFT_eSPI tft = TFT_eSPI();

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
}

bool downloadJpg(const char* path) {
  HTTPClient http;
  http.begin(jpgUrl);
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.printf("HTTP GET failed, code: %d\n", httpCode);
    return false;
  }

  File file = SPIFFS.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buffer[1024];
  int len = http.getSize();

  while (http.connected() && len > 0) {
    int bytesRead = stream->readBytes(buffer, sizeof(buffer));
    file.write(buffer, bytesRead);
    len -= bytesRead;
  }

  file.close();
  http.end();
  Serial.println("JPG download complete");

  // Check image size
  File test = SPIFFS.open(path);
  if (test) {
    Serial.printf("Image saved with %d bytes\n", test.size());
    test.close();
  } else {
    Serial.println("Failed to verify saved image");
  }
  File checkFile = SPIFFS.open("/images.jpg");
  if (checkFile) {
  uint8_t header[10];
  checkFile.read(header, 10);
  Serial.printf("File header bytes: %02X %02X %02X %02X\n", 
    header[0], header[1], header[2], header[3]);
  checkFile.close();
}

  return true;
}

void jpegRender(int xpos, int ypos) {
  tft.setSwapBytes(true); // Importante para o formato de cor correto
  
  uint16_t *pImg;
  uint32_t width = JpegDec.width;
  uint32_t height = JpegDec.height;
  uint16_t mcuWidth = JpegDec.MCUWidth;
  uint16_t mcuHeight = JpegDec.MCUHeight;
  
  Serial.printf("Image dimensions: %dx%d\n", width, height);
  
  // Calcular as posições x e y corretas
  int x = (xpos == -1) ? ((tft.width() - width) / 2) : xpos;
  int y = (ypos == -1) ? ((tft.height() - height) / 2) : ypos;
  
  Serial.printf("Starting render at base position: %d,%d\n", x, y);
  
  while (JpegDec.read()) {
    pImg = JpegDec.pImage;
    int mcu_x = JpegDec.MCUx * mcuWidth + x;
    int mcu_y = JpegDec.MCUy * mcuHeight + y;
    
    // Log apenas para o primeiro bloco MCU
    if (JpegDec.MCUx == 0 && JpegDec.MCUy == 0) {
      Serial.printf("First block position: %d,%d\n", mcu_x, mcu_y);
    }
    
    // Verificação de limites - extremamente importante!
    if (mcu_x >= 0 && mcu_y >= 0 && 
        mcu_x + mcuWidth <= tft.width() && 
        mcu_y + mcuHeight <= tft.height()) {
      tft.pushImage(mcu_x, mcu_y, mcuWidth, mcuHeight, pImg);
    } else {
      // Log para MCUs fora dos limites
      if (JpegDec.MCUx == 0 || JpegDec.MCUy == 0) {
        Serial.printf("MCU out of bounds at: %d,%d\n", mcu_x, mcu_y);
      }
    }
  }
}
void displayJpeg(const char *filename) {
  Serial.printf("Abrindo arquivo: %s\n", filename);
  File jpegFile = SPIFFS.open(filename);
  if (!jpegFile) {
    Serial.println("Erro ao abrir imagem no SPIFFS");
    return;
  }

  Serial.println("Decodificando imagem...");
  if (JpegDec.decodeFsFile(jpegFile)) {
    Serial.println("Renderizando imagem...");
    tft.setSwapBytes(true);
    jpegRender(-1, -1);
  } else {
    Serial.println("Falha ao decodificar JPEG");
  }

  jpegFile.close();
}


void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32 JPG Downloader and Display");

  
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
   // Teste básico para verificar se o display está funcionando
   tft.drawRect(0, 0, tft.width(), tft.height(), TFT_RED);
   tft.drawLine(0, 0, tft.width(), tft.height(), TFT_GREEN);
   tft.drawLine(tft.width(), 0, 0, tft.height(), TFT_BLUE);
   delay(2000);

  tft.println("Initializing...");

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS init failed");
    tft.println("SPIFFS failed!");
    return;
  }

  tft.println("Connecting to WiFi...");
  connectToWiFi();
  tft.println("WiFi connected!");

  tft.println("Downloading image...");
  if (downloadJpg("/images.jpg")) {
    tft.println("Download OK");
  } else {
    tft.println("Download FAILED!");
    return;
  }

  tft.fillScreen(TFT_BLACK);
}

void loop() {
  Serial.println("Displaying JPG...");
  displayJpeg("/images.jpg");

  delay(5000);
  tft.fillScreen(TFT_BLACK);

  // Optional: re-download if you want live updates
  // downloadJpg("/image.jpg");
}
