// secrets.h

#ifndef SECRETS_H
#define SECRETS_H

// --- WiFi Network Credentials ---
const char SECRET_SSID[] = "XXX";         // <-- Your WiFi network name
const char SECRET_PASS[] = "XXX";     // <-- Your WiFi password

// --- Python Proxy Server Configuration ---
// This must be the LOCAL IP address of the PC running the main.py server.
const char SECRET_SERVER_IP[] = "XXX"; // <-- REPLACE WITH YOUR SERVER's LOCAL IP
const int SECRET_SERVER_PORT = 8000;               // <-- The port your Python server is using (default is 8000)

#endif // SECRETS_H
