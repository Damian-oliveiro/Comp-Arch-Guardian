/*
 * Guardian V2 - Standalone IoT Safety & Location Beacon
 * 
 * This firmware is for the TinyScreen+ platform and uses a WiFi TinyShield,
 * a Sensor Board Wireling (BMA250), and a Button Wireling to create a 
 * standalone safety alert device.
 * 
 * Triggers:
 * 1. Manual press of the Large Button Wireling.
 * 2. Automatic fall detection using the BMA250 accelerometer.
 * 
 * Action:
 * Connects to a pre-configured WiFi network and sends an alert message to a 
 * specific Telegram chat via the Telegram Bot API using HTTPS.
 * 
 * Hardware:
 * - Tinyscreen+ (ASM2022)
 * - TinyShield WiFi Board (ASD2123-R)
 * - Sensor Board (ASD2511)
 * - Large Button Wireling (AST1028)
 * 
 * IDE Setup:
 * - Board: "TinyScreen+"
 * - Libraries: TinyScreen, WiFi101, TinyCircuits BMA250
 */

// Required Libraries
#include <SPI.h>
#include <WiFi101.h>
#include <TinyScreen.h>
#include <Wire.h>
#include <TinyCircuits_BMA250.h>

// Include the new secrets file
// Make sure secrets.h is in the same folder as this .ino file
#include "secrets.h"

// --- Hardware and Global Objects ---
TinyScreen display = TinyScreen(TinyScreenPlus);
BMA250 bma;
WiFiSSLClient client;

// --- Pin Definitions ---
// The Large Button Wireling is connected to the A0/D0 port on the TinyScreen+
const int BUTTON_PIN = A0; 

// --- Constants ---
const char* WIFI_HOST = "api.telegram.org";
const unsigned long ALERT_COOLDOWN_MS = 15000; // 15-second cooldown to prevent spam

// --- Fall Detection Thresholds (tune these for sensitivity) ---
const float FREEFALL_THRESHOLD_G = 0.4; // g-force magnitude to trigger freefall check
const float IMPACT_THRESHOLD_G = 3.0;   // g-force magnitude to confirm impact after freefall
const unsigned long FREEFALL_TIME_MS = 250; // Milliseconds in freefall before it's considered valid

// --- Global State Variables ---
unsigned long lastAlertTime = 0;
bool isFreefall = false;
unsigned long freefallStartTime = 0;
int wifiStatus = WL_IDLE_STATUS;

// =========================================================================
// ===================          SETUP FUNCTION         ===================
// =========================================================================
void setup() {
  Serial.begin(9600);
  
  // Initialize Display
  display.begin();
  display.setBrightness(10);
  display.setFont(thinPixel7_10ptFontInfo);
  display.fontColor(TS_16b_White, TS_16b_Black);

  displayMessage("Guardian V2\nStarting...", 2000);

  // Initialize Wireling Button Pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize Accelerometer Sensor
  displayMessage("Sensor Init...", 0);
  // Using range of 4g and bandwidth of 125Hz.
  // Bandwidth can be adjusted for faster/slower response.
  if (!bma.begin(BMA250_range_4g, BMA250_update_time_8ms)) {
    displayMessage("Sensor FAIL!", 5000);
  } else {
    displayMessage("Sensor OK", 1000);
  }

  // Connect to WiFi
  connectWiFi();

  // Final Status
  if (wifiStatus == WL_CONNECTED) {
    displayMessage("Status: ARMED", 0);
  } else {
    displayMessage("WiFi FAIL!\nOffline Mode", 0);
  }
}


// =========================================================================
// ===================           MAIN LOOP             ===================
// =========================================================================
void loop() {
  // Cooldown check to prevent spamming alerts
  if (millis() - lastAlertTime < ALERT_COOLDOWN_MS) {
    return;
  }

  // --- Trigger 1: Manual Button Press ---
  // The INPUT_PULLUP means the pin is LOW when pressed
  if (digitalRead(BUTTON_PIN) == LOW) {
    displayMessage("Button Alert!", 1000);
    sendTelegramAlert("Manual Alert Triggered!");
    lastAlertTime = millis(); // Start cooldown
  }

  // --- Trigger 2: Automatic Fall Detection ---
  if (checkForFall()) {
    displayMessage("Fall Detected!", 1000);
    sendTelegramAlert("Fall Detected!");
    lastAlertTime = millis(); // Start cooldown
  }
}

// =========================================================================
// ===================         HELPER FUNCTIONS        ===================
// =========================================================================

/**
 * @brief Connects to the configured WiFi network.
 */
void connectWiFi() {
  // These pins are specific to the TinyDuino/TinyScreen+ platform
  WiFi.setPins(8, 2, A3, -1);
  
  // Check if the WiFi shield is present
  if (WiFi.status() == WL_NO_SHIELD) {
    displayMessage("WiFi Shield\nNot Found!", 3000);
    return;
  }
  
  displayMessage("Connecting to\nWiFi...", 0);

  // Attempt to connect
  int attempts = 0;
  while (wifiStatus != WL_CONNECTED && attempts < 15) {
    wifiStatus = WiFi.begin(SECRET_SSID, SECRET_PASS);
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (wifiStatus == WL_CONNECTED) {
    IPAddress ip = WiFi.localIP();
    char ipStr[16];
    sprintf(ipStr, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    String connectedMsg = "Connected!\nIP: ";
    connectedMsg += ipStr;
    displayMessage(connectedMsg.c_str(), 3000);
  } else {
    displayMessage("Connection\nFAILED!", 3000);
  }
}

/**
 * @brief Sends an alert message to the configured Telegram chat.
 * @param eventType A string describing the alert (e.g., "Fall Detected!").
 */
void sendTelegramAlert(const char* eventType) {
  // If not connected, try to reconnect once.
  if (WiFi.status() != WL_CONNECTED) {
    displayMessage("Reconnecting...", 2000);
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      displayMessage("Send FAILED\nNo WiFi", 3000);
      return;
    }
  }

  displayMessage("Sending\nAlert...", 0);
  
  // Construct the message payload. URL encoding spaces with '%20'.
  String message = String(eventType);
  message.replace(" ", "%20");

  if (client.connect(WIFI_HOST, 443)) {
    // Construct the GET request URL
    String url = "/bot";
    url += SECRET_BOT_TOKEN;
    url += "/sendMessage?chat_id=";
    url += SECRET_CHAT_ID;
    url += "&text=";
    url += message;
    
    // Send the request
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + WIFI_HOST + "\r\n" +
                 "Connection: close\r\n\r\n");
                 
    delay(500); // Give server time to respond

    // You can optionally read the response here for debugging
    // while (client.available()) {
    //   char c = client.read();
    //   Serial.write(c);
    // }

    client.stop();
    displayMessage("Alert Sent!", 2000);
  } else {
    displayMessage("Send FAILED!", 3000);
  }
  // After sending, display the armed status again
  displayMessage("Status: ARMED", 0);
}

/**
 * @brief Checks the accelerometer for a fall pattern.
 * A fall is a period of freefall followed immediately by a high-G impact.
 * @return True if a fall is detected, false otherwise.
 */
bool checkForFall() {
  bma.read();

  // Calculate the magnitude of acceleration vector (g-force)
  // We use squares to avoid costly sqrt, and compare with squared thresholds
  // Note: Raw values from the BMA250 are proportional to g's. A value of ~64 is 1g with 4g range.
  float x_g = bma.X / 64.0;
  float y_g = bma.Y / 64.0;
  float z_g = bma.Z / 64.0;
  float total_g = sqrt(x_g * x_g + y_g * y_g + z_g * z_g);
  
  // --- Step 1: Detect start of a potential freefall ---
  if (total_g < FREEFALL_THRESHOLD_G) {
    if (!isFreefall) {
      isFreefall = true;
      freefallStartTime = millis();
    }
  } else {
    // --- Step 2: If we were in freefall, check for impact ---
    if (isFreefall) {
      // Check if the freefall was long enough to be valid
      bool wasValidFreefall = (millis() - freefallStartTime) > FREEFALL_TIME_MS;
      isFreefall = false; // Reset freefall state regardless

      // If freefall was valid AND we now have a high-G impact, it's a fall!
      if (wasValidFreefall && total_g > IMPACT_THRESHOLD_G) {
        return true; // Fall detected!
      }
    }
  }
  
  return false; // No fall detected
}


/**
 * @brief A utility to clear the screen and display a message.
 * @param message The message to display. Can include newlines `\n`.
 * @param delay_ms Time to wait after displaying the message. If 0, no delay.
 */
void displayMessage(const char* message, int delay_ms) {
  display.clearScreen();
  display.setCursor(0, 5);
  display.print(message);
  if (delay_ms > 0) {
    delay(delay_ms);
  }
}