# LÝ THUYẾT VỀ PPPoS VÀ GIAO TIẾP GIỮA ESP32 VỚI MODULE SIM

Tài liệu này trình bày các khái niệm nền tảng về giao thức PPPoS và nguyên lý cấu hình giao tiếp giữa vi điều khiển (ESP32) với Module SIM (A7677S/SIM7600).

---

## PHẦN 1: LÝ THUYẾT VỀ GIAO THỨC PPPoS (Point-to-Point Protocol over Serial)

### 1. PPP là gì?
**PPP (Point-to-Point Protocol)** là một giao thức mạng thuộc Lớp Liên kết dữ liệu (Data Link Layer - Lớp 2 trong mô hình OSI). Nó được sử dụng để thiết lập một kết nối trực tiếp, đồng bộ hoặc không đồng bộ giữa hai node mạng (ở đây là ESP32 và Trạm phát sóng của Nhà mạng).

**PPPoS** là việc chạy giao thức PPP trên một đường truyền vật lý nối tiếp (Serial/UART). Trong các hệ thống nhúng IoT, PPPoS biến cổng UART vốn dùng để truyền ký tự (Text) thành một "card mạng" (Network Interface) có thể truyền các gói tin mạng (IP Packets).

### 2. Quá trình hoạt động của PPPoS 
Tiến trình thiết lập liên kết PPPoS trải qua 4 giai đoạn (Phase) cốt lõi:

*   **Giai đoạn 1 - Dead & Establish (Thiết lập liên kết - LCP):**
    Sử dụng giao thức **LCP (Link Control Protocol)**. Hai bên (ESP32 và Nhà mạng) thỏa thuận về kích thước gói tin (MTU/MRU) và các chuẩn vật lý cơ bản.
*   **Giai đoạn 2 - Authenticate (Xác thực):**
    Sử dụng PAP (Password Authentication Protocol) hoặc CHAP (Challenge Handshake Authentication Protocol). Hầu hết các nhà mạng 4G tại Việt Nam hiện nay bỏ qua bước nhập User/Pass mạng, tuy nhiên bước này vẫn diễn ra trong nền.
*   **Giai đoạn 3 - Network (Đàm phán mạng - IPCP):**
    Sử dụng giao thức **IPCP (Internet Protocol Control Protocol)**. Trạm phát sóng (ISP) sẽ cấp phát cho ESP32 một địa chỉ IP động (Private IP), Subnet Mask, và địa chỉ DNS Servers.
*   **Giai đoạn 4 - Running (Chuyển tiếp dữ liệu - Data Mode):**
    Sau khi IPCP hoàn tất, liên kết được mở. Toàn bộ dữ liệu gửi qua chân UART lúc này KHÔNG còn là chuỗi ký tự (AT Command) nữa, mà là các gói tin TCP/IP thực thụ được đóng gói (Encapsulated) trong Frame của PPP. 

### 3. Sự chuyển đổi Chế độ (Command Mode vs Data Mode)
*   **Command Mode:** Khi mới khởi động, ESP32 nói chuyện với SIM bằng lệnh Text như `AT+CPIN?`, `AT+COPS?`.
*   **Chuyển mạch:** ESP32 gửi lệnh `ATD*99#` (Kích hoạt Dial-up). Trạng thái chuyển sang **Data Mode**. Hệ điều hành (LwIP) tiếp quản hoàn toàn cổng UART để chạy PPPoS. Các lệnh AT sẽ bị vô hiệu hóa cho đến khi PPP bị ngắt.

---

## PHẦN 2: LÝ THUYẾT CẤU HÌNH GIAO TIẾP & KẾT NỐI ESP32 VỚI SIM MODEM

Việc giao tiếp giữa ESP32 và Module 4G đòi hỏi sự đồng bộ khắt khe ở cả 3 tầng: Nguồn điện (Power), Tín hiệu cứng (Hardware Signals), và Tập lệnh điều khiển (Software/AT Command).

### 1. Lớp Nguồn & Khởi động vật lý (Power & Booting)
Module SIM 4G/LTE hoạt động độc lập như một chiếc điện thoại di động tĩnh.
*   **Nguồn cấp (VBAT):** Mạch yêu cầu dòng đỉnh (Peak Current) có thể lên tới 2A khi phát sóng mạng. Không thể dùng nguồn 3.3V/5V lấy từ mạch ESP32. Cần nguồn ngoài (Adapter hoặc Lipo) có chung tín hiệu Mass (GND) với mạch ESP32.
*   **Chân PWRKEY (Power Key):** Module thường ở trạng thái Tắt/Chờ khi vừa cấp điện. Phải giả lập thao tác "nhấn nút nguồn" bằng cách dùng một GPIO của ESP32 kích một xung điện tích cực theo Datasheet (Ví dụ: Kéo xuống LOW hoặc lên HIGH trong vòng 1-2 giây) để đánh thức bộ xử lý của SIM.
*   **Chân RST (Reset):** Nối với GPIO để kéo sập module và ép khởi động lại từ đầu khi phần mềm bị treo.

### 2. Lớp Giao tiếp UART 
*   **Chuẩn kết nối:** Dùng bộ UART của điều khiển vi (TX nối RX, RX nối TX). 
*   **Tốc độ Baud:** 115200 bps là tốc độ chuẩn nhất để giao tiếp và chạy PPPoS.
*   **Chống nhiễu:** Tuyệt đối không sử dụng các chân Bootstrap của ESP32 (như GPIO 0, 2, 5, 12, 15) làm chân TX/RX nếu mạch SIM không được cách ly tụ/trở. Xung điện giật ngược từ SIM trong lúc khởi động dội vào các chân này sẽ làm ESP32 hiểu nhầm là lệnh Nạp Firmware (Download Mode) và sập hệ thống.

### 3. Lớp Phần mềm (Phân tích AT Command Lifecycle)
Để đưa module vào trạng thái sẵn sàng cho PPPoS, ESP32 cần thực hiện chuỗi kịch bản lệnh AT nghiêm ngặt:
1.  **Thăm dò:** Gửi `AT` liên tục để đồng bộ hóa Baudrate.
2.  **Kiểm tra SIM:** Lệnh `AT+CPIN?`. Đợi module trả về `+CPIN: READY` (Chứng tỏ thẻ SIM hoạt động và không bị khóa mã PIN).
3.  **Đăng ký mạng sóng (Network Registration):** Lệnh `AT+CREG?`. Đợi trả về `0,1` (Đăng ký mạng Local thành công) hoặc `0,5` (Đăng ký băng tần Roaming).
4.  **Kiểm tra Nhà mạng (Operator):** Lệnh `AT+COPS?`. Module trả về tên hoặc mã định danh (PLMN) của trạm phát sóng (ví dụ 45204 là Viettel).
5.  **Cấu hình APN (Access Point Name):** Cực kỳ quan trọng. Lệnh `AT+CGDCONT=1,"IP","<Tên APN>"`. Tùy thuộc vào nhà mạng đọc được ở bước trên, ESP32 phải nạp APN chính xác (v-internet, m-wap, m3-world...). Sai APN, bước cuối cùng sẽ chết.
6.  **Kích hoạt Data:** Gọi `ATD*99#`.

### 4. ESP-IDF Framework đóng vai trò gì?
ESP32 (với hệ điều hành FreeRTOS và nhân LwIP) sử dụng thư viện `esp_modem`. Thư viện này thiết kế mô hình DTE (Data Terminal Equipment - ESP32) và DCE (Data Circuit-terminating Equipment - Module SIM). Nó bao bọc chu trình gửi lệnh AT rườm rà thành các API ngôn ngữ C/C++ trực quan (`esp_modem_set_apn`, `esp_modem_set_mode`). Sau khi lệnh `ATD*99#` thành công, nó đẩy luồng UART cho nhân LwIP để biến nó thành cái card mạng (`esp_netif`) tích hợp hoàn hảo với hệ thống Socket của ESP.
