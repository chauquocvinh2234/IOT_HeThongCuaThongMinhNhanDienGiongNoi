import serial
import wave

# --- CẤU HÌNH HỆ THỐNG ---
PORT = "COM6"  # Đổi thành cổng COM thực tế của mạch ESP32
BAUD_RATE = 921600
SAMPLE_RATE = 16000

# Tên file cố định. Mỗi lần có người mới, file này sẽ bị ghi đè thành file mới nhất.
OUTPUT_FILE = "latest_voice.wav" 

print(f"[*] Đang lắng nghe trên cổng {PORT}... (Nhấn Ctrl+C để thoát)")

try:
    ser = serial.Serial(PORT, BAUD_RATE)
except Exception as e:
    print(f"[!] Lỗi kết nối. Hãy chắc chắn bạn đã ĐÓNG Serial Monitor trên Arduino IDE.\nChi tiết: {e}")
    exit()

while True:
    try:
        # Đọc từng dòng từ ESP32
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        
        if line == "START_RECORDING":
            print("\n[>>>] Phát hiện người. Đang nhận dữ liệu âm thanh từ ESP32...")
            frames = []
            
            # Liên tục đọc dữ liệu byte thô
            while True:
                chunk = ser.read(1024)
                
                # Kiểm tra xem có nhận được cờ kết thúc chưa
                if b"END_RECORDING" in chunk:
                    # Cắt bỏ chữ END_RECORDING ra khỏi mảng âm thanh
                    audio_part = chunk.split(b"END_RECORDING")[0]
                    frames.append(audio_part)
                    break
                
                frames.append(chunk)

            print(f"[*] Đã nhận đủ dữ liệu. Đang lưu đè file '{OUTPUT_FILE}'...")
            
            # Lưu thành file WAV chuẩn
            with wave.open(OUTPUT_FILE, 'wb') as wf:
                wf.setnchannels(1)      # Mono
                wf.setsampwidth(2)      # 16-bit
                wf.setframerate(SAMPLE_RATE)
                wf.writeframes(b''.join(frames))
                
            print("[+] Đã lưu file mới nhất! Hệ thống tiếp tục chờ người tiếp theo...")
            
            # =========================================================
            # CHÈN CODE GỌI MÔ HÌNH AI Ở ĐÂY
            # Ví dụ: feature = extract_features(OUTPUT_FILE)
            # =========================================================
            
    except KeyboardInterrupt:
        print("\n[!] Đã tắt chương trình lắng nghe.")
        break
    except Exception as e:
        # Bỏ qua các lỗi rác trong quá trình truyền tải
        pass