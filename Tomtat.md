# BÁO CÁO CÔNG VIỆC: TÍCH HỢP MODULE 4G LTE A7677S VỚI ESP32 (PPPoS)

## 1. LCP
Giai đoạn LCP (Link Control Protocol) – bao gồm các trạng thái như Phase Start, Phase Establish, Phase Dead – KHÔNG được thực thi bên trong phần mã nguồn của Component espressif__esp_modem.

Thực chất, Component esp_modem chỉ đóng vai trò làm "môi giới" kết nối ngoại vi cho đến khi lệnh ATD*99# thành công. Sau khi kết nối Data được mở ra, việc đàm phán PPP (bao gồm giao thức LCP, xác thực PAP/CHAP, và sau cùng là IPCP) được Lõi hệ điều hành LwIP (Lightweight IP) của ESP-IDF xử lý hoàn toàn.

Cụ thể, quá trình đó diễn ra tại file nguồn nội bộ sâu trong Framework ESP-IDF:

1. File chính kiểm soát State Machine của giao thức PPP:

Vị trí: ~/esp/esp-idf/components/lwip/lwip/src/netif/ppp/ppp.c
(Đây là bộ thư viện mã nguồn mở lwIP chuẩn quốc tế, được Espressif nhúng vào hệ điều hành).
Trong file này chứa hàm ppp_netif_up() và hàm ppp_start(), chúng quyết định cách PPP State Machine chuyển đổi từ Phase Dead sang Phase Establish (LCP).
2. File chứa thuật toán đàm phán LCP cụ thể:

Vị trí: ~/esp/esp-idf/components/lwip/lwip/src/netif/ppp/lcp.c
Tại file này, các gói tin điều khiển LCP (Control Packets) như Configure-Request, Configure-Ack, Configure-Nak, và Configure-Reject sẽ được gửi xuống đường UART để "bắt tay" cấu hình kích thước MTU/MRU với trạm phát sóng nhà mạng. Mã nguồn thực thi nó rành rành trong hàm như lcp_open(), lcp_lowerup().
3. File phát ra sự kiện Log esp-netif_lwip-ppp: Phase Establish mà bạn thấy trên Terminal:

Vị trí: ~/esp/esp-idf/components/esp_netif/lwip/esp_netif_lwip_ppp.c
(Hoặc esp_netif/lwip/esp_netif_lwip.c).
Khối ESP Netif là một lớp Wrapper nhỏ phía trên LwIP của riêng Espressif. Nó sẽ đăng ký một cái Callback hook vào cấu hình LwIP thông qua cờ #define PPP_NOTIFY_PHASE 1. Mỗi khi LwIP ở file ppp.c chuyển đổi giai đoạn LCP hay IPCP, nó bắn thông điệp lên cho esp_netif gõ các dòng log (Ví dụ Phase Establish, Phase Authenticate, Phase Network, Phase Running) lên màn hình Terminal cho chúng ta thấy.
Như vậy, LCP được chạy ngầm bởi LwIP Network Stack (components/lwip/lwip/src/netif/ppp/lcp.c),
## 2. PAP xac thuc
Cả hai giao thức này đều được nhúng và thực hiện tự động toàn phần bởi nhân điều hành mạng LwIP của ESP-IDF, ngay khi giai đoạn LCP (Establish) thành công.

Dưới đây là chi tiết mã nguồn LwIP nơi diễn ra quá trình này:

1. Phân luồng Xác thực (Authentication Phase Control)
Tiến trình điều hướng LCP chuyển từ Phase Establish sang Phase Authenticate diễn ra trong file nhân điều khiển PPP:

Vị trí: ~/esp/esp-idf/components/lwip/lwip/src/netif/ppp/ppp.c
Ở file này, LwIP kiểm tra cờ cấu hình (PPP_SUPPORT_PAP hoặc PPP_SUPPORT_CHAP). Nếu máy chủ của nhà mạng (Trạm Base Station) đòi hỏi quyền xác thực thông qua việc gửi tín hiệu Configure-REQ, file này sẽ ra lệnh nhảy sang Phase Xác thực tương ứng.
2. Thuật toán Xác thực PAP
Giao thức gửi tài khoản/mật khẩu dạng Plain-text để đăng nhập, thông tin được cấu hình sẵn trong Config Interface LwIP của bạn.

Vị trí File xử lý: ~/esp/esp-idf/components/lwip/lwip/src/netif/ppp/pap.c
Trong file này chứa các hàm thực thi gửi Request (Gửi Username/Password đi), và hàm phân tích gói tin ACKs (Nhà mạng phản hồi Server Chấp nhận pap_rauthack) hoặc NAKs (Từ chối pap_rauthnak). Ở mạng 4G Việt Nam (như Viettel, Wintel), mật khẩu PPP thường là nhúng rỗng (Empty string) nên nó gửi một gói PAP rỗng đi và nhà cung cấp sẽ tự qua cửa bằng ACKs.
3. Thuật toán Xác thực mã hóa CHAP
Giao thức xác thực an toàn hơn, thay vì gửi mật khẩu, nhà mạng gửi đi một cái "Challenge" (thử thách). Máy khách (LwIP) dùng chuỗi đó băm (Hash - MD5) với mật khẩu của mình rồi gửi mã Hash đó lên.

Vị trí File xử lý: ~/esp/esp-idf/components/lwip/lwip/src/netif/ppp/chap-new.c (CHAP thế hệ mới trong LwIP) và components/lwip/lwip/src/netif/ppp/chap_ms.c (MS-CHAP).
Tại đây, các gói Challenge Request đến từ mạng sẽ được nhận và xử lý bằng các thuật toán giải mã băm thông điệp Lớp 3.
Cách đưa tham số vào (Username/Password) trước khi PAP/CHAP chạy:
Dù LwIP tự xử lý ngầm, nhưng biến số mật khẩu là do Lớp Interface cấu hình chuyển xuống.
Trong mạng ESP-IDF của bạn, nếu phải điền pass, bạn sẽ dùng API:

esp_netif_ppp_set_auth() tại file wrapper của Espressif (components/esp_netif/lwip/esp_netif_lwip_ppp.c).
(Nhưng trong dự án của bạn, ở file main.c, chúng ta không cần dùng API vì mạng LTE 3G/4G mặc định cho Module A7677S đã thiết lập cơ chế xác thực rỗng do sim vật lý tự chịu trách nhiệm định danh qua lớp sóng Radio chứ không cần đăng nhập PAP không còn ý nghĩa chặn đường truyền không còn giá trị chặn IP). LwIP tự động trả về xác thực hoàn tất!
## 3. Chốt cấu hình Phần cứng (Đã gỡ lỗi thành công)
Quá trình gỡ lỗi (debug) đã chỉ ra rằng việc chọn lộn chân UART là nguyên nhân chính khiến module treo cứng hoặc không dò được vi mạch SIM (sập nguồn Module).
Chân GPIO 2 của dòng ESP32 là Strapping Pin (quyết định chế độ nạp Code hay chạy Code), nếu cắm thiết bị ngoại vi có dòng rò vào đây sẽ làm nhiễu điện áp cả hệ thống.
Cấu hình giao tiếp phần cứng chuẩn xác nhất hiện tại:
*   **Nguồn cấp Module SIM**: Nguồn ngoài xả dòng lớn (tối thiểu 2A) nối chung dây GND (Mass) với ESP32 để chống sụt áp khởi động.
*   **Chân kích nguồn (PWRKEY)**: Kích hoạt tại **GPIO 13**.
*   **Chân Reset (RST)**: Kích hoạt tại **GPIO 15**.
*   **Giao thức kết nối**: UART (Tốc độ 115200 bps, tắt Flow Control).
*   **UART TX (ESP32 truyền)**: **GPIO 17** (Tuyệt đối không dùng GPIO 2).
*   **UART RX (ESP32 nhận)**: **GPIO 16** (Khuyên dùng).

## 4. IPCP thiet lap lien ket
Tương tự như LCP (Thiết lập liên kết) và PAP/CHAP (Xác thực), giai đoạn IPCP (Internet Protocol Control Protocol) – bước quan trọng nhất để thiết bị cấu hình được địa chỉ IP, Netmask, Gateway và DNS – cũng được thực thi hoàn toàn bên trong tầng Network Stack mã nguồn mở LwIP của ESP-IDF.

Đây là nơi và cách thức giai đoạn đàm phán IP (Phase Network) diễn ra:

1. File xử lý cốt lõi của IPCP
Sau khi vượt qua Phase Authenticate, máy trạng thái PPP của LwIP sẽ chuyển giao quyền điều khiển sang file chuyên trách giao thức IPCP.

Vị trí File xử lý: ~/esp/esp-idf/components/lwip/lwip/src/netif/ppp/ipcp.c
Chức năng: File này chứa toàn bộ logic máy trạng thái đàm phán thông số mạng IPv4. Đây là nơi LwIP tạo và gửi gói tin Configure-Request (yêu cầu cấp IP) lên mạng di động thông qua cổng UART.
Các hàm chủ chốt trong file này:
ipcp_up(): Được gọi khi IPCP đàm phán thành công và IP đã được gán hoàn tất.
ipcp_reqci(): Xây dựng gói tin đàm phán chứa các tùy chọn (ví dụ: Yêu cầu cấp IP (Option 3), Yêu cầu DNS chính/phụ (Option 129, Option 130)).
ipcp_ackci() và ipcp_nakci(): Xử lý các gói tin phản hồi từ trạm phát sóng nhà mạng. Khi mạng di động đồng ý cấp một dải IP (như 10.37.x.x), nó sẽ gửi bản tin ACK xác nhận.
2. Đăng ký thông số IP vào hệ thống sau khi trạm cấp phát
Ngay khi thuật toán IPCP ở file ipcp.c đàm phán xong (đổi state sang OPENED), nó sẽ tiêm địa chỉ IP/DNS thu được vào Card mạng ảo (Netif) của LwIP:

Cập nhật Netif: Hệ thống tự động gán địa chỉ vào cấu trúc dữ liệu netif thông qua các hàm như netif_set_addr(), netif_set_gw(), netif_set_up().
Bắn sự kiện ra ngoài (Giao diện người dùng): Ngay lúc này, luồng LwIP sẽ kích hoạt một callback báo cho tầng ESP-IDF biết. File wrapper của Espressif là:
Vị trí: ~/esp/esp-idf/components/esp_netif/lwip/esp_netif_lwip_ppp.c
Hàm: on_ppp_notify() -> Nó sẽ kích hoạt sự kiện IP_EVENT_PPP_GOT_IP.
Đây chính là cái cò súng kích hoạt để file main.c của bạn (hàm on_ip_event) nhận được dữ liệu và in log báo mừng: "CHÚC MỪNG! KẾT NỐI INTERNET 4G THÀNH CÔNG!".
3. File xử lý IPv6 (IPv6CP) - Nếu có
Nếu mạch mạng 4G hỗ trợ dải mạng IPv6, LwIP sẽ khởi chạy song song một tiến trình đàm phán IPv6CP thay vì chỉ chạy mỗi IPv4. Quá trình đó nằm ở một file riêng biệt.

Vị trí File xử lý IPv6CP: ~/esp/esp-idf/components/lwip/lwip/src/netif/ppp/ipv6cp.c
Tóm tắt chung: Chỉ chuỗi lệnh ATD*99# là do thư viện esp_modem/esp_modem_command_library.cpp chịu trách nhiệm. Còn lại quá trình trao đổi Handshake LCP, PAP, CHAP, và IPCP đều chìm bên dưới đáy của Framework, do các file .c thuần LwIP (lcp.c, pap.c, ipcp.c) tại thư mục components/lwip/lwip/src/netif/ppp/ tự động lo hậu sự/ppp/ giải quyết.
### 4.1 Thực hiện khởi động mạch cứng (Hard Power-On)
Cấp một xung điện `HIGH` kéo dài 1.5 giây để bật nút nguồn giả lập của mô-đun:
```c
gpio_set_level(MODEM_PWRKEY_PIN, 1); // Đóng Transistor kích chân PWRKEY
vTaskDelay(pdMS_TO_TICKS(1500));     // Chờ theo đúng Datasheet SIMCom
gpio_set_level(MODEM_PWRKEY_PIN, 0); // Nhả nút chờ
vTaskDelay(pdMS_TO_TICKS(5000));     // Chờ lõi HĐH của SIM khởi động hoàn tất
```

### 4.2 Vòng lặp chờ SIM sẵn sàng (Chống Crash kết nối)
Ngăn chặn thư viện PPP lao vào cấu hình mạng lúc thẻ SIM chưa kịp mồi, sử dụng lệnh thăm dò `AT+CPIN?`:
```c
while (1) {
    if (esp_modem_read_pin(dce, &pin_ok) == ESP_OK && pin_ok) {
        ESP_LOGI(TAG, "=> SIM Card OK! (Da nhan duoc the SIM)");
        break;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
}
```

### 4.3 Cơ chế Auto-APN (Phân tích PLMN - Public Land Mobile Network)
Phân tích phản hồi chuỗi từ lệnh `AT+COPS?` để quét chuỗi ký tự nhà mạng và tự động gắn Access Point Name (APN), thay vì Fix cứng vào code:
```c
if (strstr(cops_response, "VIETTEL") || strstr(cops_response, "45204")) {
    apn_to_use = "v-internet";
// ... Quét tương tự cho Mạng Mobifone (m-wap/45201) và Vinaphone/Wintel (m3-world/45202/m9-wintel).
```

### 4.4 Ủy quyền Mạng và lấy thông tin IP (Event Loop)
Hàm Callback của TCP/IP Stack được gọi khi giao thức PPP đàm phán thành công và trạm phát sóng cấp phát IP động:
```c
if (event_id == IP_EVENT_PPP_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "- Dia chi IP : " IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "- DNS Server : " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
}
```

## 5. Kết luận & Kết quả đầu ra
Quá trình khởi tạo PPPoS LwIP hoàn tất và đạt 100% mục tiêu cấu trúc giao thức Lớp 2/Lớp 3 (Internet Layer). Mạch bắt tay liên lạc hoàn hảo và báo cáo IP thành công:
```text
I (13069) pppos_main: Modem da ket noi PPPoS Server thang cong!
I (13079) pppos_main: IP          : 10.37.172.36
I (13089) pppos_main: Netmask     : 255.255.255.255
I (13089) pppos_main: Gateway     : 10.64.64.64
I (13119) pppos_main: Name Server1: 10.201.135.253
I (13129) pppos_main: DA CO MANG
```
Tiền đề này cho phép sử dụng thêm các kết nối Application Layer (HTTP/TCP/MQTT) một cách trơn tru, ổn định và tự động khôi phục mạng sau này trên đường truyền 4G.
