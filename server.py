from flask import Flask, request, jsonify
import wespeaker
import os

# --- ĐÂY CHÍNH LÀ PHẦN KHAI BÁO BỊ THIẾU ---
app = Flask(__name__)
# ------------------------------------------

print("[*] Đang nạp mô hình WeSpeaker...")
# Khởi tạo mô hình AI 1 lần duy nhất khi bật server
model = wespeaker.load_model('english')
model.set_device('cpu')
print("[+] Đã nạp mô hình thành công!")

# File giọng nói chủ nhà đã đăng ký sẵn (đảm bảo file này tồn tại trong cùng thư mục)
FILE_DANG_KY = "vinh_voice1.wav"

@app.route('/verify', methods=['POST'])
def verify_voice():
    FILE_KIEM_TRA = "temp_received.wav"

    # Hỗ trợ nhận trực tiếp luồng byte âm thanh (từ ESP32 gửi lên)
    if request.content_type == 'audio/wav':
        with open(FILE_KIEM_TRA, "wb") as f:
            f.write(request.data)
    # Hỗ trợ nhận file upload truyền thống (để dự phòng)
    elif 'audio' in request.files:
        request.files['audio'].save(FILE_KIEM_TRA)
    else:
        return jsonify({"error": "Không tìm thấy dữ liệu âm thanh hợp lệ"}), 400

    try:
        print(f"\n[*] Đang kiểm tra đối chiếu chữ ký giọng nói...")
        similarity_score = model.compute_similarity(FILE_DANG_KY, FILE_KIEM_TRA)
        
        # Chuyển kiểu dữ liệu về float để có thể parse thành JSON
        score = float(similarity_score)
        
        is_accepted = score >= 0.5
        result_msg = "ACCEPTED" if is_accepted else "REJECTED"

        print(f" -> ĐIỂM: {score:.4f} | KẾT QUẢ: {result_msg}")

        # Trả kết quả về cho ESP32
        return jsonify({
            "status": "success",
            "similarity": score,
            "accepted": is_accepted,
            "message": result_msg
        })
        
    except Exception as e:
        print(f"[!] Lỗi xử lý: {e}")
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    # Chạy server ở cổng 5000 (Localhost)
    app.run(host='0.0.0.0', port=5000)