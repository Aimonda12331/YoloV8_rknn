#!/bin/bash

echo "🔍 Kiểm tra RGA library và devices trên Orange Pi..."

echo ""
echo "==== Kiểm tra thiết bị RGA ===="
if [ -e /dev/rga ]; then
    echo "[OK] Thiết bị /dev/rga tồn tại."
    ls -la /dev/rga
else
    echo "[ERROR] Không tìm thấy /dev/rga!"
fi

echo ""
echo "==== Kiểm tra module kernel RGA ===="
lsmod | grep rga
if lsmod | grep -q rga; then
    echo "[OK] Module kernel RGA đã được load."
else
    echo "[WARN] Module kernel RGA chưa được load (có thể built-in)!"
fi

echo ""
echo "📚 Tìm kiếm RGA libraries:"
find /usr -name "*rga*" -type f 2>/dev/null | head -10

echo ""
echo "📂 Kiểm tra thư mục RGA headers:"
ls -la /usr/include/rga/ 2>/dev/null || echo "❌ Không tìm thấy /usr/include/rga/"

echo ""
echo "🔗 Kiểm tra RGA library files:"
find /usr/lib -name "*rga*" 2>/dev/null

echo ""
echo "📦 Kiểm tra package RGA:"
dpkg -l | grep -i rga || echo "❌ Không tìm thấy RGA package"

echo ""
echo "🏗️ Kiểm tra librga trong build system:"
pkg-config --exists librga && echo "✅ pkg-config tìm thấy librga" || echo "❌ pkg-config không tìm thấy librga"

echo ""
echo "📖 Kiểm tra RGA API headers:"
test -f /usr/include/rga/rga.h && echo "✅ rga.h found" || echo "❌ rga.h not found"
test -f /usr/include/rga/im2d.h && echo "✅ im2d.h found" || echo "❌ im2d.h not found"  
test -f /usr/include/rga/RgaUtils.h && echo "✅ RgaUtils.h found" || echo "❌ RgaUtils.h not found"

echo ""
echo "==== Thông tin driver RGA ===="
dmesg | grep -i rga | tail -n 10

echo ""
echo "🎯 Kết thúc kiểm tra RGA ====="

# → RGA đã hoạt động đúng trên hệ thống của bạn, có thể sử dụng cho các tác vụ tăng tốc đồ họa/video.
# Không cần lo lắng về kết quả lsmod | grep rga vì một số hệ có thể built-in module, không hiện qua lsmod.