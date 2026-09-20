// MAC TX: 3C:0F:02:29:33:38
#include <WiFi.h>
#include <esp_wifi.h>

#define PIN_TRIGGER 0 // Pin 0: Cò súng (Trigger)
#define PIN_SENSOR  1 // Pin 1: Cảm biến quang (Light Sensor)

// Định dạng Header khung dữ liệu chuẩn IEEE 802.11 (24 Bytes)
typedef struct {
  uint16_t frame_control; // 0x0008: Data Frame
  uint16_t duration;
  uint8_t  da[6];         // Địa chỉ nhận: Broadcast (FF:FF:FF:FF:FF:FF)
  uint8_t  sa[6];         // Địa chỉ phát: MAC TX
  uint8_t  bssid[6];      // BSSID
  uint16_t seq_ctrl;      // Số thứ tự gói tin
} __attribute__((packed)) wifi_header_t;

typedef struct {
  wifi_header_t header;
  uint8_t payload[2];     // [0] = Trigger (Pin 0), [1] = Sensor (Pin 1)
} __attribute__((packed)) raw_packet_t;

raw_packet_t packet;
uint8_t txMAC[6] = {0x3C, 0x0F, 0x02, 0x29, 0x33, 0x38};

volatile bool needSend = false;
volatile uint8_t trigger_state = 1;
volatile uint8_t sensor_state = 1;
uint16_t seq_counter = 0;

// Ngắt phần cứng chạy tức thì (<1us) khi Cò hoặc Cảm biến đổi trạng thái
void IRAM_ATTR ISR_Pin_Change() {
  uint32_t gpio_in = REG_READ(GPIO_IN_REG);
  trigger_state = (gpio_in & (1 << PIN_TRIGGER)) ? 1 : 0;
  sensor_state  = (gpio_in & (1 << PIN_SENSOR))  ? 1 : 0;
  needSend = true;
}

void setup() {
  setCpuFrequencyMhz(160); // CPU 160MHz

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  esp_wifi_start();
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE); // Cố định Kênh 1

  // Khởi tạo Header 802.11 thô
  packet.header.frame_control = 0x0008; // IEEE 802.11 Data Frame
  packet.header.duration = 0x0000;
  memset(packet.header.da, 0xFF, 6);   // Broadcast
  memcpy(packet.header.sa, txMAC, 6);   // Gán MAC TX
  memcpy(packet.header.bssid, txMAC, 6);
  packet.header.seq_ctrl = 0;

  pinMode(PIN_TRIGGER, INPUT_PULLUP);
  pinMode(PIN_SENSOR, INPUT_PULLUP);

  // Đọc trạng thái ban đầu
  uint32_t gpio_in = REG_READ(GPIO_IN_REG);
  trigger_state = (gpio_in & (1 << PIN_TRIGGER)) ? 1 : 0;
  sensor_state  = (gpio_in & (1 << PIN_SENSOR))  ? 1 : 0;

  // Gắn ngắt CHANGE cho CẢ 2 CHÂN
  attachInterrupt(digitalPinToInterrupt(PIN_TRIGGER), ISR_Pin_Change, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_SENSOR),  ISR_Pin_Change, CHANGE);

  // Gửi gói đầu tiên để đồng bộ trạng thái ban đầu với RX
  needSend = true;
}

void loop() {
  if (needSend) {
    uint8_t t, s;
    noInterrupts();
    t = trigger_state;
    s = sensor_state;
    needSend = false;
    interrupts();

    // Cập nhật dữ liệu vào gói tin thô
    packet.payload[0] = t;
    packet.payload[1] = s;
    packet.header.seq_ctrl = (seq_counter++) << 4;

    // Burst Send: Bắn liên tiếp 3 gói tin thô (tổng thời gian ~60us) để chống nhiễu sóng
    for (int i = 0; i < 3; i++) {
      esp_wifi_80211_tx(WIFI_IF_STA, &packet, sizeof(packet), false);
    }
  }
}