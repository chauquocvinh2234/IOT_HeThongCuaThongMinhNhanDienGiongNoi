# Sử dụng base image PyTorch có sẵn CUDA để chạy GPU
FROM pytorch/pytorch:2.1.0-cuda11.8-cudnn8-runtime

# 1. Thêm dòng này để TẮT tính năng hỏi đáp tương tác khi cài package
ENV DEBIAN_FRONTEND=noninteractive

# 2. (Tùy chọn) Thiết lập luôn múi giờ mặc định để nó không hỏi lại
ENV TZ=Asia/Ho_Chi_Minh

# Cài đặt ffmpeg và các công cụ khác
RUN apt-get update && apt-get install -y \
    ffmpeg \
    git \
    libsndfile1 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy file requirements.txt vào container
COPY requirements.txt .

# Cài đặt toàn bộ thư viện trong requirements.txt
RUN pip install --no-cache-dir -r requirements.txt

# Copy file code backend vào container và đổi tên thành app.py cho gọn
COPY BackendPipelineNhanDangGiongNoi.py /app/app.py

# Mở cổng 5000 cho Flask
EXPOSE 5000

# Khởi chạy server
CMD ["python", "app.py"]