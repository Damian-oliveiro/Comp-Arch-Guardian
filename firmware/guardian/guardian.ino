/*************************************************************************
 * Guardian V2 - Standalone IoT Safety & Location Beacon
 * 
 * Description:
 * A wearable safety device on the TinyCircuit platform that sends a
 * Telegram alert when a button is pressed or a fall is detected.
 * This version displays live accelerometer data on the screen.
 *
 * Hardware:
 * - Tinyscreen+ (ASM2022)
 * - TinyShield WiFi Board (ASD2123-R)
 * - Sensor Board (ASD211) with BMA250
 * - Large Button Wireling (AST1028)
 *
 * Written for the TinyCircuits platform.
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
// All sensitive data is now stored in the 'secrets.h' file.
#include "secrets.h"
// -----------------------------------------------------------------------------

// Hardware Pin Definitions
#define BUTTON_PIN A1 // Large Button Wireling is connected to pin A1

// WiFi and Network Configuration
char server[] = "api.telegram.org";
WiFiSSLClient client;

// Initialize Hardware Objects
TinyScreen display = TinyScreen(TinyScreenPlus);
BMA250 accel_sensor;

// Fall Detection Thresholds
const float FREEFALL_THRESHOLD = 0.4; // g-force threshold for freefall (e.g., < 0.4g)
const float IMPACT_THRESHOLD = 3.0;   // g-force threshold for impact (e.g., > 3.0g)
bool isFallDetected = false;

// Cooldown period to prevent spamming alerts (in milliseconds)
const unsigned long ALERT_COOLDOWN = 10000; // 10 seconds
unsigned long lastAlertTime = 0;


void setup() {
  // Start Serial for debugging (optional)
  Serial.begin(9600);

  // Initialize I2C for the accelerometer
  Wire.begin();
  
  // Initialize the TinyScreen display
  display.begin();
  display.setBrightness(10);
  display.setFont(thinPixel7_10ptFontInfo);
  display.fontColor(TS_8b_White, TS_8b_Black);
  display.setCursor(5, 20);
  display.print("Guardian V2 Booting...");
  delay(1000);

  // Initialize the Large Button Wireling pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initialize the BMA250 accelerometer
  if (accel_sensor.begin(BMA250_range_4g, BMA250_update_time_64ms) != 0) {
    display.clearScreen();
    display.setCursor(10, 20);
    display.print("Accel Fail!");
    while(1); // Halt if sensor fails
  } else {
    display.clearScreen();
    display.setCursor(15, 20);
    display.fontColor(TS_8b_Green, TS_8b_Black);
    display.print("Accel OK!");
    delay(1500);
    display.fontColor(TS_8b_White, TS_8b_Black);
  }

  // Set WiFi pins - VERY IMPORTANT for TinyShield WiFi
  WiFi.setPins(8, 2, A3, -1);
  
  // Check if the WiFi shield is present
  if (WiFi.status() == WL_NO_SHIELD) {
    display.clearScreen();
    display.setCursor(10, 20);
    display.print("WiFi Fail!");
    while (true); // Halt if no shield
  }

  // Connect to WiFi
  connectWiFi();

  // Device is ready
  display.clearScreen();
  display.setCursor(25, 5); // Move "Armed" to the top of the screen
  display.fontColor(TS_8b_Green, TS_8b_Black);
  display.print("Armed");
}


void loop() {
  // Check if enough time has passed since the last alert to re-arm
  if (millis() - lastAlertTime > ALERT_COOLDOWN) {
    
    // 1. Check for manual button press
    if (digitalRead(BUTTON_PIN) == LOW) {
      display.clearScreen();
      display.setCursor(10, 20);
      display.print("Button Press!");
      delay(500);
      sendTelegramAlert("Manual alert triggered!");
      lastAlertTime = millis();
    }

    // 2. Check for a fall
    if (checkForFall()) {
      display.clearScreen();
      display.setCursor(15, 20);
      display.print("Fall Detected!");
      delay(500);
      sendTelegramAlert("Fall detected!");
      lastAlertTime = millis();
    }
  }

  // Update the display with live accelerometer data
  updateDisplay();
  
  // Add a small delay to make the display readable
  delay(100);
}

/**
 * @brief Updates the display with live data from the BMA250.
 */
void updateDisplay() {
  accel_sensor.read();
  
  display.fontColor(TS_8b_White, TS_8b_Black);

  // Create formatted strings for each axis
  String x_str = "X: " + String(accel_sensor.X);
  String y_str = "Y: " + String(accel_sensor.Y);
  String z_str = "Z: " + String(accel_sensor.Z);

  // Set cursor and print X value, adding spaces to clear old characters
  display.setCursor(10, 25);
  display.print(x_str);
  for (int i = x_str.length(); i < 15; i++) { display.print(" "); }

  // Set cursor and print Y value
  display.setCursor(10, 35);
  display.print(y_str);
  for (int i = y_str.length(); i < 15; i++) { display.print(" "); }
  
  // Set cursor and print Z value
  display.setCursor(10, 45);
  display.print(z_str);
  for (int i = z_str.length(); i < 15; i++) { display.print(" "); }
}


/**
 * @brief Connects to the configured WiFi network.
 */
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


/**
 * @brief Sends an alert message to the configured Telegram chat.
 * @param event_message The message describing the event.
 */
void sendTelegramAlert(String event_message) {
  display.clearScreen();
  display.setCursor(10, 20);
  display.fontColor(TS_8b_Yellow, TS_8b_Black);
  display.print("Sending Alert...");
  
  if (client.connect(server, 443)) {
    String message = "Guardian V2 Alert: " + event_message;
    message.replace(" ", "%20");
    
    client.println("GET /bot" + String(SECRET_BOT_TOKEN) + "/sendMessage?chat_id=" + String(SECRET_CHAT_ID) + "&text=" + message);
    client.println("Host: " + String(server));
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
  
  // Return to Armed screen state; the loop will handle coordinate display
  display.clearScreen();
  display.setCursor(25, 5); // Reset to the same top position
  display.fontColor(TS_8b_Green, TS_8b_Black);
  display.print("Armed");
}


/**
 * @brief Checks the accelerometer for a fall event.
 * @return True if a fall is detected, false otherwise.
 */
bool checkForFall() {
  static bool freefall_flag = false;
  static unsigned long freefall_time = 0;

  accel_sensor.read();
  
  float x_g = accel_sensor.X / 1024.0;
  float y_g = accel_sensor.Y / 1024.0;
  float z_g = accel_sensor.Z / 1024.0;
  float total_g = sqrt(pow(x_g, 2) + pow(y_g, 2) + pow(z_g, 2));

  if (total_g < FREEFALL_THRESHOLD) {
    freefall_flag = true;
    freefall_time = millis();
  }

  if (freefall_flag) {
    if (total_g > IMPACT_THRESHOLD) {
      freefall_flag = false;
      return true;
    }
    if (millis() - freefall_time > 1000) {
      freefall_flag = false;
    }
  }
  
  return false;
}