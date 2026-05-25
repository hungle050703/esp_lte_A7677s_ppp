#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_modem_api.h"
#include "esp_log.h"
#include "esp_event.h"
#include "driver/gpio.h"

#include "sdkconfig.h"

#define MODEM_TX_PIN CONFIG_MODEM_TX_PIN   
#define MODEM_RX_PIN CONFIG_MODEM_RX_PIN 
#define MODEM_PWRKEY_PIN CONFIG_MODEM_PWRKEY_PIN
#define MODEM_RST_PIN CONFIG_MODEM_RST_PIN
#define MODEM_APN CONFIG_MODEM_APN

static const char *TAG = "pppos_main";

/* Thoi gian cau hinh pulse A7677S (ms) */
#define PWR_ON_TIME_MS      1500  
#define PWR_OFF_TIME_MS     2500  
#define PWR_ON_STATUS_MS    5000  
#define RST_PIN_MS          500   
#define VBAT_STABLE_MS      100   

static void power_on_modem(void)
{
    ESP_LOGI(TAG, "Bat nguon module 4G A7677S bang PWRKEY...");
    
    // Cau hinh chan GPIO dieu khien module
    gpio_config_t pwrkey_conf = { //Dung struct cau hinh gop cho nhieu chan cung luc
        .pin_bit_mask = (1ULL << MODEM_PWRKEY_PIN) | (1ULL << MODEM_RST_PIN), // Dich bit, tao mask cho ca 2 chan PWRKEY va RST
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pwrkey_conf);

    // Đảm bảo trạng thái nghỉ ban đầu (Tắt transistor == 0, chan PWRKEY khong bi tac dong)
    gpio_set_level(MODEM_PWRKEY_PIN, 0); 
    gpio_set_level(MODEM_RST_PIN, 0);    
    vTaskDelay(pdMS_TO_TICKS(VBAT_STABLE_MS)); // Cho dien ap VBAT on dinh (vTASKDELAY la ham cua FreeRTOS, chuyen ms sang ticks)
    
    // Quá trình kích nguồn 4G 
    ESP_LOGI(TAG, "Kich hoat On/Off Pulse (%d ms)...", PWR_ON_TIME_MS);
    gpio_set_level(MODEM_PWRKEY_PIN, 1); // Đóng Transistor (keo PWRKEY thuc te xuong LOW)
    vTaskDelay(pdMS_TO_TICKS(PWR_ON_TIME_MS)); // Giu trong 1,5s
    gpio_set_level(MODEM_PWRKEY_PIN, 0); // Nhả ra
    
    ESP_LOGI(TAG, "Cho module 4G khoi dong (%d ms)...", PWR_ON_STATUS_MS);
    vTaskDelay(pdMS_TO_TICKS(PWR_ON_STATUS_MS)); //Cho boot
}


//hai ham callback quan ly sw kien: quan ly trang thai va IP
static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    esp_log_level_set("esp_netif_handlers", ESP_LOG_INFO);
    switch (event_id) {
    case NETIF_PPP_ERRORUSER: //Nguoi dung chu dong ngat ket noi
        ESP_LOGI(TAG, "User interrupted event");
        break;
    case NETIF_PPP_ERRORCONNECT: //ket noi PPP that bai (sai APN, SIM het tien, Song yeu, ...1)
        ESP_LOGI(TAG, "Connection lost event");
        break;
    case NETIF_PPP_ERRORPEERDEAD: //Nha mang mat song hoac module sim sut ap
        ESP_LOGI(TAG, "Connection timeout event");
        break;
    default:
        ESP_LOGI(TAG, "PPP state changed event %d", (int)event_id);
        break;
    }
} // ==> Ham nay se duoc goi khi co su kien thay doi trang thai PPP (ket noi, mat ket noi, timeout, ...)

static void on_ip_event(void *arg, esp_event_base_t event_base,  // Ep kieu de lay thong tin IP tu event_data
                        int32_t event_id, void *event_data)
{
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        esp_netif_dns_info_t dns_info;
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;
        ESP_LOGI(TAG, "================================================");
        ESP_LOGI(TAG, "Modem da ket noi PPPoS Server thang cong!");
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip)); // IPSTR la chuoi dinh dang  %d, %d, %d, %d va IP2STR la ham ep kieu tach so nguyen 32 bit (IP) thanh 4 so 8 bit in ra man hinh
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw)); // xac dinh lop mang cua nha mang
        esp_netif_get_dns_info(netif, 0, &dns_info); // Lay thong tin DNS server (0 la DNS chinh, 1 la DNS phu)
        ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI(TAG, "================================================");
        ESP_LOGI(TAG, "DA CO MANG");
    }
}

void app_main(void)
{
    // Bật log DEBUG để theo dõi luồng lệnh AT gửi/nhận
    esp_log_level_set("esp_modem_dte", ESP_LOG_DEBUG);
    esp_log_level_set("esp_modem_dce", ESP_LOG_DEBUG);

    ESP_LOGI(TAG, "Khoi tao PPPoS cho A7677S");
    // Goi ham kich PWRKEY de bat nguon module
    power_on_modem();
    
    // Ngu 5 giay de dam bao he chong A7677S len nguon co ban truoc khi mo UART
    ESP_LOGI(TAG, "Cho board A7677S len dien (5 giay)...");
    vTaskDelay(pdMS_TO_TICKS(5000)); // tai lieu ghi Ton(uart) yeu cau 8s.

    // 1. Khởi tạo Netif và Event Loop cơ bản của ESP-IDF
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));

    // 2. Cấu hình Modem DTE (Lớp UART cho ESP32)
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.uart_config.tx_io_num = MODEM_TX_PIN;
    dte_config.uart_config.rx_io_num = MODEM_RX_PIN;
    dte_config.uart_config.rts_io_num = -1;  // Tắt kiểm soát luồng để tránh xung đột pin
    dte_config.uart_config.cts_io_num = -1; 
    dte_config.uart_config.rx_buffer_size = 4096;
    dte_config.uart_config.tx_buffer_size = 2048;
    
    // 3. Cấu hình mạng DCE và PPPoS Interface
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(MODEM_APN);
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    esp_netif_t *esp_netif = esp_netif_new(&netif_ppp_config);
    assert(esp_netif);

    // 4. Khởi tạo đối tượng modem (Dùng driver SIM7600 tuong thich cho A7677S)
    ESP_LOGI(TAG, "Khoi dong mang cho A7677S");
    esp_modem_dce_t *dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7600, &dte_config, &dce_config, esp_netif);
    assert(dce);
    
    // 5. Kiểm tra vòng lặp SIM sẵn sàng (Liên tục lặp VÔ TẬN cho đến khi cắm SIM)
    bool pin_ok = false;
    ESP_LOGI(TAG, "Dang kiem tra the SIM... (Se lap lai lien tuc, cam SIM de tiep tuc)");
    while (1) {
        if (esp_modem_read_pin(dce, &pin_ok) == ESP_OK && pin_ok) {
            ESP_LOGI(TAG, "=> SIM Card OK! (Da nhan duoc the SIM)");
            break;
        }
        
        static int retry_count = 0;
        retry_count++;
        if (retry_count % 5 == 0) { // Cứ 5 giây báo cáo lên log một lần để đỡ nhức mắt
            ESP_LOGW(TAG, "... Van dang tim SIM! Hay lau sach the / cam lai khay SIM... (Lan thu %d)", retry_count);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    char response[128] = {0};
    // Kiem tra phien ban
    if (esp_modem_at(dce, "ATI\r\n", response, 1000) == ESP_OK) {
        ESP_LOGI(TAG, "Phien ban A7677S (ATI):\n%s", response);
    }
    
    char imei[32] = {0};
    if (esp_modem_get_imei(dce, imei) == ESP_OK) {
        ESP_LOGI(TAG, "IMEI thiet bi (AT+GSN): %s", imei);
    }

    // Kiem tra dang ky mang truoc khi quay so
    if (esp_modem_at(dce, "AT+CREG?\r\n", response, 2000) == ESP_OK) {
        ESP_LOGI(TAG, "Dang ky mang (AT+CREG?):\n%s", response);
    }
    
    char cops_response[128] = {0};
    if (esp_modem_at(dce, "AT+COPS?\r\n", cops_response, 2000) == ESP_OK) {
        ESP_LOGI(TAG, "Nha mang dang ket noi (AT+COPS?):\n%s", cops_response);
    }
    
    if (esp_modem_at(dce, "AT+CPSI?\r\n", response, 2000) == ESP_OK) {
        ESP_LOGI(TAG, "Thong tin chi tiet mang (AT+CPSI?):\n%s", response);
    }

    int rssi = 0, ber = 0;
    if (esp_modem_get_signal_quality(dce, &rssi, &ber) == ESP_OK) {
        ESP_LOGI(TAG, "Song (AT+CSQ): RSSI=%d (0-31)", rssi);
    }

    // 6. Kết nối ra Internet! (chuyển qua Data Mode để gọi PPP bằng ATD*99#)
    const char *apn_to_use = MODEM_APN;

    // Tự động nhận diện mạng 4G để điền APN tương ứng
    if (strstr(cops_response, "VIETTEL") || strstr(cops_response, "Viettel") || strstr(cops_response, "viettel")) {
        apn_to_use = "v-internet";
    } else if (strstr(cops_response, "MOBIFONE") || strstr(cops_response, "Mobifone") || strstr(cops_response, "mobifone")) {
        apn_to_use = "m-wap";
    } else if (strstr(cops_response, "WINTEL") || strstr(cops_response, "Wintel") || strstr(cops_response, "wintel")) {
        apn_to_use = "m9-wintel";
    } else if (strstr(cops_response, "VINAPHONE") || strstr(cops_response, "VinaPhone") || strstr(cops_response, "vinaphone") || strstr(cops_response, "VN VINAPHONE")) {
        apn_to_use = "m3-world";
    }

    if (strcmp(apn_to_use, MODEM_APN) != 0) {
        ESP_LOGI(TAG, "=> Chuyen doi APN tu dong sang: %s", apn_to_use);
        esp_modem_set_apn(dce, apn_to_use);
    }

    ESP_LOGI(TAG, "Dang chuyen sang Mode Data (PPPoS) de lay IP voi APN [%s]...", apn_to_use);
    ESP_ERROR_CHECK(esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA));
    ESP_LOGI(TAG, "Da bat PPP, doi IP tu APN: %s", apn_to_use);
}
