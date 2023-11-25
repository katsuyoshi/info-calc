#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>

esp_now_peer_info_t espnow_slave;

static bool started = false;
static int minitus = 2;
static int remains = minitus * 600;

// @refer https://it-evo.jp/blog/blog-1397/
void espnow_on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  //Serial.print("Last Packet Sent to: ");
  Serial.println(macStr);
  //Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}


void espnow_setup() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  } else {
    Serial.println("ESPNow Init Failed");
    ESP.restart();
  }

  memset(&espnow_slave, 0, sizeof(espnow_slave));
  for (int i = 0; i < 6; ++i) {
    espnow_slave.peer_addr[i] = (uint8_t)0xff;
  }
  
  esp_err_t addStatus = esp_now_add_peer(&espnow_slave);
  if (addStatus == ESP_OK) {
    Serial.println("Pair success");
  }

  esp_now_register_send_cb(espnow_on_data_sent);
}

void espnow_teardown()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}

void espnow_send(int ch, float value, char *unit) {
  char str[64] = {};
  sprintf(str, "%i,%5.3f,%s", ch, value, unit);
Serial.println(str);
  esp_err_t result = esp_now_send(espnow_slave.peer_addr, (uint8_t *)str, strlen(str));
  Serial.print("Send Status: ");
  if (result == ESP_OK) {
    Serial.println("Success");
  } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
    Serial.println("ESPNOW not Init.");
  } else if (result == ESP_ERR_ESPNOW_ARG) {
    Serial.println("Invalid Argument");
  } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
    Serial.println("Internal Error");
  } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
    Serial.println("ESP_ERR_ESPNOW_NO_MEM");
  } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
    Serial.println("Peer not found.");
  } else {
    Serial.println("Not sure what happened");
  }
}


void display() {
  M5.Lcd.clear();
  M5.Lcd.setColor(started ? GREEN : RED);
  M5.Lcd.setCursor(8, 4);
  M5.Lcd.setTextSize(2);
  M5.Lcd.println(started ? "RUN" : "STOP");
  M5.Lcd.println();

  M5.Lcd.setTextSize(4);

  int m = remains / 600;
  int s = (remains / 10) % 60;
  M5.Lcd.printf(" %02d:%02d\n", m, s);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Lcd.setRotation(1);
  M5.Lcd.setTextSize(3);

  espnow_setup();
  display();
}

void loop() {
  static unsigned long tick = millis();

  M5.update();

  if (M5.BtnA.wasPressed()) {
    Serial.println("Btn A was pressed");
    started = started ? false : true;
    if (started) {
      if (remains == 0) {
        remains = minitus * 60 * 10;
      }
    }
    display();
  }
  if (M5.BtnB.wasPressed()) {
    Serial.println("Btn B was pressed");
    if (started == false) {
      minitus++;
      remains = minitus * 60 * 10;
    }
    display();
  }
  if (M5.BtnB.wasReleaseFor(1000)) {
    Serial.println("Btn B was long pressed");
    minitus = 2;
    remains = minitus * 60 * 10;
    display();
  }

  unsigned long now = millis();
  if (now - tick >= 100) {
    tick = now;

    if (started) {
      if (remains % 10 == 0) {
        int v = remains / 10;
Serial.printf("v: %d\n", v);
        int m = v / 60;
        int s = v % 60;
        float f = (float)m + (float)s * 0.01;
Serial.printf("f: %f\n", f);
        espnow_send(3, f, "timer");
        display();
      }

      if (remains > 0) {
        remains--;
      } else {
        started = false;
        display();
      }
    }
  }

  delay(10);

}
