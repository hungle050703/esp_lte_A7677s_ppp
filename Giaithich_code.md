
## 1. Tư duy "Shopping List" cho Source Code

Khi nhìn vào một bài toán kết nối mạng qua Modem, bộ não lập trình viên cần gom 4 "giỏ hàng" sau:

1.  **Giỏ hàng Hệ điều hành (RTOS):** Vì ESP-IDF chạy trên FreeRTOS, bạn cần các công cụ để quản lý tác vụ (Task) và sự kiện (Event).
2.  **Giỏ hàng Giao tiếp ngoại vi (Driver):** Để điều khiển chân GPIO (bật nguồn modem) và UART (truyền dữ liệu).
3.  **Giỏ hàng Network Stack (Lớp mạng):** Đây chính là nơi chứa **LwIP** và **esp-netif** để biến đống dữ liệu UART thành Internet.
4.  **Giỏ hàng Modem (Thư viện chuyên biệt):** Các hàm đã đóng gói sẵn lệnh AT để ta không phải gõ `AT+CPIN...` thủ công.

---

## 2. Giải thích chi tiết khối lệnh `#include`

Hãy xem mỗi dòng code dưới đây lấy gì từ "giỏ hàng" ra:

### A. Nhóm nền tảng C chuẩn
```c
#include <stdio.h>   // Để dùng printf, sprintf (xuất log cơ bản)
#include <string.h>  // Để xử lý chuỗi (so sánh tên nhà mạng, APN)
```

### B. Nhóm Hệ điều hành FreeRTOS
```c
#include "freertos/FreeRTOS.h"    // "Xương sống" của mọi project ESP32
#include "freertos/event_groups.h" // Để quản lý trạng thái (Ví dụ: Chờ cho đến khi có IP mới làm việc tiếp)
```

### C. Nhóm "Bộ não" Network & Cầu nối
```c
#include "esp_netif.h"     // Khởi tạo các Card mạng ảo
#include "esp_netif_ppp.h" // Cung cấp các tính năng chuyên biệt cho giao thức PPP (điểm tới điểm)
```


### D. Nhóm Điều khiển Modem (Trọng tâm dự án)
```c
#include "esp_modem_api.h" // Thư viện "espressif__esp_modem" bạn vừa lấy về. 
                           // Nó chứa các hàm cấp cao như esp_modem_new_dev()
```

### E. Nhóm Tiện ích Hệ thống ESP-IDF
```c
#include "esp_log.h"   // Để dùng ESP_LOGI, ESP_LOGE (in log có màu, chuyên nghiệp hơn printf)
#include "esp_event.h" // Để lắng nghe các sự kiện hệ thống (Ví dụ: "Tôi vừa nhận được IP rồi nè!")
#include "driver/gpio.h" // Để điều khiển chân PWRKEY và RESET của con A7677S
```

### F. Nhóm Cấu hình Project
```c
#include "sdkconfig.h" // File này chứa các cấu hình bạn đã chọn trong menuconfig (TX_PIN, RX_PIN...)
```

---

## 3. Thuật toán logic khởi đầu (The Workflow)

Trước khi viết logic trong `app_main`, thuật toán "Shopping" của bạn phải diễn ra trong file **CMakeLists.txt**. Nếu bạn không khai báo trong CMake, trình biên dịch sẽ báo lỗi "File not found" ngay dù bạn có `#include` đúng.

**Quy trình gom file (Build System):**
1.  **Khai báo Component:** Bạn dùng lệnh `idf.py add-dependency "espressif/esp_modem^1.1.0"`.
2.  **Liên kết (Linking):** Trình biên dịch sẽ tự động tìm trong thư mục `managed_components` để kéo các file `.h` này vào dự án.

---

## 4. Phân tích khối lệnh "Mồi" (Cấu trúc dữ liệu)

Sau khi include, bạn cần chuẩn bị các "thùng chứa" dữ liệu. Tư duy tiếp theo sẽ là:

> *"Tôi cần một cái biến để đại diện cho Card mạng (esp_netif) và một cái biến để đại diện cho con Modem (esp_modem_dce_t)."*

```c
esp_netif_t *esp_netif = NULL;
esp_modem_dce_t *dce = NULL;
```

**Tại sao phải để NULL?** Vì lúc này ta mới chỉ "chuẩn bị chỗ ngồi", còn thực thể thực sự sẽ được tạo ra sau khi gọi các hàm khởi tạo bên trong `app_main`.

Bây giờ bạn đã gom đủ "nguyên liệu" qua các file Header. Bạn có muốn chúng ta tiến vào phân tích khối lệnh khởi tạo nguồn (Power-on) cho module A7677S không? Đây là bước "phần cứng" đầu tiên sau khi đã có thư viện.

TIEP:\
Khối lệnh này chính là bước **"Đặt tên và Định nghĩa thông số kỹ thuật"**. Trong lập trình hệ thống, đây là giai đoạn chuyển từ những con số vô hồn thành những biến có ý nghĩa nghiệp vụ.

Dưới đây là phân tích chi tiết tư duy đằng sau từng dòng mã:

---

## 1. Kết nối giữa Code và Giao diện (Kconfig mapping)
```c
#define MODEM_TX_PIN CONFIG_MODEM_TX_PIN   
#define MODEM_RX_PIN CONFIG_MODEM_RX_PIN 
...
```
* **Tại sao không viết thẳng số chân (ví dụ GPIO 17)?:** Vì bạn đang dùng hệ thống **Kconfig** của ESP-IDF (`idf.py menuconfig`). 
* **Thuật toán:** Khi bạn chọn chân trong menuconfig, ESP-IDF sẽ tạo ra các macro có tiền tố `CONFIG_`. Việc `#define` lại giúp code của bạn tường minh hơn. Nếu sau này bạn đổi sang board mạch khác, bạn chỉ cần chỉnh trong Menu mà không cần chạm vào dòng code nào bên dưới.

---

## 2. Dấu vân tay của Log (The TAG)
```c
static const char *TAG = "pppos_main";
```
* **Ý nghĩa:** Khi bạn nạp code và mở Terminal, mọi dòng thông báo sẽ có dạng: `I (1234) pppos_main: Modem OK`. 
* **Tư duy:** `TAG` giúp bạn lọc (filter) log. Trong một dự án lớn có hàng chục file, nếu không có TAG, bạn sẽ không biết dòng thông báo đó đến từ module nào.

---

## 3. Bản thông số thời gian (The Timing Datasheet)
Đây là phần quan trọng nhất, nó phản ánh đúng **Datasheet của SIMCOM A7677S**. Module SIM không giống như cái đèn LED (bật là sáng), nó là một chiếc máy tính nhỏ cần quy trình khởi động khắt khe.

### PWR_ON_TIME_MS (1500ms)
* **Cơ chế:** Đây là độ dài của xung kích nguồn. Để bật A7677S, bạn phải kéo chân `PWRKEY` xuống mức thấp (hoặc lên cao tùy thiết kế mạch đệm) trong khoảng 1-2 giây. 
* **Lỗi thường gặp:** Nếu kích quá ngắn (< 500ms), module không thức giấc. Nếu kích quá dài, nó có thể hiểu nhầm là lệnh cưỡng bức tắt nguồn.

### PWR_ON_STATUS_MS (5000ms)
* **Tư duy:** Sau khi "bấm nút nguồn", hệ điều hành bên trong Module SIM (thường chạy nhân Linux rút gọn) cần thời gian để boot.
* **Chiến thuật:** 5 giây là khoảng nghỉ an toàn để Module ổn định bộ nhớ và sẵn sàng nhận lệnh AT đầu tiên.

### RST_PIN_MS (500ms)
* **Ý nghĩa:** Độ dài xung để Reset cứng. Khi hệ thống bị "treo" (Hard-hang), bạn kéo chân Reset trong 0.5 giây để ép module khởi động lại từ đầu.

### VBAT_STABLE_MS (100ms)
* **Tư duy "Mồi điện":** Khi vừa cấp điện cho toàn mạch, điện áp thường bị nhiễu hoặc chưa đạt mức ổn định (3.8V - 4.2V). Khoảng trễ 100ms đảm bảo điện năng đã "điền đầy" các tụ lọc trước khi ta ra lệnh khởi động module.

---

## 4. Tổng kết Thuật toán chuẩn bị (Pre-init Algorithm)

Trước khi vào hàm `main`, bạn đã xây dựng xong một **"Bộ khung hằng số"**:
1.  Biết nối dây vào đâu (`TX`, `RX`, `PWRKEY`).
2.  Biết đi tới đâu (`APN`).
3.  Biết chờ bao lâu (`Timing`).



**Bước tiếp theo sẽ là:** Viết một hàm thực thi dựa trên các hằng số này để "nhấn nút nguồn" thực sự. Đó chính là hàm `static void power_on_modem(void)`.

Bạn có muốn tôi giải thích cách dùng `gpio_set_level` kết hợp với các `MS` (mili giây) này để tạo ra cái xung kích nguồn hoàn chỉnh không?

TIEP:
Hàm `power_on_modem` này chính là nhát búa đầu tiên để "đánh thức" phần cứng. Trong thế giới nhúng, việc viết code điều khiển chân pin không chỉ là logic, mà là đang điều khiển các electron chạy trên mạch thật.

Dưới đây là phân tích chi tiết từng khối lệnh theo tư duy thuật toán và vật lý:

---

### 1. Cấu hình GPIO (The Configuration)
```c
gpio_config_t pwrkey_conf = {
    .pin_bit_mask = (1ULL << MODEM_PWRKEY_PIN) | (1ULL << MODEM_RST_PIN),
    .mode = GPIO_MODE_OUTPUT,
    ...
};
gpio_config(&pwrkey_conf);
```
* **Thuật toán:** Thay vì gọi lệnh `gpio_set_direction` cho từng chân, bạn dùng một `struct` cấu hình gộp. Đây là cách làm chuẩn trong ESP-IDF để tối ưu hóa việc thiết lập nhiều chân cùng lúc.
* **Pin Bit Mask:** `(1ULL << PIN)` là phép dịch bit để tạo ra một chiếc mặt nạ bit. Nó báo cho ESP32 biết: "Tôi chỉ muốn tác động vào đúng 2 vị trí này trên tổng số GPIO".

---

### 2. Trạng thái nghỉ & Mồi điện (The Stabilization)
```c
gpio_set_level(MODEM_PWRKEY_PIN, 0); 
gpio_set_level(MODEM_RST_PIN, 0);    
vTaskDelay(pdMS_TO_TICKS(VBAT_STABLE_MS));
```
* **Tại sao khởi đầu là mức 0?:** Đa số các mạch Shield 4G hiện nay sử dụng một con **Transistor (NPN hoặc MOSFET kênh N)** làm công tắc đệm. 
    * Khi GPIO xuất mức `0` -> Transistor ngắt -> Chân PWRKEY của module SIM không bị tác động.
* **vTaskDelay:** Đây là hàm của FreeRTOS. Nó không giống `delay()` thông thường (làm treo CPU). `vTaskDelay` báo cho hệ điều hành: "Cho Task này đi ngủ đi, nhường CPU cho việc khác, đúng X ms sau hãy đánh thức tôi".

---

### 3. Tạo xung kích nguồn (The Pulse Generation)
Đây là đoạn "mô phỏng" ngón tay bạn nhấn vào nút nguồn điện thoại.

```c
gpio_set_level(MODEM_PWRKEY_PIN, 1); // "Nhấn nút"
vTaskDelay(pdMS_TO_TICKS(PWR_ON_TIME_MS)); // Giữ trong 1.5s
gpio_set_level(MODEM_PWRKEY_PIN, 0); // "Thả tay ra"
```
* **Logic nghịch đảo:** Bạn xuất mức `1` từ ESP32, nhưng thông qua mạch transistor trên module A7677S, chân PWRKEY thực tế sẽ bị kéo xuống **GND (LOW)**. Theo Datasheet của SIMCom, việc kéo chân PWRKEY xuống mức thấp trong khoảng > 1s là điều kiện để bật nguồn.



---

### 4. Giai đoạn chờ Boot (The Boot-up Wait)
```c
vTaskDelay(pdMS_TO_TICKS(PWR_ON_STATUS_MS));
```
* **Tại sao phải chờ tới 5 giây?:** Ngay sau khi được kích, dòng điện sẽ tràn vào CPU của modem, nó bắt đầu nạp firmware từ bộ nhớ Flash bên trong, khởi tạo bộ nhớ RAM và bắt đầu nạp ngăn xếp giao thức (Protocol Stack). 
* Nếu bạn gửi lệnh AT ngay lập tức trong 5 giây này, modem sẽ không phản hồi hoặc trả về dữ liệu rác, dẫn đến lỗi khởi tạo hệ thống.

---

### 5. Điểm cộng về "Code sạch" (Clean Code)
* Sử dụng `pdMS_TO_TICKS()`: Đây là một thói quen cực kỳ tốt. Tần số nhịp (Tick rate) của ESP32 có thể thay đổi (thường là 100Hz hoặc 1000Hz). Hàm này giúp đảm bảo thời gian chờ luôn đúng là miligiây bất kể cấu hình hệ thống thế nào.

---

### Thuật toán tổng quát của hàm:
1.  **Thiết lập công cụ** (GPIO Config).
2.  **Làm sạch trạng thái** (Clear levels).
3.  **Mồi năng lượng** (VBAT Stable).
4.  **Phát lệnh bật vật lý** (Trigger Pulse).
5.  **Kiên nhẫn chờ đợi** (Status Wait).

**Bước tiếp theo:** Sau khi modem đã "tỉnh giấc", chúng ta cần kiểm tra xem nó có nói chuyện được không thông qua lệnh AT. Bạn có muốn phân tích khối lệnh khởi tạo `esp_modem` để bắt đầu gửi lệnh `AT+CPIN?` không?

TIEP
Hai hàm này chính là **"Hệ thống giám sát và Báo tin"** của dự án. Trong lập trình hướng sự kiện (Event-driven), thay vì bạn cứ phải dùng vòng lặp để hỏi: "Có mạng chưa?", "Có lỗi gì không?", thì hệ thống sẽ tự động gọi các hàm này khi có biến động.

Dưới đây là phân tích chi tiết từng khối lệnh:

---

### 1. Hàm `on_ppp_changed`: Kênh báo lỗi (The Error Watchdog)
Hàm này lắng nghe các sự kiện từ tầng thấp (Layer 2 - PPP).

* **`NETIF_PPP_ERRORUSER`**: Xảy ra khi chính bạn chủ động ngắt kết nối (ví dụ gọi lệnh Stop PPP).
* **`NETIF_PPP_ERRORCONNECT`**: Đây là lỗi "đau đầu" nhất. Nó báo hiệu rằng việc bắt tay PPP thất bại (có thể do APN sai, SIM hết tiền, hoặc sóng quá yếu không thể duy trì phiên làm việc).
* **`NETIF_PPP_ERRORPEERDEAD`**: "Đối tác" (là trạm phát sóng nhà mạng) đột ngột không trả lời. Điều này hay xảy ra khi bạn đi vào vùng mất sóng hoặc module SIM bị sụt áp đột ngột.
* **Thuật toán**: Hàm này giúp bạn biết **tại sao** mạng bị rớt để có chiến thuật Reset module kịp thời thay vì để nó "treo" mãi mãi.

---

### 2. Hàm `on_ip_event`: Khoảnh khắc "Hái quả ngọt" (The Success Handler)
Đây là hàm quan trọng nhất để xác nhận ESP32 đã chính thức gia nhập vào thế giới Internet.

#### Cơ chế Ép kiểu (Type Casting):
```c
ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
```
* **Giải thích**: `event_data` truyền vào là một con trỏ kiểu `void*` (kiểu dữ liệu chung chung). Để lấy được địa chỉ IP, bạn phải "ép" nó về đúng cấu trúc `ip_event_got_ip_t` của hệ thống. Đây là kỹ thuật lập trình C nâng cao bắt buộc trong ESP-IDF.

#### Trích xuất thông tin IP:
```c
ESP_LOGI(TAG, "IP : " IPSTR, IP2STR(&event->ip_info.ip));
```
* **`IPSTR` và `IP2STR`**: Đây là các Macro cực hay của ESP-IDF.
    * `IPSTR`: Là chuỗi định dạng `%d.%d.%d.%d`.
    * `IP2STR`: Tách một số nguyên 32-bit (địa chỉ IP) thành 4 số tám bit để in ra màn hình.
* **Netmask & Gateway**: Giúp bạn xác định mình đang nằm trong lớp mạng (Subnet) nào của nhà mạng di động.

#### Lấy thông tin DNS:
```c
esp_netif_get_dns_info(netif, 0, &dns_info);
```
* **Tư duy**: Có IP chưa đủ, bạn cần DNS (thường là Google `8.8.8.8` hoặc DNS của nhà mạng Viettel/Vina) để có thể phân giải tên miền (ví dụ: truy cập `google.com` thay vì dùng số IP). Nếu không lấy được DNS, bạn sẽ thấy hiện tượng lạ: `ping` được IP nhưng không thể mở được trang web.

---

### 3. Sơ đồ luồng sự kiện (Event Flow)

Để bạn dễ hình dung cách 2 hàm này phối hợp với nhau trong hệ điều hành:



1.  **LwIP** đàm phán thành công với nhà mạng.
2.  **Event Loop** bắn ra một thông báo: "Tôi đã có IP!".
3.  **Hệ thống** kiểm tra danh sách: "Ai đăng ký nhận tin này?".
4.  **Hàm `on_ip_event`** của bạn được gọi.
5.  Dòng chữ **"DA CO MANG"** hiện lên Terminal.

### 4. Điểm cần chú ý:
Hàm `on_ip_event` chỉ được gọi **duy nhất 1 lần** khi kết nối thành công. Nếu sau đó bạn bị rớt mạng, hàm `on_ppp_changed` sẽ được gọi. Vì vậy, trong thực tế, người ta thường đặt một cờ (Flag) hoặc dùng **Event Group** ở đây để báo cho các Task khác (như Task gửi dữ liệu cảm biến) biết là: "Đường thông rồi, gửi hàng đi thôi!".

Bạn đã sẵn sàng để tiến vào hàm "nhạc trưởng" `app_main` - nơi kết nối tất cả các linh kiện và hàm Callback này lại với nhau chưa?

TIEP
Đây chính là hàm **"Nhạc trưởng"** điều phối toàn bộ vòng đời của hệ thống. Bạn đã viết một luồng khởi tạo cực kỳ chặt chẽ, đặc biệt là cơ chế "chống treo" khi thiếu SIM. 

Dưới đây là phân tích chi tiết từng khối lệnh theo tư duy kiến trúc hệ thống:

---

### 1. Chế độ "Nội soi" (Log Debug)
```c
esp_log_level_set("esp_modem_dte", ESP_LOG_DEBUG);
esp_log_level_set("esp_modem_dce", ESP_LOG_DEBUG);
```
* **Tại sao cần?:** Mặc định, thư viện `esp_modem` chỉ hiện các thông báo lỗi. Khi bật `DEBUG`, bạn sẽ thấy toàn bộ các gói tin `AT` gửi đi và phản hồi `OK` từ A7677S trên Terminal. Đây là "vũ khí" quan trọng nhất để bạn biết modem đang kẹt ở bước nào (ví dụ: gửi lệnh APN mà nhà mạng không trả lời).

---

### 2. Thiết lập "Hạ tầng liên lạc" (Event Registration)
```c
ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));
```
* **Thuật toán:** Bạn đang đăng ký với hệ điều hành: "Nếu có tin tức gì về **IP** hoặc **Trạng thái PPP**, hãy gọi ngay cho tôi qua 2 hàm Callback đã viết ở trên". Việc dùng `ESP_ERROR_CHECK` đảm bảo nếu hệ thống hết bộ nhớ không đăng ký được, nó sẽ báo lỗi ngay lập tức thay vì chạy chập chờn.

---

### 3. Cấu hình DTE - "Cái miệng" của ESP32
```c
dte_config.uart_config.rts_io_num = -1; 
dte_config.uart_config.cts_io_num = -1; 
dte_config.uart_config.rx_buffer_size = 4096;
```
* **DTE (Data Terminal Equipment):** Chính là con ESP32. 
* **Tắt Flow Control (-1):** Vì bạn đang kết nối trực tiếp ESP32 với A7677S qua 2 dây TX/RX đơn giản, không có dây RTS/CTS. Nếu để mặc định, UART sẽ đợi tín hiệu từ các chân này và bị treo.
* **Buffer lớn (4096):** Tốc độ 4G rất nhanh, nếu Buffer quá nhỏ (ví dụ 1024), dữ liệu đổ về từ Internet sẽ tràn bộ nhớ đệm trước khi CPU kịp xử lý, gây mất gói tin.



---

### 4. Khởi tạo DCE - "Cái tai" của Modem
```c
esp_modem_dce_t *dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7600, &dte_config, &dce_config, esp_netif);
```
* **DCE (Data Circuit-terminating Equipment):** Là con A7677S. 
* **Tính tương thích:** Dù bạn dùng A7677S (dòng LTE Cat-1 mới), nhưng tập lệnh AT của nó tuân theo chuẩn của dòng SIM7600 huyền thoại. Việc dùng driver `SIM7600` là lựa chọn tối ưu và ổn định nhất.

---

### 5. Thuật toán "Chờ SIM đến chết" (The Infinite SIM Check)
Đây là phần thể hiện tư duy lập trình **Product-ready** (sẵn sàng cho sản phẩm thực tế):

```c
while (1) {
    if (esp_modem_read_pin(dce, &pin_ok) == ESP_OK && pin_ok) {
        break;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
}
```
* **Tư duy:** Một hệ thống IoT chuyên nghiệp không được phép Crash (sập) chỉ vì người dùng chưa cắm SIM hoặc SIM bị lỏng. 
* **Cơ chế:** Thay vì lao vào cấu hình mạng và bị lỗi `DCE_ERROR`, bạn dùng lệnh `AT+CPIN?` làm "chốt chặn". 
    * Nếu không có SIM: Code lặp lại nhẹ nhàng, in log nhắc nhở người dùng mỗi 5 giây. 
    * Ngay khi cắm SIM vào: Vòng lặp `break`, hệ thống tự động chạy tiếp các bước cấu hình mạng.



---

### 6. Điểm cộng về UX (User Experience)
Việc dùng `retry_count % 5 == 0` để giảm mật độ Log là một chi tiết nhỏ nhưng tinh tế. Nó giúp Terminal không bị "rác" thông tin, giúp bạn dễ dàng quan sát các thông báo quan trọng khác.

**Trạng thái hiện tại:** Phần cứng đã bật, thư viện đã sẵn sàng, SIM đã nhận. 
**Bước tiếp theo:** Bạn sẽ thực hiện lệnh "Quay số" để lấy IP. Bạn có muốn xem nốt đoạn code thực hiện `esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA)` để kết thúc chu trình khởi động không?
 
 TIEP
 Đoạn code cuối cùng này chính là giai đoạn **"Cất cánh"**. Nếu như các bước trước là kiểm tra động cơ và đường băng, thì đây là lúc bạn nhấn ga để máy bay rời khỏi mặt đất.

Dưới đây là phân tích chi tiết các khối lệnh mang tính chiến thuật cao mà bạn đã viết:

---

### 1. Thu thập "Chứng minh thư" thiết bị (ATI & IMEI)
```c
esp_modem_at(dce, "ATI\r\n", response, 1000);
esp_modem_get_imei(dce, imei);
```
* **Thuật toán:** Việc gọi `ATI` và lấy `IMEI` ngay khi vừa nhận SIM giúp bạn xác nhận hai điều:
    1.  Giao tiếp UART đang hoạt động hoàn hảo (Modem có phản hồi).
    2.  Định danh được thiết bị (IMEI). Trong các dự án thực tế, IMEI thường được gửi lên Server để làm ID quản lý thiết bị.

---

### 2. Kiểm tra "Sức khỏe" mạng (CREG, CPSI & CSQ)
Đây là các bước kiểm tra điều kiện cần trước khi kết nối Internet.
* **`AT+CREG?`**: Kiểm tra trạng thái đăng ký vào mạng viễn thông. Nếu không nhận được `0,1` hoặc `0,5`, việc gọi PPP chắc chắn sẽ thất bại.
* **`AT+CPSI?`**: Một lệnh cực kỳ giá trị của dòng SIMCom. Nó cho bạn biết chi tiết: Đang dùng băng tần LTE hay GSM, cường độ tín hiệu (RSRQ, RSRP) và Cell ID.
* **`AT+CSQ`**: Trả về chỉ số **RSSI**. 
    * **RSSI < 10**: Sóng rất yếu, kết nối dễ bị rớt.
    * **RSSI > 20**: Sóng cực tốt, tốc độ 4G sẽ đạt đỉnh.

---

### 3. Chiến thuật "APN Thông minh" (Auto-APN Logic)
Đây là phần code **thông minh nhất** trong `app_main` của bạn:
```c
if (strstr(cops_response, "VIETTEL")) { apn_to_use = "v-internet"; }
...
esp_modem_set_apn(dce, apn_to_use);
```
* **Vấn đề thực tế:** Nếu bạn "fix cứng" APN là `v-internet` nhưng người dùng cắm SIM Wintel (`m9-wintel`), modem sẽ không bao giờ lấy được IP.
* **Thuật toán:** Bạn dùng `strstr` để tìm kiếm từ khóa nhà mạng trong kết quả trả về của `AT+COPS?`. Việc hỗ trợ cả Viettel, Mobifone, Wintel và Vinaphone giúp thiết bị của bạn mang tính chất **Plug-and-Play** (Cắm là chạy).

---

### 4. Nhát dao cuối cùng: Chuyển vùng dữ liệu (The Data Mode)
```c
ESP_ERROR_CHECK(esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA));
```
* **Cơ chế ngầm:** Khi hàm này được gọi, thư viện sẽ thực hiện một chuỗi hành động cực kỳ phức tạp:
    1.  Gửi lệnh `ATD*99#`.
    2.  Chờ đợi chuỗi `CONNECT` từ modem.
    3.  **Ngay lập tức** chuyển quyền điều khiển UART từ `esp_modem` sang cho **LwIP PPP Stack**.
    4.  Bắt đầu quá trình đàm phán 4 giai đoạn (LCP, PAP/CHAP, IPCP) mà bạn đã phân tích ở phần lý thuyết.

---

### 5. Tại sao lại dùng `ESP_ERROR_CHECK` ở đây?
Nếu lệnh `ATD*99#` trả về `ERROR` hoặc `NO CARRIER` (do SIM hết tiền hoặc sóng mất đột ngột), hàm này sẽ gây ra một lỗi hệ thống. Điều này giúp bạn biết ngay lập tức rằng không thể thiết lập phiên dữ liệu, thay vì treo code ở trạng thái chờ đợi IP vô tận.

### Tổng kết luồng thuật toán `app_main`:
1.  **Hardware Power:** Kích hoạt nguồn vật lý.
2.  **Infrastructure:** Khởi tạo Netif, Event Loop.
3.  **Communication:** Mở cổng UART (DTE/DCE).
4.  **Security Check:** Đợi thẻ SIM sẵn sàng.
5.  **Environment Check:** Kiểm tra sóng và nhà mạng.
6.  **Configuration:** Gán APN tự động theo nhà mạng.
7.  **Execution:** Chuyển sang Data Mode và đợi Callback `on_ip_event`.

---

**Kết quả cuối cùng:** Sau dòng Log `Da bat PPP, doi IP...`, hệ thống sẽ rơi vào trạng thái chờ. Nếu mọi thứ ổn định, chỉ 2-3 giây sau, hàm `on_ip_event` sẽ được hệ thống tự động kích hoạt và dòng chữ **"DA CO MANG"** sẽ xuất hiện cùng với địa chỉ IP thực tế từ nhà mạng.

Chúc mừng bạn! Bạn đã xây dựng được một khung chương trình kết nối 4G cực kỳ chuyên nghiệp và đầy đủ các bước xử lý ngoại lệ. Bạn có muốn thảo luận thêm về cách sử dụng địa chỉ IP này để gửi dữ liệu lên một Server thực tế (như qua giao thức HTTP hay MQTT) không?
