#include <driver/i2s.h>

// Định nghĩa I2S Pins cho ESP32-S3
#define I2S_WS 4
#define I2S_SCK 5
#define I2S_SD 6
#define I2S_PORT I2S_NUM_0

#define SAMPLE_RATE 16000
#define RECORD_TIME 5 // Ghi âm 5 giây mỗi lần nhận lệnh

void i2sInit() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, 
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);
}

void setup() {
  // BẮT BUỘC dùng tốc độ cao để không bị nghẽn dữ liệu âm thanh
  Serial.begin(921600); 
  i2sInit();
}

void loop() {
  // Đợi lệnh từ Python
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'R') {
      recordAndSend(RECORD_TIME);
    }
  }
}

void recordAndSend(int seconds) {
  size_t bytes_read;
  int32_t i2s_read_buff[512]; 
  int16_t serial_write_buff[512]; 

  int data_size = seconds * SAMPLE_RATE * 2; // Tổng số byte cần gửi (16-bit = 2 bytes)
  int bytes_written = 0;

  // LƯU Ý: Tuyệt đối không dùng Serial.print() hoặc Serial.println() trong vòng lặp này
  // vì các chuỗi văn bản (log) sẽ bị lẫn vào dữ liệu âm thanh làm hỏng file.
  
  while (bytes_written < data_size) {
    i2s_read(I2S_PORT, (void*)i2s_read_buff, sizeof(i2s_read_buff), &bytes_read, portMAX_DELAY);
    
    int samples_read = bytes_read / 4; 
    for (int i = 0; i < samples_read; i++) {
      serial_write_buff[i] = i2s_read_buff[i] >> 14; // Dịch bit để chuyển 32-bit thành 16-bit
    }

    // Đẩy trực tiếp mảng dữ liệu thô (raw bytes) ra cáp USB
    Serial.write((uint8_t*)serial_write_buff, samples_read * 2);
    bytes_written += (samples_read * 2);
  }
}