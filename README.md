# Guardian: IoT Safety & Location Beacon

The Guardian Project is a wearable IoT safety device built on the TinyCircuit platform. It is designed to send an emergency alert to a Telegram chat when a user presses a button or when an advanced, hybrid algorithm detects a potential fall.

This system is composed of two main parts:
1.  **The TinyCircuit Wearable:** A battery-powered device that monitors for button presses and fall events.
2.  **The Python Proxy Server:** A script running on a local PC that receives simple HTTP alerts from the device and securely forwards them to the Telegram API.

### Key Features
- **Manual Alert:** Trigger an alert instantly by pressing the large button.
- **High-Impact Fall Detection:** Detects sudden, hard falls using an "Impact -> Stillness" model.
- **Low-Impact Slump Detection:** Detects slower falls, slumps, or fainting spells using a "Tilt -> No Recovery" model.
- **On-Screen Debugging:** The TinyScreen+ provides real-time feedback on G-force, device orientation, and fall detection status.
- **Secure & Decoupled:** The device sends insecure local alerts, and the server handles the secure HTTPS communication with Telegram, improving device performance and security.

---

## Part 1: Setting Up the TinyCircuit Device

### Required Components:
- **Processor & Display:** TinyScreen+ (ASM2022)
- **Connectivity:** TinyShield WiFi Board (ASD2123-R) (uses the ATWINC1500 chipset)
- **Sensing:** Sensor Board (ASD2511) (includes a BMA250 accelerometer)
- **User Input:** Large Button Wireling (AST1028)
- **Power:** 150 mAh Li-Poly Battery (ASR0003)

### Development Environment & Toolchain:
- **Operating System:** Windows
- **IDE:** Arduino IDE
- **Board Support Package:** Arduino SAMD Boards (32-bits ARM Cortex-M0+)
- **IDE Configuration:**
    - **Board:** `Tools > Board > Arduino SAMD Boards > TinyScreen+`
    - **Port:** The correct COM port for the connected device.

### Library Installation
1.  Open the Arduino IDE.
2.  Go to **Tools > Manage Libraries...** (`Ctrl+Shift+I`).
3.  Find and install the following libraries:
    *   `TinyScreen` (by TinyCircuits, ensure v1.1.0 or newer)
    *   `WiFi101` (by Arduino) - [GitHub Link](https://github.com/arduino-libraries/WiFi101)
    *   `TinyCircuits BMA250 library` (by TinyCircuits) - [GitHub Link](https://github.com/TinyCircuits/TinyCircuits-BMA250-library)

### Configuration & Uploading
1.  **Find Your Server's Local IP:** Before configuring the device, you must know the IP address of the computer that will run the Python server. Open Command Prompt (`cmd`) on that PC and type `ipconfig`. Find the "IPv4 Address" (e.g., `192.168.1.105`).
2.  **Configure Credentials:** Open the `firmware/guardian/secrets.h` file in the Arduino IDE.
    *   Set `SECRET_SSID` to your WiFi network name.
    *   Set `SECRET_PASS` to your WiFi password.
    *   Set `SECRET_SERVER_IP` to the IP address you found in the previous step.
3.  **Upload the Sketch:**
    *   Open the `firmware/guardian/guardian.ino` file.
    *   Connect your TinyScreen+ to your computer via USB.
    *   Ensure the correct Board and Port are selected under the `Tools` menu.
    *   Click the "Upload" button (the right-arrow icon).

---

## Part 2: Setting Up the Python Proxy Server

The server acts as a bridge, listening for simple HTTP requests from the TinyScreen+ and securely relaying them to Telegram.

### System Requirements
- Python 3.7+
- Windows OS (with firewall access)

### Server Setup
1.  **Navigate to the Server Directory:** Open a Command Prompt (`cmd`) or PowerShell and change to the `server` directory of this project.
    ```bash
    cd path/to/project/server
    ```

3.  **Install Dependencies:** Run the following command to install all the necessary Python libraries:
    ```bash
    pip install -r requirements.txt
    ```

4.  **Configure Environment Variables:**
    *   In the `server` directory, create a new file named `.env`.
    *   Add your Telegram Bot Token and Chat ID (see Part 3 below) to this file:
      ```
      # .env file - Stores secrets securely
      SECRET_BOT_TOKEN="YOUR_TELEGRAM_BOT_TOKEN_HERE"
      SECRET_CHAT_ID="YOUR_TELEGRAM_CHAT_ID_HERE"
      ```

5.  **Configure Windows Firewall:** You must allow the TinyScreen+ to connect to your server.
    *   Open "Windows Defender Firewall with Advanced Security".
    *   Go to "Inbound Rules" and click "New Rule...".
    *   Select **Port**, then **TCP**, and specify local port **8000**.
    *   Select **Allow the connection** and apply the rule to your current network profile (e.g., Private).
    *   Give the rule a name like "Guardian Project Server".

### Running the Server
1.  Ensure you are still in the `server` directory in your Command Prompt.
2.  Run the server using this command:
    ```bash
    python main.py
    ```
3.  A message like `INFO: Uvicorn running on http://0.0.0.0:8000` will appear. **Keep this terminal window open** for the server to remain active.

---

## Part 3: Setting up Telegram

1.  **Create a Telegram Bot:**
    *   In Telegram, start a chat with the user `@BotFather`.
    *   Send the `/newbot` command and follow the prompts to name your bot.
    *   BotFather will give you a **Bot Token**. This is your `SECRET_BOT_TOKEN`.
2.  **Find Your Chat ID:**
    *   In Telegram, start a chat with the user `@userinfobot`.
    *   Send the `/start` command.
    *   The bot will reply with your information, including your **Chat ID**. This is your `SECRET_CHAT_ID`.

---

## Part 4: Project Structure

```
.
├── firmware
│   └── guardian
│       ├── guardian.ino     # Main code for the TinyScreen+ device
│       └── secrets.h        # Configuration for WiFi and server IP
├── server
│   ├── main.py              # The Python proxy server script
│   ├── requirements.txt     # Python library dependencies
│   └── .env                 # (You create this) Stores Telegram secrets
└── README.md                # This file
```
