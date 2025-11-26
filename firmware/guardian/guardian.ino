/*************************************************************************
 * Guardian V3 - Standalone IoT Safety & Location Beacon w/ WiFi Geolocation
 * 
 * Description:
 * This version scans for nearby WiFi networks and sends their data to a 
 * server. The server then uses this data to estimate the device's location
 * and includes it in the Telegram alert.
 ************************************************************************/

// Included Libraries
#include <SPI.h>
#include <Wire.h>
#include <WiFi101.h>
#include <TinyScreen.h>
#include <BMA250.h>
#include <math.h>

#include "secrets.h"

// Hardware Pin Definitions
#define BUTTON_PIN A1

// Network Client
WiFiClient client;

// Initialize Hardware Objects
TinyScreen display = TinyScreen(TinyScreenPlus);
BMA250 accel_sensor;

// Fall Detection Thresholds
const float FREEFALL_THRESHOLD = 0.6;
const float IMPACT_THRESHOLD = 2.2;

// Cooldown period to prevent spamming alerts
const unsigned long ALERT_COOLDOWN = 10000; // 10 seconds
unsigned long lastAlertTime = 0;


void setup() {
  Serial.begin(9600);
  Wire.begin();
  
  display.begin();
  display.setBrightness(10);
  display.setFont(thinPixel7_10ptFontInfo);
  display.fontColor(TS_8b_White, TS_8b_Black);
  display.setCursor(5, 20);
  display.print("Guardian V3 Booting...");
  delay(1000);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  if (accel_sensor.begin(BMA250_range_4g, BMA250_update_time_64ms) != 0) {
    display.clearScreen();
    display.setCursor(10, 20);
    display.print("Accel Fail!");
    while(1);
  } else {
    display.clearScreen();
    display.setCursor(15, 20);
    display.fontColor(TS_8b_Green, TS_8b_Black);
    display.print("Accel OK!");
    delay(1500);
    display.fontColor(TS_8b_White, TS_8b_Black);
  }

  WiFi.setPins(8, 2, A3, -1);
  
  if (WiFi.status() == WL_NO_SHIELD) {
    display.clearScreen();
    display.setCursor(10, 20);
    display.print("WiFi Fail!");
    while (true);
  }

  connectWiFi();

  display.clearScreen();
  display.setCursor(25, 5);
  display.fontColor(TS_8b_Green, TS_8b_Black);
  display.print("Armed");
}

void loop() {
  if (millis() - lastAlertTime > ALERT_COOLDOWN) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      sendTelegramAlert("Manual alert triggered!");
      lastAlertTime = millis();
    }

    if (checkForFall()) {
      sendTelegramAlert("Fall detected! Joseph is at risk! LOL weak!");
      lastAlertTime = millis();
    }
  }
  updateDisplay();
  delay(100);
}

void updateDisplay() {
  accel_sensor.read();
  display.fontColor(TS_8b_White, TS_8b_Black);
  display.setCursor(10, 25);
  display.print("X: " + String(accel_sensor.X) + "   ");
  display.setCursor(10, 35);
  display.print("Y: " + String(accel_sensor.Y) + "   ");
  display.setCursor(10, 45);
  display.print("Z: " + String(accel_sensor.Z) + "   ");
}

void connectWiFi() {
  display.clearScreen();
  display.setCursor(5, 20);
  display.print("Connecting WiFi...");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(SECRET_SSID, SECRET_PASS);
    attempts++;
    display.setCursor(45, 40);
    display.print(attempts);
    delay(5000);
  }

  display.clearScreen();
  display.setCursor(10, 20);
  display.fontColor(TS_8b_Green, TS_8b_Black);
  display.print("WiFi Connected");
  delay(2000);
}


/*************************************************************************
 * Scans for nearby WiFi networks and formats them into a single string
 * for the server. Format: "MAC1,RSSI1;MAC2,RSSI2;..."
 ************************************************************************/
String getWifiScanData() {
  display.setCursor(0, 55);
  display.fontColor(TS_8b_Blue, TS_8b_Black); // <-- CORRECTED LINE
  display.print("Scanning WiFi...");

  String payload = "";
  int num_networks = WiFi.scanNetworks();

  display.clearScreen(); // Clear screen before showing results
  
  if (num_networks == 0) {
    display.setCursor(5, 20);
    display.print("No networks found.");
    delay(1000);
    return "none";
  }

  // Limit to the 7 strongest networks to keep URL length manageable
  int networks_to_send = min(num_networks, 7);
  for (int i = 0; i < networks_to_send; i++) {
    uint8_t bssid[6];
    WiFi.BSSID(i, bssid);
    
    char mac_addr[18];
    sprintf(mac_addr, "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    
    payload += String(mac_addr);
    payload += ",";
    payload += WiFi.RSSI(i);
    
    if (i < networks_to_send - 1) {
      payload += ";";
    }
  }
  
  display.setCursor(0, 55);
  display.fontColor(TS_8b_White, TS_8b_Black);
  display.print("Scan Complete!  "); // Extra spaces to clear old text
  return payload;
}


/*************************************************************************
 * Now includes the WiFi scan data in the GET request to the server.
 ************************************************************************/
void sendTelegramAlert(String event_message) {
  display.clearScreen();
  display.setCursor(10, 20);
  display.fontColor(TS_8b_Yellow, TS_8b_Black);
  display.print("Sending Alert...");

  // Get WiFi scan data before sending the alert
  String wifi_data = getWifiScanData();
  
  if (client.connect(SECRET_SERVER_IP, SECRET_SERVER_PORT)) {
    event_message.replace(" ", "%20");
    
    // Append the WiFi scan data to the URL
    String url = "/send_alert?event_message=" + event_message + "&wifi_scan=" + wifi_data;
    
    client.print(String("GET ") + url + " HTTP/1.1\r\n");
    client.print(String("Host: ") + SECRET_SERVER_IP + "\r\n");
    client.println("Connection: close");
    client.println();
    
    display.clearScreen();
    display.setCursor(15, 20);
    display.fontColor(TS_8b_Green, TS_8b_Black);
    display.print("Alert Sent!");

  } else {
    display.clearScreen();
    display.setCursor(25, 20);
    display.fontColor(TS_8b_Red, TS_8b_Black);
    display.print("Failed!");
  }

  client.stop();
  delay(2000);
  
  display.clearScreen();
  display.setCursor(25, 5);
  display.fontColor(TS_8b_Green, TS_8b_Black);
  display.print("Armed");
}

bool checkForFall() {
  static bool freefall_flag = false;
  static unsigned long freefall_time = 0;

  accel_sensor.read();
  
  float x_g = accel_sensor.X / 256.0;
  float y_g = accel_sensor.Y / 256.0;
  float z_g = accel_sensor.Z / 256.0;
  float total_g = sqrt(pow(x_g, 2) + pow(y_g, 2) + pow(z_g, 2));

  Serial.print("Total G-force: ");
  Serial.println(total_g);

  if (total_g < FREEFALL_THRESHOLD) {
    freefall_flag = true;
    freefall_time = millis();
  }

  if (freefall_flag) {
    if (total_g > IMPACT_THRESHOLD) {
      freefall_flag = false;
      return true; // Fall detected!
    }
    
    if (millis() - freefall_time > 1200) {
      freefall_flag = false;
    }
  }
  
  return false;
}
