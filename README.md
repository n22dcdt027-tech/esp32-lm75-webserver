# Hệ thống giám sát và cảnh báo nhiệt độ ESP32 LM75

## Giới thiệu

Hệ thống sử dụng ESP32 NodeMCU LuaNode32 và cảm biến LM75
để giám sát, hiển thị và cảnh báo nhiệt độ thông qua máy chủ web nhúng.

## Phần cứng

- ESP32 NodeMCU LuaNode32
- Cảm biến CJMCU-75 LM75
- LED đỏ
- LED xanh
- Còi chủ động
- Hai điện trở 220 ohm
- Breadboard và dây nối

## Kết nối

| Thiết bị | Chân ESP32 |
|---|---|
| LM75 SDA | GPIO21 |
| LM75 SCL | GPIO22 |
| LED đỏ | GPIO25 |
| LED xanh | GPIO26 |
| Còi | GPIO27 |

## Ngưỡng cảnh báo

- Bật cảnh báo khi nhiệt độ lớn hơn hoặc bằng 35°C.
- Tắt cảnh báo khi nhiệt độ nhỏ hơn hoặc bằng 34°C.

## Cách sử dụng

1. Mở chương trình bằng Arduino IDE.
2. Chọn bo ESP32 Dev Module.
3. Điền tên và mật khẩu Wi-Fi.
4. Chọn đúng cổng COM.
5. Biên dịch và nạp chương trình.
6. Mở Serial Monitor ở tốc độ 115200 baud.
7. Truy cập địa chỉ IP được hiển thị.

## Thành viên thực hiện

- Nguyễn Minh Huy
- Lê Hải Đăng
- Nguyễn Duy Tuấn Minh
