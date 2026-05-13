// -------------------------------------------------------------------------
// Declare essential library for coding
#include <driver/i2s.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "esp_heap_caps.h"
#include "secrets.h"
// -------------------------------------------------------------------------

// -------------------------------------------------------------------------
// Declare the configuration of Servo
#define SERVO_PIN 15
Servo doorServo;

// Declare the configuration for HC-SR04
#define TRIG_PIN 6
#define ECHO_PIN 7
#define DISTANCE_THRESHOLD 10

// Declare the configuration for LEDs
#define Y_LED_PIN 4
#define G_LED_PIN 5
#define G1_LED_PIN 41
#define R_LED_PIN 39
#define R1_LED_PIN 10

#define BUTTON_PIN 9

// Declare the configuration for Buzzer
#define BUZZER_PIN 8

// Declare the configuration for INMP441 
#define I2S_WS_PIN 16
#define I2S_SCK_PIN 17
#define I2S_SD_PIN 18
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

void setup() {
  Serial.begin(115200);
  delay(1000);

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
  pinMode(R_LED_PIN, OUTPUT);
  pinMode(R1_LED_PIN, OUTPUT);
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
    // Người vào tầm ngắm, chuyển sang chế độ CHỜ (Bật đèn Xanh)
    digitalWrite(Y_LED_PIN, LOW);
    digitalWrite(G_LED_PIN, HIGH);
    
    if (digitalRead(BUTTON_PIN) == HIGH) {
      Serial.println("[@] Nút được bấm! Bắt đầu quá trình thu âm...");
      recordAndSendAudio(); // Bắt đầu ghi âm
      
      // Delay để tránh việc vô tình ấn nháy nút nhiều lần liên tục
      delay(1000); 
    }

  } else{
    digitalWrite(Y_LED_PIN, HIGH);
    digitalWrite(G_LED_PIN, LOW);
    digitalWrite(R1_LED_PIN, LOW); // Đảm bảo R1 tắt khi người đi khỏi
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
  digitalWrite(R1_LED_PIN, HIGH);

// 3. Recording directly to RAM (Vòng lặp thu âm linh hoạt)
  // SỬA LOW THÀNH HIGH: Vòng lặp chạy chừng nào nút VẪN ĐANG BẤM (HIGH) VÀ chưa quá 5 giây
  while (digitalRead(BUTTON_PIN) == HIGH && bytesRead < wavDataSize) {
    esp_err_t result = i2s_read(I2S_PORT, &sample, sizeof(sample), &bytesIn, portMAX_DELAY);
    if (result == ESP_OK && bytesIn > 0) {
      int16_t audio_sample = sample >> 14;
      // Copy 2 byte of audio_sample to buffer
      memcpy(dataPtr, &audio_sample, 2);
      dataPtr += 2;
      bytesRead += 2;
    }
  }

  // Tắt đèn R1_LED báo hiệu kết thúc thu âm khi nhả nút
  digitalWrite(R1_LED_PIN, LOW);
  
  Serial.println("[@] Đã nhả nút. Kết thúc thu âm! Đang kết nối API server...");

  // BƯỚC QUAN TRỌNG: Tạo lại Header cho file WAV với kích thước THỰC TẾ vừa thu được
  generateWavHeader(audioBuffer, bytesRead, SAMPLE_RATE);
  
  // Tính lại tổng dung lượng thực tế để gửi đi (Data thực tế + 44 bytes header)
  uint32_t actualTotalSizeToSend = bytesRead + 44;

  // 4. Send all files in RAM via the API
  if(WiFi.status() == WL_CONNECTED){
    
    // --- BẮT ĐẦU ĐOẠN CẦN SỬA ---
    WiFiClientSecure client;
    client.setInsecure(); // Lệnh cực kỳ quan trọng để bỏ qua bước xác minh chứng chỉ SSL của Ngrok
    
    HTTPClient http;
    http.begin(client, serverName);
    // --- KẾT THÚC ĐOẠN CẦN SỬA ---

    http.setTimeout(10000);
    // Declare the type of send data as a WAV file
    http.addHeader("Content-Type", "audio/wav");
    
    // ĐẨY FILE ĐI VỚI DUNG LƯỢNG THỰC TẾ (Thay vì totalSize cứng 5 giây như cũ)
    int httpResponseCode = http.POST(audioBuffer, actualTotalSizeToSend);
    
    if(httpResponseCode > 0){
      Serial.print("[@] Server response (Code ");
      Serial.print(httpResponseCode);
      Serial.println("):");
      
      // Get and print out the result that server returns (ACCEPTED or REJECTED)
      String response = http.getString();
      Serial.println(response);
      
      if(response.indexOf("ACCEPTED") > 0) {
        Serial.println("[@] Welcome home! The door is opening in 5 seconds.");
        
        // --- ĐOẠN THÊM MỚI BẮT ĐẦU ---
        // Nháy đèn xanh G1 và bíp còi 2 lần ngắn báo hiệu mở khóa thành công
        for(int i = 0; i < 2; i++) {
          digitalWrite(BUZZER_PIN, HIGH);
          digitalWrite(G1_LED_PIN, HIGH);
          delay(100); // Kêu ngắn để phân biệt với báo động
          digitalWrite(BUZZER_PIN, LOW);
          digitalWrite(G1_LED_PIN, LOW);
          delay(100);
        }
        
        // Bật giữ đèn G1 sáng trong suốt 5 giây cửa mở
        digitalWrite(G1_LED_PIN, HIGH);
        // --- ĐOẠN THÊM MỚI KẾT THÚC ---

        // Mở cửa
        doorServo.write(90);
        
        // Chờ 5 giây
        delay(5000); 
        
        Serial.println("[@] The door is autonomously closed.");
        
        // Đóng cửa
        doorServo.write(0);
        
        // Tắt đèn xanh G1 sau khi cửa đã đóng
        digitalWrite(G1_LED_PIN, LOW);
        
      } else {
        Serial.println("[@] You're not the person who lives in this house.");
        
        // Kích hoạt cảnh báo: Hú còi và chớp đèn đỏ 5 lần
        for(int i = 0; i < 5; i++) {
          digitalWrite(BUZZER_PIN, HIGH);  // Bật còi
          digitalWrite(R_LED_PIN, HIGH);   // Bật đèn đỏ
          delay(150);
          digitalWrite(BUZZER_PIN, LOW);   // Tắt còi
          digitalWrite(R_LED_PIN, LOW);    // Tắt đèn đỏ
          delay(150);
        }
      }
      
    } else {
      Serial.print("[!] Error: HTTP POST ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("[!] Error: Lost Wifi connection.");
  }

  // 5. Notice: Free up memory after sending to prevent RAM overflow
  free(audioBuffer);
}