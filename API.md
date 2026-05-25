# TÀI LIỆU API & CẤU TRÚC CHƯƠNG TRÌNH

Tài liệu này liệt kê tất cả các Hàm, Biến, Callback, Hằng số (Constant) và các hàm API từ thư viện ESP-IDF được sử dụng trực tiếp trong Project `esp_lte_a7677s_ppp`.

---

## 1. CÁC HẰNG SỐ CẤU HÌNH (Constants & Macros)

Các hằng số này được định nghĩa ở đầu file `main.c` thông qua việc lấy tham số từ giao diện Menuconfig (`sdkconfig` / `Kconfig.projbuild`).

| Tên Hằng Số (Macro) | Nguồn cấp file (Định nghĩa) | Ý nghĩa & Chú thích |
| :--- | :--- | :--- |
| `MODEM_TX_PIN` | `main/Kconfig.projbuild` | Chân ESP32 gửi dữ liệu TX (Hiện đang dùng GPIO 17). |
| `MODEM_RX_PIN` | `main/Kconfig.projbuild` | Chân ESP32 nhận dữ liệu RX (Hiện đang dùng GPIO 16). |
| `MODEM_PWRKEY_PIN` | `main/Kconfig.projbuild` | Chân điều khiển kích bật nguồn cho mạch A7677S (GPIO 13). |
| `MODEM_RST_PIN` | `main/Kconfig.projbuild` | Chân Reset mạch A7677S (GPIO 15). |
| `MODEM_APN` | `main/Kconfig.projbuild` | Chuỗi APN mặc định dự phòng (ví dụ "v-internet"). |
| `PWR_ON_TIME_MS` | `main/main.c` (Dòng 20) | Thời gian độ dài của xung kích nguồn (Pulse): 1500 ms. |
| `PWR_ON_STATUS_MS` | `main/main.c` (Dòng 22) | Thời gian chờ mạch SIM khởi động xong HĐH: 5000 ms. |
| `VBAT_STABLE_MS` | `main/main.c` (Dòng 24) | Thời gian trễ mồi điền áp trước khi bật Module: 100 ms. |

---

## 2. CÁC HÀM XỬ LÝ (Functions & Callbacks) TRONG `main.c`

Đây là các hàm tự viết trong không gian của người dùng:

### `static void power_on_modem(void)`
*   **Vị trí:** `main/main.c` (Khoảng dòng 26)
*   **Chức năng:** Khởi tạo chân GPIO Output. Tạo chuỗi tín hiệu xung (kéo mức `1` rồi nhả về `0`) vào chân PWRKEY nhằm "bấm nút nguồn" đánh thức mạch SIM sau khi lên điện.

### `static void on_ppp_changed(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)`
*   **Vị trí:** `main/main.c` (Khoảng dòng 54)
*   **Chức năng:** Hàm Hook (Callback) được nhân Event-Loop gọi mỗi khi trạng thái PPP bị thay đổi do LwIP điều khiển (Ví dụ: Đột ngột mất tín hiệu ngắt mạng, rớt cáp, báo lỗi Dead Phase, v.v.). Chủ yếu dùng để in log gỡ lỗi hệ thống.

### `static void on_ip_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)`
*   **Vị trí:** `main/main.c` (Khoảng dòng 72)
*   **Chức năng:** Callback cực kỳ quan trọng được hệ thống gọi khi nhận được cấp phát IP thành công (`IP_EVENT_PPP_GOT_IP`).
*   **Khối xử lý:** Ép kiểu lấy `ip_event_got_ip_t`, lấy chuỗi thông tin IP/Netmask/Gateway từ mạng 4G cung cấp và in ra màn hình. Gọi thêm ngầm `esp_netif_get_dns_info` để xuất DNS Server.

### `void app_main(void)`
*   **Vị trí:** `main/main.c`
*   **Chức năng:** Điểm bắt đầu (khởi chạy) của chương trình sau khi boot. Tập hợp toàn bộ logic liên kết Modem và Cấu trúc mạng (xem liệt kê ở phần 3).

---

## 3. CÁC API THƯ VIỆN ESP-IDF & ESP_MODEM ĐÃ SỬ DỤNG

Danh sách này bao gồm các hàm API cốt lõi được gọi bên trong ứng dụng.

### 3.1. Các API Cấu hình Mạng (Network & Event)
*   **File thư viện gốc:** `components/esp_netif/`, `components/esp_event/`
*   **`esp_netif_init()`**: Khởi tạo LwIP Core (Network Stack).
*   **`esp_event_loop_create_default()`**: Tạo vòng lặp sự kiện mặc định để hệ thống lắng nghe các tương tác của hệ điều hành.
*   **`esp_netif_new()`**: Tạo một Netif (Network Interface) chuyên biệt dành cho giao thức PPP. Gắn cấu hình biến thông qua `ESP_NETIF_DEFAULT_PPP()`.

### 3.2. Các cấu trúc cấu hình Modem (Structs/Macros)
*   **File thư viện gốc:** `managed_components/espressif__esp_modem/`
*   **`esp_modem_dte_config_t`**: Struct quy định tầng giao tiếp cứng (Data Terminal Equipment). Cài đặt tốc độ Baud, kích cỡ Buffer, cài pin TX/RX trên ESP32.
*   **`esp_modem_dce_config_t`**: Struct quy định tầng điều khiển đầu cuối mạng (Data Circuit-terminating Equipment). Nơi khởi tạo cấu hình APN truyền vào cho Modem.
*   **`esp_modem_new_dev()`**: Khởi tạo đối tượng modem `dce` theo cấu hình được tiêm. Driver được sử dụng để tương thích cho A7677S là `ESP_MODEM_DCE_SIM7600`.

### 3.3. Các API Điều khiển AT Command qua ESP_MODEM
Toàn bộ các API sau đây thuộc Wrapper `esp_modem` đóng gói lệnh AT thô thành API C an toàn:
*   **`esp_modem_read_pin(dce, &pin_ok)`**: Ngầm gọi lệnh `AT+CPIN?` kiểm tra thẻ SIM. Nếu trả về `+CPIN: READY` thì ghi vào `pin_ok` là `true`.
*   **`esp_modem_at(dce, command, out_response, timeout)`**: Một hàm đệm cực kỳ hữu dụng. Giúp bạn gửi Lệnh Text thô (Ví dụ `"ATI
"`) và hứng kết quả từ UART trả về ngay lập tức vào một chuỗi Buffer mảng Char nội bộ.
*   **`esp_modem_get_imei()`**: API trả về IMEI của thiết bị. Ngầm gọi `AT+GSN`.
*   **`esp_modem_get_signal_quality()`**: API lấy chỉ số sóng và nhiễu (RSSI, BER). Ngầm gọi `AT+CSQ`.

### 3.4. API Quyết định chuyển mạch
*   **`esp_modem_set_apn(dce, apn_string)`**: Sửa đổi APN trên Data mode.
*   **`esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA)`**: Lệnh TỐI QUAN TRỌNG giúp ra lệnh cho ESP32 ngưng gửi chữ AT Command, tự động thực thi quá trình quay số PPPoS (`ATD*99#`).
