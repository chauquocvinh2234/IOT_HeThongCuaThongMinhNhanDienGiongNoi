import serial
import wave
import time

# --- CẤU HÌNH ---
# THAY ĐỔI CỔNG COM PHÙ HỢP VỚI MÁY BẠN (VD: 'COM3' trên Windows, '/dev/ttyUSB0' trên Mac/Linux)
SERIAL_PORT = 'COM6' 
BAUD_RATE = 921600

RECORD_SECONDS = 5
SAMPLE_RATE = 16000
CHANNELS = 1
SAMPWIDTH = 2 # 16-bit tương đương 2 bytes

# Tính toán chính xác tổng số byte máy tính cần đọc từ ESP32
TOTAL_BYTES = RECORD_SECONDS * SAMPLE_RATE * CHANNELS * SAMPWIDTH
OUTPUT_FILENAME = "esp32_audio_record.wav"

try:
    # Mở cổng Serial
    print(f"Đang kết nối tới {SERIAL_PORT} ở tốc độ {BAUD_RATE}...")
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=10)
    time.sleep(2) # Đợi ESP32 khởi động lại sau khi mở kết nối Serial

    print("Đang gửi lệnh ghi âm ('R') tới ESP32...")
    ser.write(b'R')

    print(f"Đang thu thập dữ liệu âm thanh ({RECORD_SECONDS} giây)...")
    # Đọc chặn (block) cho đến khi nhận đủ số byte
    raw_audio_data = ser.read(TOTAL_BYTES) 

    if len(raw_audio_data) < TOTAL_BYTES:
        print("Cảnh báo: Không nhận đủ dữ liệu. Có thể do timeout.")
    else:
        print("Đã nhận đủ dữ liệu! Đang tiến hành tạo file .wav...")

    # Sử dụng thư viện wave để ghi file
    with wave.open(OUTPUT_FILENAME, 'wb') as wf:
        wf.setnchannels(CHANNELS)
        wf.setsampwidth(SAMPWIDTH)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(raw_audio_data)

    print(f"Hoàn tất! File đã được lưu tại: {OUTPUT_FILENAME}")

except Exception as e:
    print(f"Đã xảy ra lỗi: {e}")
finally:
    if 'ser' in locals() and ser.is_open:
        ser.close()
        print("Đã đóng cổng Serial.")