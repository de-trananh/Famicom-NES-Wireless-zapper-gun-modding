// RX MAC: 3C:0F:02:29:71:34
#include <WiFi.h>
#include <esp_wifi.h>

uint8_t allowedMAC[6] = {0x3C, 0x0F, 0x02, 0x29, 0x33, 0x38};

// Callback chạy bằng ngắt cứng khi phát hiện có sóng 802.11 đi qua
void IRAM_ATTR promiscuous_rx_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_DATA) return; // Chỉ xử lý gói Data

  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  uint8_t* payload = pkt->payload;

  // Lọc nhanh theo MAC nguồn của TX (nằm ở offset byte thứ 10 trong Header 802.11)
  if (memcmp(&payload[10], allowedMAC, 6) != 0) return;

  // Payload dữ liệu bắt đầu ngay sau Header 24 bytes (ở offset 24 và 25)
  if (payload[24]) REG_WRITE(GPIO_OUT_W1TS_REG, (1 << 0));
  else             REG_WRITE(GPIO_OUT_W1TC_REG, (1 << 0));

  if (payload[25]) REG_WRITE(GPIO_OUT_W1TS_REG, (1 << 1));
  else             REG_WRITE(GPIO_OUT_W1TC_REG, (1 << 1));
}

void setup() {
  setCpuFrequencyMhz(160);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  // Bật chế độ Promiscuous (Bắt toàn bộ gói tin thô trên không)
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(promiscuous_rx_cb);

  pinMode(0, OUTPUT);
  pinMode(1, OUTPUT);
  digitalWrite(0, LOW);delay(100);digitalWrite(0, HIGH);
  digitalWrite(1, LOW);delay(100);digitalWrite(1, HIGH);
}

void loop() {
  // Loop để trống
}