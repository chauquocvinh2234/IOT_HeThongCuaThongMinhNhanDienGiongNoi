// -------------------------------------------------------------------------
// Khai báo các thư viện cần thiết
#include <driver/i2s.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "esp_heap_caps.h"
#include "secrets.h"
#include <U8g2lib.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
// -------------------------------------------------------------------------

// -------------------------------------------------------------------------
// Khởi tạo màn hình OLED 1.3" (SH1106) sử dụng I2C phần cứng trên chân 8 và 9
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* SCL=*/ 9, /* SDA=*/ 8);

int currentDisplayState = -1; // Biến trạng thái để chống giật/nháy màn hình

// Khai báo chân cho Servo và thư viện điều khiển Servo
#define SERVO_PIN 13
Servo doorServo;

// Khai báo chân cho HC-SR04 và các đèn LED để tương tác với HC-SR04
#define TRIG_PIN 41 
#define ECHO_PIN 42 
#define DISTANCE_THRESHOLD 20
#define Y_LED_PIN 47 
#define G_LED_PIN 48 

// Khai báo chân cho nút và đèn LED để tương tác với nút
#define R_LED_PIN 17 
#define BUTTON_PIN 18 

// Khai báo chân cho Buzzer và đèn LED để tương tác với Buzzer
#define BUZZER_PIN 5
#define G1_LED_PIN 6
#define R1_LED_PIN 4

// Khai báo chân cho INMP441 và thiết lập thông số cho việc ghi âm giọng nói
#define I2S_WS_PIN 10 
#define I2S_SCK_PIN 11 
#define I2S_SD_PIN 12 
#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 16000
#define RECORD_TIME_SEC 5
const uint32_t wavDataSize = RECORD_TIME_SEC * SAMPLE_RATE * 2;

// Khai báo cấu hình MQTT (Sử dụng Broker công cộng của HiveMQ)
#define MQTT_BROKER "broker.hivemq.com"
#define MQTT_PORT 1883
#define MQTT_TOPIC_CMD "smartdoor/command"   // Topic nhận lệnh mở cửa
#define MQTT_TOPIC_STATUS "smartdoor/status" // Topic gửi trạng thái ESP32

WiFiClient espMqttClient;
PubSubClient mqttClient(espMqttClient);

volatile bool mqttOpenDoorFlag = false;      // Cờ báo có lệnh mở cửa từ MQTT
unsigned long lastMqttReconnectAttempt = 0;  // Thời điểm thử kết nối lại MQTT lần cuối

// -------------------------------------------------------------------------

// Khai báo nguyên mẫu hàm
void initMicrophone();
void recordAndSendAudio();
float getDistance();
void controlSystemLogic(float distance, int distance_threshold);
void generateWavHeader(uint8_t* wav_header, uint32_t wav_size, uint32_t sample_rate);
void updateDisplay(int state, String extra = "");
void mqttCallback(char* topic, byte* payload, unsigned int length);
void reconnectMQTT();
void handleRemoteDoorOpen();

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("[@] Đã vào chế độ Cài đặt WiFi");
  Serial.println(WiFi.softAPIP()); // In ra Local IP của mạch ESP32
  
  // Hiển thị lên OLED tên WiFi cần kết nối
  updateDisplay(4, "Cài WiFi: " + myWiFiManager->getConfigPortalSSID());
  /*
      - myWiFiManager->getConfigPortalSSID(): Trích xuất tên WiFi mà mạch đang phát ra.
  */
}


void setup() {
  Serial.begin(115200);
  delay(1000);

  // Khởi tạo I2C và Màn hình OLED
  Wire.begin(8, 9); // SDA = 8, SCL = 9
  u8g2.begin();
  u8g2.enableUTF8Print(); // Kích hoạt in Tiếng Việt
  updateDisplay(4, "Đang kết nối WiFi...");

  WiFiManager wifiManager;

  // Đăng ký hàm callback để update màn hình OLED khi rớt mạng
  wifiManager.setAPCallback(configModeCallback);

  // Thiết lập thời gian chờ cho mạch ESP32 kết nối lại thiết lập WiFi cũ
  // Nếu sau 10 giây không kết nối WiFi được thì ESP32 sẽ chuyển sang chế độ phát WiFi cấu hình
  // Người dùng có thể truy cập vào IP Local của WiFi được mạch phát ra để cấu hình mạng mới
  wifiManager.setConnectTimeout(10);
  /*
      - Thiết lập thời gian chờ cho cổng cài đặt là 3 phút
      - Nếu sau 3 phút không ai cấu hình, ESP32 sẽ reset để thử lại việc kết nối WiFi
  */
  wifiManager.setConfigPortalTimeout(180);

  // Khởi chạy WiFiManager. 
  // "SmartDoor_Setup" là tên WiFi do ESP32 phát ra nếu không có mạng.
  // Hàm này sẽ block chương trình cho đến khi kết nối thành công hoặc timeout.
  if (!wifiManager.autoConnect("SmartDoor_Setup")) {
    Serial.println("[!] Kết nối thất bại và hết thời gian chờ");
    updateDisplay(4, "Lỗi WiFi! Đang thử lại...");
    delay(3000);
    ESP.restart(); // Khởi động lại mạch
  }

  // Nếu mạch ESP32 kết nối mạng thành công thì sẽ hiển thị dòng này
  Serial.println("\n[@] Kết nối WiFi thành công, địa chỉ IP: ");
  Serial.println(WiFi.localIP());
  
  updateDisplay(4, "WiFi OK!");
  delay(1500);
  updateDisplay(0);
  // ===============================================

  // Thiết lập chân đầu ra cho các thiết bị được kết nối
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(Y_LED_PIN, OUTPUT);
  pinMode(G_LED_PIN, OUTPUT);
  pinMode(G1_LED_PIN, OUTPUT);
  pinMode(R1_LED_PIN, OUTPUT);
  pinMode(R_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);

  // Khởi tạo Servo
  doorServo.attach(SERVO_PIN);
  doorServo.write(0); // Góc bằng 0 <=> Cửa đang khóa

  // Khởi tạo INMP441
  initMicrophone();

  // Khởi tạo kết nối MQTT tới Broker
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(256);
  reconnectMQTT();
}

void loop() {
  // 1. Duy trì kết nối MQTT (tự động reconnect mỗi 5 giây nếu mất kết nối)
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt > 5000) {
      lastMqttReconnectAttempt = now;
      reconnectMQTT();
    }
  }
  mqttClient.loop(); // Xử lý các message MQTT đến

  // 2. Xử lý lệnh mở cửa từ MQTT (nếu có)
  if (mqttOpenDoorFlag) {
    mqttOpenDoorFlag = false;
    handleRemoteDoorOpen();
  }

  // 3. Lấy khoảng cách được trả về từ HC-SR04
  float distance = getDistance();
  
  // 4. Điều khiển logic của hệ thống nhận diện
  controlSystemLogic(distance, DISTANCE_THRESHOLD);

  delay(100);
}

// Hàm xử lý chức năng của toàn bộ hệ thống
void controlSystemLogic(float distance, int distance_threshold){
  /*
    - Dưới đây là mô tả luồng xử lý cho toàn bộ hệ thống:
      + Nếu khoảng cách trả về thuộc [0, 10] cm:
        * Hệ thống sẽ sáng đèn màu xanh lá (G_LED_PIN), tắt đèn màu vàng (Y_LED_PIN) và màn hình OLED sẽ hiển thị là "Phát hiện người! Nhấn giữ nút để nói"
        * Người dùng nhấn giữ nút và nói vào mic (INMP441), khi người dùng thả nút ra thì đoạn giọng nói sẽ được lưu lại và gửi lên máy chủ để xử lý. Sau đó
          hệ thống dừng 1 giây để ổn định và tiếp tục lặp lại luồng xử lý của hệ thống.  
      + Nếu khoảng cách trả về thuộc [10.01, maxValue_Of_HC-SR04]:
        * Hệ thống sẽ sáng đèn màu vàng (Y_LED_PIN), tắt đèn màu xanh lá (G_LED_PIN) và sẽ hiển thị lên màn hình OLED
          là "Hệ thống khóa"

    - Giải thích:
      + updateDisplay(0): Màn hình OLED sẽ hiển thị "Hệ thống khóa"
      + updateDisplay(1): Màn hình OLED sẽ hiển thị "Phát hiện người! Nhấn giữ nút để nói"
  */
  if (distance > 0 && distance <= distance_threshold){
    digitalWrite(Y_LED_PIN, LOW);
    digitalWrite(G_LED_PIN, HIGH);
    
    updateDisplay(1);
    
    if (digitalRead(BUTTON_PIN) == HIGH) {
      Serial.println("[@] Nút được bấm! Bắt đầu quá trình thu âm...");
      recordAndSendAudio(); 
      delay(1000);
    }
  } else{
    digitalWrite(Y_LED_PIN, HIGH);
    digitalWrite(G_LED_PIN, LOW);
    digitalWrite(R_LED_PIN, LOW);
    updateDisplay(0);
  }
}

// Hàm để lấy khoảng cách được trả về của HC-SR04
float getDistance(){
  /*
    - Luồng xử lý của chức năng này như sau:
      1. Phát sóng (Trigger): Kéo chân TRIG lên mức HIGH trong đúng 10 micro-giây (µs) để kích hoạt module phát ra sóng siêu âm.
      2. Nhận sóng (Echo): Dùng hàm pulseIn() để đo thời gian chân ECHO duy trì ở mức HIGH. Đây chính là tổng thời gian sóng âm bay đi và dội lại khi gặp vật cản.
      3. Tính toán: Khoảng cách = (Thời gian * Vận tốc âm thanh) / 2.
         (Vận tốc âm thanh ~ 340m/s, tương đương 0.034 cm/µs. Chia 2 vì sóng phải đi cả 2 chiều: từ cảm biến đến vật cản rồi dội ngược lại).
      4. Trả kết quả: In ra cổng Serial để theo dõi và trả về giá trị khoảng cách (cm).
  */

  long duration;
  float distance;

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duration = pulseIn(ECHO_PIN, HIGH);

  distance = duration * 0.034/2;

  Serial.print("Khoảng cách: ");
  Serial.print(distance);
  Serial.println(" cm");

  return distance;
}

// Hàm khởi tạo của thiết bị INMP441
void initMicrophone() {
  /*
    - Luồng xử lý của chức năng này như sau:
      1. Cấu hình thông số I2S (i2s_config): Thiết lập ESP32 làm thiết bị chủ (Master) để nhận dữ liệu (RX). 
         Chỉ định các thông số thu âm như: tần số lấy mẫu (Sample Rate), độ phân giải (32-bit), kênh âm thanh (trái/mono) 
         và thiết lập bộ nhớ đệm DMA để xử lý luồng âm thanh liên tục mà không làm nghẽn CPU.
      2. Cấu hình phần cứng (pin_config): Ánh xạ các chân GPIO của mạch ESP32 tương ứng với các chân giao tiếp của micro INMP441 (SCK/BCLK, WS/LRC, SD/Data).
      3. Khởi chạy: Nạp các cấu hình trên vào driver I2S, áp dụng phần cứng và bật I2S lên để sẵn sàng thu âm giọng nói.
  */
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
  
  Serial.println("[@] Microphone INMP441 đã sẵn sàng");
}


// Hàm tạo Header chuẩn 44-byte cho file âm thanh định dạng WAV
void generateWavHeader(uint8_t* wav_header, uint32_t wav_size, uint32_t sample_rate) {
  /*
    - Luồng xử lý của chức năng này như sau:
      1. Mục đích: Dữ liệu âm thanh thô từ INMP441 chỉ là các dải số. Để máy tính hay Backend Python hiểu được đây là file âm thanh, 
         nó cần 44 byte thông tin "mô tả" ở ngay đầu file.
      2. Tính toán kích thước: Tính tổng kích thước file và tốc độ truyền byte (byteRate = sample_rate * 2 byte/mẫu cho chuẩn 16-bit Mono).
      3. Tạo mảng Header: Lắp ghép một mảng đúng 44 byte chứa các cờ nhận diện chuẩn như "RIFF", "WAVE", "fmt ", và "data". 
         (Lưu ý: Các phép toán dịch bit `>> 8`, `>> 16`, `>> 24` và `& 0xff` có nhiệm vụ cắt một số nguyên lớn 32-bit thành 4 phần nhỏ 8-bit và ghi ngược từ đuôi lên đầu theo chuẩn Little-Endian bắt buộc của file WAV).
      4. Gắn vào bộ nhớ: Dùng hàm memcpy() để chép chính xác 44 byte vừa tạo này vào phần đầu tiên của bộ nhớ đệm (buffer) chứa file ghi âm.
  */
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

// Hàm để ghi âm lại giọng nói và gửi đoạn giọng nói đó về máy chủ
void recordAndSendAudio() {
  uint32_t totalSize = wavDataSize + 44;
  
  // 1. Cấp bộ nhớ PSRAM để lưu trữ file ghi âm giọng nói
  uint8_t *audioBuffer = (uint8_t *)heap_caps_malloc(totalSize, MALLOC_CAP_SPIRAM);
  if(audioBuffer == NULL) {
    Serial.println("[!] LỖI: Không đủ dung lượng bộ nhớ PSRAM.");
    return;
  }

  // 2. Thêm 44 byte header vào đầu của file ghi âm giọng nói
  generateWavHeader(audioBuffer, wavDataSize, SAMPLE_RATE);

  i2s_stop(I2S_PORT);
  i2s_start(I2S_PORT);

  int32_t sample = 0;
  size_t bytesIn = 0;
  uint32_t bytesRead = 0;

  // Con trỏ ghi dữ liệu, bắt đầu ghi từ vị trí byte thứ 44
  uint8_t *dataPtr = audioBuffer + 44; 

  Serial.println("[@] Hãy giữ nút để nói (Tối đa 5 giây)...");
  
  // 3. Ghi âm giọng nói khi vào đúng tầm của HC-SR04 và nhấn giữ nút
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

  // 4. Sau khi thu âm xong, xử lý file thu âm được lưu trên bộ nhớ đệm và gửi file đó lên máy chủ để xử lý
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
      
      // Bóc tách JSON để lấy tên của người dùng
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, response);
      
      if (!error) {
        String message = doc["message"]; // Lấy trường "message"
        String matched_user = doc["matched_user"]; // Lấy trường "matched_user"

        if(message == "ACCEPTED") {
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
    u8g2.drawUTF8(5, 45, "Nhấn giữ nút để nói");
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
    } 
    // TỰ ĐỘNG CHIA DÒNG KHI ĐANG KẾT NỐI WIFI
    else if (extra == "Đang kết nối WiFi...") {
      u8g2.drawUTF8(10, 25, "Đang kết nối");
      u8g2.drawUTF8(35, 50, "WiFi..."); // Đẩy chữ WiFi ra giữa màn hình
    }
    // TỰ ĐỘNG CHIA DÒNG KHI LỖI WIFI
    else if (extra == "Lỗi WiFi! Đang thử lại...") {
      u8g2.drawUTF8(20, 25, "Lỗi WiFi!");
      u8g2.drawUTF8(5, 50, "Đang thử lại...");
    }
    else {
      // Các thông báo ngắn khác (như Cảnh báo hoặc WiFi OK) thì giữ nguyên dòng giữa
      u8g2.setCursor(0, 35);
      u8g2.print(extra); 
    }
  }
  u8g2.sendBuffer();
}

// =========================================================================
// CÁC HÀM MQTT (Thay thế cho cơ chế Polling cũ)
// =========================================================================

// Hàm callback được gọi tự động khi có message MQTT đến
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  /*
    - Luồng xử lý:
      1. Đọc nội dung message từ payload (mảng byte) và chuyển sang String.
      2. Kiểm tra nếu topic là "smartdoor/command" và nội dung là "open"
         thì bật cờ mqttOpenDoorFlag = true.
      3. Cờ này sẽ được kiểm tra trong hàm loop() để thực thi mở cửa.
         (Không mở cửa trực tiếp trong callback vì delay() dài sẽ gây mất kết nối MQTT)
  */
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.println("[@] MQTT nhận message: " + message + " | Topic: " + String(topic));
  
  if (String(topic) == MQTT_TOPIC_CMD && message == "open") {
    mqttOpenDoorFlag = true;
  }
}

// Hàm kết nối (hoặc kết nối lại) tới MQTT Broker
void reconnectMQTT() {
  /*
    - Luồng xử lý:
      1. Tạo Client ID ngẫu nhiên để tránh xung đột khi nhiều ESP32 cùng kết nối.
      2. Thiết lập Last Will & Testament (LWT): Nếu ESP32 mất kết nối đột ngột,
         Broker sẽ tự động publish "offline" lên topic smartdoor/status.
      3. Sau khi kết nối thành công, subscribe topic smartdoor/command để nhận lệnh
         và publish "online" lên smartdoor/status để Backend biết ESP32 đang hoạt động.
  */
  Serial.print("[@] Đang kết nối MQTT Broker...");
  
  String clientId = "ESP32SmartDoor-" + String(random(0xffff), HEX);
  
  // Tham số connect: clientId, username, password, willTopic, willQoS, willRetain, willMessage
  if (mqttClient.connect(clientId.c_str(), NULL, NULL, MQTT_TOPIC_STATUS, 1, true, "offline")) {
    Serial.println(" Thành công!");
    mqttClient.subscribe(MQTT_TOPIC_CMD, 1); // Subscribe với QoS 1 (đảm bảo nhận ít nhất 1 lần)
    mqttClient.publish(MQTT_TOPIC_STATUS, "online", true); // Retained message
    Serial.println("[@] Đã subscribe topic: " + String(MQTT_TOPIC_CMD));
  } else {
    Serial.print(" Thất bại! Mã lỗi: ");
    Serial.println(mqttClient.state());
  }
}

// Hàm xử lý mở cửa khi nhận lệnh từ MQTT (thay thế logic cũ trong checkRemoteCommand)
void handleRemoteDoorOpen() {
  Serial.println("[@] NHẬN LỆNH MỞ CỬA TỪ ĐIỆN THOẠI (MQTT)!");
  updateDisplay(4, "Mở cửa từ xa...");
  
  // Bíp còi và nháy đèn xanh 2 lần
  for(int i = 0; i < 2; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(G1_LED_PIN, HIGH);
    delay(100); 
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(G1_LED_PIN, LOW);
    delay(100);
  }
  
  digitalWrite(G1_LED_PIN, HIGH);
  doorServo.write(90); // Mở cửa
  
  // Thông báo cho Backend biết cửa đã mở thật
  mqttClient.publish(MQTT_TOPIC_STATUS, "opened", true);
  
  delay(5000); // Giữ cửa mở 5 giây
  
  doorServo.write(0); // Đóng cửa
  digitalWrite(G1_LED_PIN, LOW);
  
  // Thông báo cho Backend biết cửa đã đóng
  mqttClient.publish(MQTT_TOPIC_STATUS, "closed", true);
  
  updateDisplay(0); // Trả màn hình về trạng thái chờ
}