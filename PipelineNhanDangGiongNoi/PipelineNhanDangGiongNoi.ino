// -------------------------------------------------------------------------
// Declare essential library for coding
#include <driver/i2s.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_heap_caps.h"
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

// Declare the configuration for INMP441 
#define I2S_WS_PIN 16
#define I2S_SCK_PIN 17
#define I2S_SD_PIN 18
#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 16000

// Declare the configuration for recording
#define RECORD_TIME_SEC 5
const uint32_t wavDataSize = RECORD_TIME_SEC * SAMPLE_RATE * 2;

// Declare Wi-fi configuration
const char* ssid = "vinhplaykennen";
const char* password = "Chauvinh22032004@Aa";

// Declare IP of the server that's running
const char* serverName = "https://broken-unrigged-scolding.ngrok-free.dev/verify";
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
    // Obstacle detected, turn on GREEN, turn off RED
    digitalWrite(Y_LED_PIN, LOW);
    digitalWrite(G_LED_PIN, HIGH);
    
    Serial.println("[@] Recording your voice...");
    recordAndSendAudio(); // Start recording process
    
    // Add a short delay after recording to prevent the 
    // recording circuit from running continuously if the person remains standing there
    Serial.println("[@] Recording finished. Waiting for next trigger...");
    delay(2000); 

  } else{
    digitalWrite(Y_LED_PIN, HIGH);
    digitalWrite(G_LED_PIN, LOW);
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

  Serial.println("[@] Recording (5 seconds)...");
  
  // 3. Recording directly to RAM
  while (bytesRead < wavDataSize) {
    esp_err_t result = i2s_read(I2S_PORT, &sample, sizeof(sample), &bytesIn, portMAX_DELAY);
    if (result == ESP_OK && bytesIn > 0) {
      // 1. Dịch bit để lấy tín hiệu gốc 16-bit
      int32_t raw_sample = sample >> 14; 
      
      // 2. Nhân với hệ số khuếch đại (Đổi số 5 thành mức bạn muốn. VD: 3, 5, 8...)
      float gain = 5.0; 
      int32_t amplified_sample = raw_sample * gain;

      // 3. Khóa giới hạn (Clamp) để chống vỡ tiếng (chống tràn số nguyên 16-bit)
      if (amplified_sample > 32767) amplified_sample = 32767;
      if (amplified_sample < -32768) amplified_sample = -32768;

      // 4. Ép kiểu về 16-bit và đưa vào buffer như cũ
      int16_t audio_sample = (int16_t)amplified_sample;
      
      memcpy(dataPtr, &audio_sample, 2);
      dataPtr += 2;
      bytesRead += 2;
    }
  }
  
  Serial.println("[@] Audio successfully recorded! Connecting to API server...");

  // 4. Send all files in RAM via the API
  if(WiFi.status() == WL_CONNECTED){
    WiFiClient client;
    HTTPClient http;
    http.begin(client, serverName);
    
    // Declare the type of send data as a WAV file
    http.addHeader("Content-Type", "audio/wav");

    // Execute a POST request to send the buffer directly
    int httpResponseCode = http.POST(audioBuffer, totalSize);
    
    if(httpResponseCode > 0){
      Serial.print("[@] Server response (Code ");
      Serial.print(httpResponseCode);
      Serial.println("):");
      
      // Get and print out the result that server returns (ACCEPTED or REJECTED)
      String response = http.getString();
      Serial.println(response);
      
      if(response.indexOf("ACCEPTED") > 0) {
        Serial.println("[@] Welcome home! The door is opening in 5 seconds.");
        
        doorServo.write(90);
        
        delay(5000); 
        
        Serial.println("[@] The door is autonomously closed.");
        doorServo.write(0);
        
      } else {
        Serial.println("[@] You're not the person who lives in this house.");
        // Bạn có thể cho chớp nháy đèn LED đỏ ở đây để cảnh báo thêm
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