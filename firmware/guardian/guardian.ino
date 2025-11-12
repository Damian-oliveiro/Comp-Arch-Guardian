/*************************************************************************
 * Guardian V2 - Standalone IoT Safety & Location Beacon
 * 
 * Description:
 * A wearable safety device on the TinyCircuit platform that sends a
 * Telegram alert when a button is pressed or a fall is detected.
 * This version sends an HTTP request to an intermediate Python server, which
 * then securely forwards the alert to Telegram.
 ************************************************************************/

// Included Libraries
#include <SPI.h>
#include <Wire.h>
#include <WiFi101.h>
#include <TinyScreen.h>
#include <BMA250.h>
#include <math.h>

// -----------------------------------------------------------------------------
// USER CONFIGURATION SECTION
// -----------------------------------------------------------------------------
// All user-specific settings (WiFi credentials, server IP, and port)
// have been moved to the 'secrets.h' file for better organization.
#include "secrets.h"
// -----------------------------------------------------------------------------

// Hardware Pin Definitions
#define BUTTON_PIN A1 // Large Button Wireling is connected to pin A1

// Network Client
WiFiClient client;

// Initialize Hardware Objects
TinyScreen display = TinyScreen(TinyScreenPlus);
BMA250 accel_sensor;

// =============================================================================
// MODIFIED FALL DETECTION THRESHOLDS
// These values have been adjusted to be more sensitive to realistic falls.
// =============================================================================
const float FREEFALL_THRESHOLD = 0.6; // Increased from 0.4 to catch slower falls.
const float IMPACT_THRESHOLD = 2.2;   // Lowered from 3.0 to detect softer impacts.
// =============================================================================


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
  display.print("Guardian V2 Booting...");
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
      sendTelegramAlert("Fall detected!");
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

void sendTelegramAlert(String event_message) {
  display.clearScreen();
  display.setCursor(10, 20);
  display.fontColor(TS_8b_Yellow, TS_8b_Black);
  display.print("Sending Alert...");
  
  // Connect to the server using credentials from secrets.h
  if (client.connect(SECRET_SERVER_IP, SECRET_SERVER_PORT)) {
    event_message.replace(" ", "%20");
    String url = "/send_alert?event_message=" + event_message;
    
    client.print(String("GET ") + url + " HTTP/1.1\r\n");
    // Use the Host IP from secrets.h
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
  
  // Correct divisor for +/- 4g range is 256.0
  float x_g = accel_sensor.X / 256.0;
  float y_g = accel_sensor.Y / 256.0;
  float z_g = accel_sensor.Z / 256.0;
  float total_g = sqrt(pow(x_g, 2) + pow(y_g, 2) + pow(z_g, 2));

  // --- DEBUGGING: Use the Serial Monitor in the Arduino IDE to see these values ---
  Serial.print("Total G-force: ");
  Serial.println(total_g);
  // -------------------------------------------------------------------------------

  if (total_g < FREEFALL_THRESHOLD) {
    freefall_flag = true;
    freefall_time = millis();
  }

  if (freefall_flag) {
    if (total_g > IMPACT_THRESHOLD) {
      freefall_flag = false;
      return true; // Fall detected!
    }
    
    // MODIFIED: Reset the flag if no impact is detected within 1.2 seconds
    if (millis() - freefall_time > 1200) {
      freefall_flag = false;
    }
  }
  
  return false;
}
