import os
import requests
import threading
from dotenv import load_dotenv

# Load variables from .env file
load_dotenv()

from flask import Flask, request, jsonify
import wespeaker
import numpy as np
from datetime import datetime
from pymongo import MongoClient
from bson.objectid import ObjectId
from werkzeug.security import generate_password_hash, check_password_hash
from pydub import AudioSegment
from flasgger import Swagger
from pyngrok import ngrok

app = Flask(__name__)

# Lấy Token và Chat ID của Bot Telegram
TELEGRAM_BOT_TOKEN = os.environ.get("TELEGRAM_BOT_TOKEN")
TELEGRAM_CHAT_ID = os.environ.get("TELEGRAM_CHAT_ID")

# Cấu hình Swagger UI
swagger_config = {
    "headers": [],
    "specs": [
        {
            "endpoint": 'apispec_1',
            "route": '/apispec_1.json',
            "rule_filter": lambda rule: True,
            "model_filter": lambda tag: True,
        }
    ],
    "static_url_path": "/flasgger_static",
    "swagger_ui": True,
    "specs_route": "/apidocs/"
}

swagger_template = {
    "swagger": "2.0",
    "info": {
        "title": "IOT Smart Door API",
        "description": "Tài liệu API cho hệ thống Nhà Thông Minh nhận diện giọng nói.",
        "version": "1.0.0"
    }
}

swagger = Swagger(app, config=swagger_config, template=swagger_template)

# ==========================================
# CẤU HÌNH MONGODB ATLAS
# ==========================================
# Thay connection string bên dưới bằng connection string thật của bạn
MONGO_URI = os.environ.get("MONGO_URI")
DB_NAME = "iot_smart_door"

client = MongoClient(MONGO_URI)
db = client[DB_NAME]
users_collection = db["users"]
history_collection = db["door_history"]

print("[+] Đã kết nối MongoDB Atlas thành công!")

# ==========================================
# NẠP MÔ HÌNH WESPEAKER
# ==========================================
print("[*] Đang nạp mô hình WeSpeaker...")
model = wespeaker.load_model('english')
model.set_device("cpu")
print("[+] Đã nạp mô hình thành công!")


# ==========================================
# HÀM TIỆN ÍCH (UTILITY FUNCTIONS)
# ==========================================
def send_telegram_message_task(text_msg):
    """Hàm thực thi việc gọi API Telegram (chạy trong luồng riêng)."""
    if not TELEGRAM_BOT_TOKEN or not TELEGRAM_CHAT_ID:
        print("[!] Cảnh báo: Chưa cấu hình TELEGRAM_BOT_TOKEN hoặc TELEGRAM_CHAT_ID trong file .env")
        return

    url = f"https://api.telegram.org/bot{TELEGRAM_BOT_TOKEN}/sendMessage"
    payload = {
        "chat_id": TELEGRAM_CHAT_ID,
        "text": text_msg
    }
    
    try:
        # Gọi API Telegram với timeout ngắn để an toàn cho luồng
        response = requests.post(url, json=payload, timeout=5)
        if response.status_code == 200:
            print("[Telegram] Đã gửi thông báo thành công.")
        else:
            print(f"[Telegram] Lỗi gửi thông báo: {response.text}")
    except Exception as e:
        print(f"[Telegram] Lỗi kết nối: {e}")

def notify_telegram(text_msg):
    """Hàm kích hoạt luồng ngầm để gửi thông báo."""
    # Tạo một luồng (thread) mới để gửi tin nhắn mà không block luồng chính của Flask
    thread = threading.Thread(target=send_telegram_message_task, args=(text_msg,))
    thread.start()

def convert_to_wav_16k_mono(input_path, output_path="converted.wav"):
    """
    Chuyển đổi file âm thanh bất kỳ (m4a, mp3, ogg, webm, ...)
    sang định dạng WAV 16kHz, 1 kênh (mono), 16-bit PCM.
    Yêu cầu: cài đặt ffmpeg trên hệ thống.
    """
    print(f"  -> Đang chuyển đổi '{input_path}' sang WAV 16kHz mono...")
    audio = AudioSegment.from_file(input_path)
    audio = audio.set_frame_rate(16000)    # 16 kHz
    audio = audio.set_channels(1)          # Mono
    audio = audio.set_sample_width(2)      # 16-bit (2 bytes)
    audio.export(output_path, format="wav")
    print(f"  -> ✅ Đã chuyển đổi thành công: {output_path}")
    return output_path


def get_embedding(file_path):
    """Trích xuất vector đặc trưng giọng nói từ file âm thanh."""
    emb = model.extract_embedding(file_path)
    # Tùy phiên bản WeSpeaker, kết quả có thể là Tensor hoặc Numpy array.
    # Ép kiểu về mảng 1 chiều (1D numpy array) để dễ tính toán
    if hasattr(emb, 'numpy'):
        emb = emb.numpy()
    return emb.flatten()


def compute_cosine_similarity(v1, v2):
    """Tính độ tương đồng Cosine giữa 2 vector."""
    return np.dot(v1, v2) / (np.linalg.norm(v1) * np.linalg.norm(v2))


# ==========================================
# 1. API ĐĂNG NHẬP - /login
# ==========================================
@app.route('/login', methods=['POST'])
def login():
    """
    Đăng nhập người dùng.
    ---
    tags:
      - Xác thực (Authentication)
    consumes:
      - application/json
    parameters:
      - in: body
        name: body
        required: true
        schema:
          type: object
          properties:
            username:
              type: string
              example: admin
            password:
              type: string
              example: 123456
    responses:
      200:
        description: Đăng nhập thành công
      400:
        description: Lỗi dữ liệu gửi lên
      401:
        description: Sai tài khoản hoặc mật khẩu
      500:
        description: Lỗi máy chủ
    """
    data = request.get_json()

    if not data:
        return jsonify({"status": "error", "message": "Dữ liệu không hợp lệ"}), 400

    username = data.get('username')
    password = data.get('password')

    if not username or not password:
        return jsonify({
            "status": "error",
            "message": "Vui lòng nhập đầy đủ tên đăng nhập và mật khẩu"
        }), 400

    try:
        # Tìm user trong MongoDB
        user = users_collection.find_one({"username": username})

        if not user:
            return jsonify({
                "status": "error",
                "message": "Tên đăng nhập không tồn tại"
            }), 401

        # Kiểm tra mật khẩu
        if not check_password_hash(user["password"], password):
            return jsonify({
                "status": "error",
                "message": "Mật khẩu không chính xác"
            }), 401

        fullname = user["personal_information"]["fullname"]
        print(f"[+] Đăng nhập thành công: {fullname} ({username})")

        return jsonify({
            "status": "success",
            "message": f"Chào mừng {fullname} quay trở lại!",
            "user": {
                "user_id": str(user["_id"]),
                "username": user["username"],
                "fullname": fullname
            }
        })

    except Exception as e:
        print(f"[!] Lỗi đăng nhập: {e}")
        return jsonify({"status": "error", "message": str(e)}), 500


# ==========================================
# 2. API ĐĂNG KÝ GIỌNG NÓI - /register
# ==========================================
@app.route('/register', methods=['POST'])
def register_voice():
    """
    Đăng ký người dùng mới kèm giọng nói.
    ---
    tags:
      - Xác thực (Authentication)
    consumes:
      - multipart/form-data
    parameters:
      - in: formData
        name: username
        type: string
        required: true
        description: Tên đăng nhập
      - in: formData
        name: password
        type: string
        required: true
        description: Mật khẩu
      - in: formData
        name: fullname
        type: string
        required: true
        description: Họ và tên đầy đủ
      - in: formData
        name: audio
        type: file
        required: true
        description: File âm thanh ghi âm giọng nói (.wav, .m4a)
    responses:
      201:
        description: Đăng ký thành công
      400:
        description: Thiếu thông tin hoặc file âm thanh
      409:
        description: Username đã tồn tại
      500:
        description: Lỗi hệ thống
    """
    TEMP_RAW_FILE = "temp_register_raw"   # File gốc từ frontend (có thể là .m4a, .wav, ...)
    TEMP_WAV_FILE = "temp_register.wav"    # File sau khi convert sang WAV 16kHz mono

    # ------- Kiểm tra các trường bắt buộc -------
    username = request.form.get('username')
    password = request.form.get('password')
    fullname = request.form.get('fullname')

    if not username or not password or not fullname:
        return jsonify({
            "status": "error",
            "message": "Thiếu thông tin bắt buộc (username, password, fullname)"
        }), 400

    # ------- Kiểm tra trùng username -------
    if users_collection.find_one({"username": username}):
        return jsonify({
            "status": "error",
            "message": f"Username '{username}' đã tồn tại trong hệ thống"
        }), 409

    # ------- Nhận file âm thanh -------
    if 'audio' not in request.files:
        return jsonify({
            "status": "error",
            "message": "Không tìm thấy file âm thanh. Vui lòng gửi file với key 'audio'"
        }), 400

    audio_file = request.files['audio']

    if audio_file.filename == '':
        return jsonify({
            "status": "error",
            "message": "File âm thanh trống"
        }), 400

    # Xác định extension gốc từ tên file upload
    original_ext = os.path.splitext(audio_file.filename)[1] or '.m4a'
    raw_path = TEMP_RAW_FILE + original_ext

    # Lưu file gốc tạm để xử lý
    audio_file.save(raw_path)

    try:
        print(f"\n[*] Đang đăng ký giọng nói cho: {fullname} ({username})")
        print(f"  -> File nhận được: {audio_file.filename} ({original_ext})")

        # Bước 0: Chuyển đổi file âm thanh sang WAV 16kHz mono
        convert_to_wav_16k_mono(raw_path, TEMP_WAV_FILE)

        # Bước 1: Trích xuất embedding từ file giọng nói đã chuẩn hóa
        embedding = get_embedding(TEMP_WAV_FILE)
        print(f"  -> Đã trích xuất embedding: shape={embedding.shape}")

        # Bước 2: Chuyển numpy array sang list để lưu vào MongoDB
        embedding_list = embedding.tolist()

        # Bước 3: Hash mật khẩu để bảo mật
        hashed_password = generate_password_hash(password)

        # Bước 4: Tạo document theo cấu trúc yêu cầu và lưu vào MongoDB
        user_document = {
            "username": username,
            "password": hashed_password,
            "personal_information": {
                "fullname": fullname,
                "speech_vector": embedding_list
            }
        }

        result = users_collection.insert_one(user_document)

        print(f"  -> ✅ Đã lưu vào MongoDB Atlas! (ID: {result.inserted_id})")

        return jsonify({
            "status": "success",
            "message": f"Đăng ký thành công cho '{fullname}'",
            "user_id": str(result.inserted_id),
            "embedding_dimension": len(embedding_list)
        }), 201

    except Exception as e:
        print(f"[!] Lỗi đăng ký: {e}")
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

    finally:
        # Dọn dẹp file tạm
        for f in [raw_path, TEMP_WAV_FILE]:
            if os.path.exists(f):
                os.remove(f)


# ==========================================
# 3. API XÁC THỰC GIỌNG NÓI 1:N - /verify
# ==========================================
@app.route('/verify', methods=['POST'])
def verify_voice():
    """
    Xác thực giọng nói (Nhận diện mở cửa).
    ---
    tags:
      - Nhận diện (Verification)
    consumes:
      - multipart/form-data
      - audio/wav
    parameters:
      - in: formData
        name: audio
        type: file
        required: false
        description: File âm thanh chứa giọng nói cần mở cửa (Upload file qua form-data hoặc gửi trực tiếp dạng raw binary).
    responses:
      200:
        description: Kết quả xác thực (ACCEPTED hoặc REJECTED)
      400:
        description: Không tìm thấy file âm thanh
      500:
        description: Lỗi máy chủ
    """
    FILE_KIEM_TRA = "temp_received.wav"

    # Nhận file từ ESP32
    if request.content_type == 'audio/wav':
        with open(FILE_KIEM_TRA, "wb") as f:
            f.write(request.data)
    elif 'audio' in request.files:
        request.files['audio'].save(FILE_KIEM_TRA)
    else:
        return jsonify({"error": "Không tìm thấy dữ liệu âm thanh hợp lệ"}), 400

    try:
        print(f"\n[*] Nhận yêu cầu mở cửa. Đang trích xuất đặc trưng...")

        # Bước 1: Biến giọng nói vừa thu thành Vector
        incoming_vector = get_embedding(FILE_KIEM_TRA)

        best_match_name = "Unknown"
        best_match_id = None
        highest_score = 0.0

        # Bước 2: Lấy toàn bộ user từ MongoDB và so khớp 1:N
        all_users = users_collection.find({}, {
            "username": 1,
            "personal_information.fullname": 1,
            "personal_information.speech_vector": 1
        })

        for user in all_users:
            stored_vector = np.array(user["personal_information"]["speech_vector"])
            fullname = user["personal_information"]["fullname"]

            score = compute_cosine_similarity(incoming_vector, stored_vector)
            score = float(score)

            print(f"  + So sánh với {fullname} ({user['username']}): {score:.4f}")

            if score > highest_score:
                highest_score = score
                best_match_name = fullname
                best_match_id = user["_id"]

        # Bước 3: Kiểm tra xem điểm cao nhất có vượt ngưỡng an toàn không
        THRESHOLD = 0.6
        is_accepted = highest_score >= THRESHOLD

        if is_accepted:
            result_msg = "ACCEPTED"
            print(f" => ✅ KẾT QUẢ: Cho phép {best_match_name} vào nhà! (Điểm: {highest_score:.4f})")
            notify_telegram(f"✅ Cửa đã được mở thành công bởi: {best_match_name} (Độ tin cậy: {highest_score:.2f})")
        else:
            result_msg = "REJECTED"
            print(f" => ❌ KẾT QUẢ: Từ chối mở cửa. Người lạ! (Điểm gần nhất: {highest_score:.4f})")
            notify_telegram("🚨 CẢNH BÁO: Phát hiện người lạ đang cố gắng mở cửa bằng giọng nói!")
        # Bước 3.1: Lưu lịch sử mở cửa vào MongoDB (cả thành công và thất bại)
        history_record = {
            "user_id": best_match_id if is_accepted else None,
            "fullname": best_match_name if is_accepted else "Không xác định",
            "time": datetime.now(),
            "status": result_msg
        }
        history_collection.insert_one(history_record)
        print(f"  -> 📝 Đã lưu lịch sử mở cửa: {result_msg}")

        # Bước 4: Trả kết quả về cho ESP32
        return jsonify({
            "status": "success",
            "accepted": is_accepted,
            "matched_user": best_match_name if is_accepted else "None",
            "similarity": highest_score,
            "message": result_msg
        })

    except Exception as e:
        print(f"[!] Lỗi xử lý: {e}")
        return jsonify({"error": str(e)}), 500

    finally:
        # Dọn dẹp file tạm
        if os.path.exists(FILE_KIEM_TRA):
            os.remove(FILE_KIEM_TRA)


# ==========================================
# 4. API LẤY LỊCH SỬ MỞ CỬA - /history
# ==========================================
@app.route('/history', methods=['GET'])
def get_door_history():
    """
    Lấy toàn bộ lịch sử mở cửa.
    ---
    tags:
      - Lịch sử (History)
    responses:
      200:
        description: Danh sách lịch sử mở cửa, sắp xếp mới nhất trước
      500:
        description: Lỗi máy chủ
    """
    try:
        # Lấy toàn bộ lịch sử, sắp xếp mới nhất trước
        records = history_collection.find({}, {"_id": 0}).sort("time", -1)

        history_list = []
        for record in records:
            history_list.append({
                "user_id": str(record["user_id"]) if record.get("user_id") else None,
                "fullname": record["fullname"],
                "time": record["time"].strftime("%Y-%m-%d %H:%M:%S"),
                "status": record.get("status", "UNKNOWN")
            })

        print(f"[*] Trả về {len(history_list)} bản ghi lịch sử mở cửa.")

        return jsonify({
            "status": "success",
            "total": len(history_list),
            "history": history_list
        })

    except Exception as e:
        print(f"[!] Lỗi lấy lịch sử: {e}")
        return jsonify({"status": "error", "message": str(e)}), 500


# ==========================================
# 5. API ĐỔI THÔNG TIN / GIỌNG NÓI - /changeInformation
# ==========================================
@app.route('/changeInformation', methods=['POST'])
def change_information():
    """
    Cập nhật thông tin / giọng nói.
    ---
    tags:
      - Người dùng (User)
    consumes:
      - multipart/form-data
    parameters:
      - in: formData
        name: user_id
        type: string
        required: true
        description: ID của người dùng (bắt buộc)
      - in: formData
        name: fullname
        type: string
        required: false
        description: Họ và tên mới (tùy chọn)
      - in: formData
        name: audio
        type: file
        required: false
        description: File giọng nói mới (tùy chọn)
    responses:
      200:
        description: Cập nhật thành công
      400:
        description: Lỗi dữ liệu đầu vào
      404:
        description: Không tìm thấy user
      500:
        description: Lỗi hệ thống
    """
    user_id = request.form.get('user_id')
    fullname = request.form.get('fullname')
    audio_file = request.files.get('audio')

    if not user_id:
        return jsonify({"status": "error", "message": "Thiếu user_id"}), 400

    try:
        # Tìm user
        user = users_collection.find_one({"_id": ObjectId(user_id)})
        if not user:
            return jsonify({"status": "error", "message": "Người dùng không tồn tại"}), 404

        update_fields = {}

        # Nếu có cập nhật tên
        if fullname and fullname.strip() != "":
            update_fields["personal_information.fullname"] = fullname.strip()

        # Nếu có cập nhật giọng nói
        if audio_file and audio_file.filename != '':
            TEMP_RAW_FILE = "temp_update_raw"
            TEMP_WAV_FILE = "temp_update.wav"
            
            original_ext = os.path.splitext(audio_file.filename)[1] or '.m4a'
            raw_path = TEMP_RAW_FILE + original_ext

            audio_file.save(raw_path)
            try:
                # Trích xuất embedding mới
                convert_to_wav_16k_mono(raw_path, TEMP_WAV_FILE)
                embedding = get_embedding(TEMP_WAV_FILE)
                embedding_list = embedding.tolist()
                
                update_fields["personal_information.speech_vector"] = embedding_list
            finally:
                for f in [raw_path, TEMP_WAV_FILE]:
                    if os.path.exists(f):
                        os.remove(f)

        if not update_fields:
            return jsonify({"status": "error", "message": "Không có thông tin nào để cập nhật"}), 400

        # Cập nhật vào DB
        users_collection.update_one({"_id": ObjectId(user_id)}, {"$set": update_fields})
        
        # Nếu có thay đổi tên, trả về tên mới
        new_fullname = fullname.strip() if (fullname and fullname.strip() != "") else user["personal_information"]["fullname"]

        print(f"[+] Đã cập nhật thông tin cho: {new_fullname} ({user['username']})")

        return jsonify({
            "status": "success",
            "message": "Cập nhật thông tin thành công!",
            "new_fullname": new_fullname
        })

    except Exception as e:
        print(f"[!] Lỗi cập nhật thông tin: {e}")
        return jsonify({"status": "error", "message": str(e)}), 500


if __name__ == '__main__':
    # 1. Dán Auth Token của bạn vào đây
    ngrok_token = os.environ.get("NGROK_AUTH_TOKEN")
    if ngrok_token:
        ngrok.set_auth_token(ngrok_token)
    
    # 2. Khởi tạo đường hầm với domain cố định
    ngrok_domain = os.environ.get("NGROK_DOMAIN", "broken-unrigged-scolding.ngrok-free.dev")
    public_url = ngrok.connect(5000, domain=ngrok_domain).public_url
    
    print(f"\n[🚀] NGROK TUNNEL ĐÃ MỞ CỐ ĐỊNH TẠI: {public_url}")
    print(f"[🚀] API Swagger test: {public_url}/apidocs/\n")

    # 3. Khởi chạy server Flask
    app.run(host='0.0.0.0', port=5000)
