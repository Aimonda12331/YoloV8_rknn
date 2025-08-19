#!/bin/bash

MODEL_PATH="./yolov8.rknn"

echo "🔍 Kiểm tra thông tin model: $MODEL_PATH"
echo ""

if [ ! -f "$MODEL_PATH" ]; then
    echo "❌ Không tìm thấy file model $MODEL_PATH"
    exit 1
fi

# Kiểm tra rknn_toolkit2 đã cài chưa
if ! command -v rknn_tool &> /dev/null; then
    echo "❌ Chưa cài rknn_toolkit2 (rknn_tool)"
    echo "👉 Hãy cài bằng: pip install rknn-toolkit2"
    exit 1
fi

# In thông tin model
echo "==== Thông tin model RKNN ===="
rknn_tool --model $MODEL_PATH --info
echo "=============================="
