// -------------------------------------------------------------------------
// Declare essential library for coding
#include <driver/i2s.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "esp_heap_caps.h"
#include "secrets.h"
#include <U8g2lib.h>
#include <Wire.h>
#include <ArduinoJson.h>
// -------------------------------------------------------------------------

// -------------------------------------------------------------------------
// Khởi tạo màn hình OLED 1.3" (SH1106) sử dụng I2C phần cứng trên chân 8 và 9
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* SCL=*/ 9, /* SDA=*/ 8);

int currentDisplayState = -1; // Biến trạng thái để chống giật/nháy màn hình

// Declare the configuration of Servo
#define SERVO_PIN 13
Servo doorServo;

// Declare the configuration for HC-SR04
#define TRIG_PIN 41 
#define ECHO_PIN 42 
#define DISTANCE_THRESHOLD 10
#define Y_LED_PIN 47 
#define G_LED_PIN 48 

// Declare the configuration for Button
#define R_LED_PIN 17 
#define BUTTON_PIN 18 

// Declare the configuration for Buzzer
#define BUZZER_PIN 5
#define G1_LED_PIN 6
#define R1_LED_PIN 4

// Declare the configuration for INMP441 
#define I2S_WS_PIN 10 
#define I2S_SCK_PIN 11 
#define I2S_SD_PIN 12 
#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 16000

// Declare the configuration for recording
#define RECORD_TIME_SEC 5
const uint32_t wavDataSize = RECORD_TIME_SEC * SAMPLE_RATE * 2;

// -------------------------------------------------------------------------

// Declare function prototypes
void initMicrophone();
void recordAndSendAudio();
float getDistance();
void controlSystemLogic(float distance, int distance_threshold);
void generateWavHeader(uint8_t* wav_header, uint32_t wav_size, uint32_t sample_rate);
void updateDisplay(int state, String extra = "");

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Khởi tạo I2C và Màn hình OLED
  Wire.begin(8, 9); // SDA = 8, SCL = 9
  u8g2.begin();
  u8g2.enableUTF8Print(); // Kích hoạt in Tiếng Việt
  updateDisplay(0); // Hiển thị "Hệ thống khóa" ban đầu

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("[@] Connecting to WiFi");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[@] WiFi Connected! IP: ");
  Serial.println(WiFi.localIP());

  // Configure pin mode for devices
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(Y_LED_PIN, OUTPUT);
  pinMode(G_LED_PIN, OUTPUT);
  pinMode(G1_LED_PIN, OUTPUT);
  pinMode(R1_LED_PIN, OUTPUT);
  pinMode(R_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);

  // Initialize Servo
  doorServo.attach(SERVO_PIN);
  doorServo.write(0); //Angle 0: Door locked

  // Initialize INMP441
  initMicrophone();
}

void loop() {
  // Get distance from HC-SR04
  float distance = getDistance();
  // Controlling the LED logics based on the returned distance
  controlSystemLogic(distance, DISTANCE_THRESHOLD);

  delay(100);
}

void controlSystemLogic(float distance, int distance_threshold){
  if (distance > 0 && distance < distance_threshold){
    digitalWrite(Y_LED_PIN, LOW);
    digitalWrite(G_LED_PIN, HIGH);
    
    updateDisplay(1); // Màn hình: "Phát hiện người! Nhấn giữ để nói"
    
    if (digitalRead(BUTTON_PIN) == HIGH) {
      Serial.println("[@] Nút được bấm! Bắt đầu quá trình thu âm...");
      recordAndSendAudio(); 
      delay(1000);
    }
  } else{
    digitalWrite(Y_LED_PIN, HIGH);
    digitalWrite(G_LED_PIN, LOW);
    digitalWrite(R_LED_PIN, LOW);
    updateDisplay(0); // Màn hình: "Hệ thống khóa"
  }
}

// The function to get distance from HC-SR04
float getDistance(){
  long duration;
  float distance;
  // 1. Sending ultrasonic signal (with HIGH signal in 10 microseconds)
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // 2. Read the response time of the ECHO pin
  duration = pulseIn(ECHO_PIN, HIGH);

  // 3. Calculate the distance (Speed of sound ~ 340m/s --> 0.034cm/microsecond)
  // Divide by 2 because the sound waves must travel to the obstacle and then bounce back
  distance = duration * 0.034/2;

  // 4. Print out the result to the serial screen
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  return distance;
}

// The function to initialize INMP441
void initMicrophone() {
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = i2s_bits_per_sample_t(32),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = -1,
    .data_in_num = I2S_SD_PIN
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_start(I2S_PORT);
  
  Serial.println("[@] Microphone INMP441 is ready.");
}

void generateWavHeader(uint8_t* wav_header, uint32_t wav_size, uint32_t sample_rate) {
  uint32_t fileSize = wav_size + 36;
  uint32_t byteRate = sample_rate * 2;
  const uint8_t setWavHeader[] = {
    'R', 'I', 'F', 'F',
    (uint8_t)(fileSize & 0xff), (uint8_t)((fileSize >> 8) & 0xff), (uint8_t)((fileSize >> 16) & 0xff), (uint8_t)((fileSize >> 24) & 0xff),
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x10, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x01, 0x00,
    (uint8_t)(sample_rate & 0xff), (uint8_t)((sample_rate >> 8) & 0xff), (uint8_t)((sample_rate >> 16) & 0xff), (uint8_t)((sample_rate >> 24) & 0xff),
    (uint8_t)(byteRate & 0xff), (uint8_t)((byteRate >> 8) & 0xff), (uint8_t)((byteRate >> 16) & 0xff), (uint8_t)((byteRate >> 24) & 0xff),
    0x02, 0x00, 0x10, 0x00,
    'd', 'a', 't', 'a',
    (uint8_t)(wav_size & 0xff), (uint8_t)((wav_size >> 8) & 0xff), (uint8_t)((wav_size >> 16) & 0xff), (uint8_t)((wav_size >> 24) & 0xff)
  };
  memcpy(wav_header, setWavHeader, 44);
}

// The function to record audio
void recordAndSendAudio() {
  uint32_t totalSize = wavDataSize + 44; // Total size: Audio data + 44 byte header
  
  // 1. Allocate memory from PSRAM to store the file
  uint8_t *audioBuffer = (uint8_t *)heap_caps_malloc(totalSize, MALLOC_CAP_SPIRAM);
  if(audioBuffer == NULL) {
    Serial.println("[!] Error: Not enough memory PSRAM.");
    return;
  }

  // 2. Insert a 44-byte header at the beginning of the buffer
  generateWavHeader(audioBuffer, wavDataSize, SAMPLE_RATE);

  i2s_stop(I2S_PORT);
  i2s_start(I2S_PORT);

  int32_t sample = 0;
  size_t bytesIn = 0;
  uint32_t bytesRead = 0;
  
  // Data write pointer, starting at byte position 44
  uint8_t *dataPtr = audioBuffer + 44; 

  Serial.println("[@] Hãy giữ nút để nói (Tối đa 5 giây)...");
  
  // Bật đèn R1_LED báo hiệu đang thu âm
  digitalWrite(R_LED_PIN, HIGH);

  uint32_t startTime = millis();
  uint32_t lastElapsed = 255;

  while (digitalRead(BUTTON_PIN) == HIGH && bytesRead < wavDataSize) {
    // Tính số giây đã trôi qua để update OLED
    uint32_t elapsed = (millis() - startTime) / 1000;
    if (elapsed != lastElapsed) { // Chỉ render màn hình khi giây thay đổi để không gây giật lag Audio
      updateDisplay(2, String(elapsed));
      lastElapsed = elapsed;
    }

    esp_err_t result = i2s_read(I2S_PORT, &sample, sizeof(sample), &bytesIn, portMAX_DELAY);
    if (result == ESP_OK && bytesIn > 0) {
      int16_t audio_sample = sample >> 14;
      memcpy(dataPtr, &audio_sample, 2);
      dataPtr += 2;
      bytesRead += 2;
    }
  }

  digitalWrite(R_LED_PIN, LOW);
  updateDisplay(3); // Màn hình: "Đang xử lý AI..."

  generateWavHeader(audioBuffer, bytesRead, SAMPLE_RATE);
  uint32_t actualTotalSizeToSend = bytesRead + 44;

  if(WiFi.status() == WL_CONNECTED){
    WiFiClientSecure client;
    client.setInsecure(); 
    HTTPClient http;
    http.begin(client, serverName);
    http.setTimeout(20000); // Tăng timeout lên 20s
    http.addHeader("Content-Type", "audio/wav");
    
    int httpResponseCode = http.POST(audioBuffer, actualTotalSizeToSend);
    if(httpResponseCode > 0){
      String response = http.getString();
      Serial.println(response);
      
      // BÓC TÁCH JSON ĐỂ LẤY TÊN NGƯỜI DÙNG
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, response);
      
      if (!error) {
        String message = doc["message"]; // Lấy trường "message"
        String matched_user = doc["matched_user"]; // Lấy trường "matched_user"

        if(message == "ACCEPTED") {
          // HIỂN THỊ CHÀO MỪNG LÊN OLED
          updateDisplay(4, "Chào " + matched_user + "!");
          
          for(int i = 0; i < 2; i++) {
            digitalWrite(BUZZER_PIN, HIGH);
            digitalWrite(G1_LED_PIN, HIGH);
            delay(100); 
            digitalWrite(BUZZER_PIN, LOW);
            digitalWrite(G1_LED_PIN, LOW);
            delay(100);
          }
          digitalWrite(G1_LED_PIN, HIGH);
          doorServo.write(90);
          delay(5000); 
          doorServo.write(0);
          digitalWrite(G1_LED_PIN, LOW);
          
        } else {
          // HIỂN THỊ CẢNH BÁO LÊN OLED
          updateDisplay(4, "CẢNH BÁO! Kẻ lạ");
          
          for(int i = 0; i < 5; i++) {
            digitalWrite(BUZZER_PIN, HIGH);
            digitalWrite(R1_LED_PIN, HIGH);
            delay(150);
            digitalWrite(BUZZER_PIN, LOW);
            digitalWrite(R1_LED_PIN, LOW);
            delay(150);
          }
        }
      }
    } else {
      updateDisplay(4, "Lỗi mạng: " + String(httpResponseCode));
      delay(3000);
    }
    http.end();
  }
  free(audioBuffer);
}

void updateDisplay(int state, String extra) {
  // Tránh việc vẽ lại màn hình liên tục nếu trạng thái không đổi (trừ khi đang đếm giây)
  if (currentDisplayState == state && state != 2) return;
  currentDisplayState = state;

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_unifont_t_vietnamese1); // Font hỗ trợ Tiếng Việt
  
  if (state == 0) {
    u8g2.drawUTF8(15, 35, "Hệ thống khóa");
  } else if (state == 1) {
    u8g2.drawUTF8(5, 25, "Phát hiện người!");
    u8g2.drawUTF8(5, 45, "Nhấn giữ để nói");
  } else if (state == 2) {
    u8g2.drawUTF8(10, 25, "Đang thu âm...");
    u8g2.setCursor(50, 50);
    u8g2.print(extra); 
    u8g2.print(" s");
  } else if (state == 3) {
    u8g2.drawUTF8(15, 35, "Đang xử lý AI...");
  } else if (state == 4) {
    // TỰ ĐỘNG CHIA DÒNG NẾU LÀ LỜI CHÀO
    if (extra.indexOf("Chào") >= 0) {
      u8g2.drawUTF8(0, 25, "Chào mừng:");
      
      // Lấy phần tên phía sau chữ "Chào "
      int spacePos = extra.indexOf(" ");
      String name = extra.substring(spacePos + 1);
      
      u8g2.setCursor(0, 50);
      u8g2.print(name); // In tên ở dòng dưới (tọa độ y=50)
    } else {
      // Các thông báo ngắn khác (như Cảnh báo) thì giữ nguyên dòng giữa
      u8g2.setCursor(0, 35);
      u8g2.print(extra); 
    }
  }
  u8g2.sendBuffer();
}