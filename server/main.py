# server/main.py
import os
import requests
from fastapi import FastAPI, HTTPException, Query
import uvicorn
from dotenv import load_dotenv
import googlemaps

# --- Configuration ---
load_dotenv()
SECRET_BOT_TOKEN = os.getenv("SECRET_BOT_TOKEN")
SECRET_CHAT_ID = os.getenv("SECRET_CHAT_ID")
GOOGLE_MAPS_API_KEY = os.getenv("GOOGLE_MAPS_API_KEY")

# --- Validation ---
if not all([SECRET_BOT_TOKEN, SECRET_CHAT_ID, GOOGLE_MAPS_API_KEY]):
    raise ValueError(
        "Missing required environment variables. "
        "Ensure SECRET_BOT_TOKEN, SECRET_CHAT_ID, and GOOGLE_MAPS_API_KEY are in .env"
    )

# --- Initialize Services ---
app = FastAPI()
gmaps = googlemaps.Client(key=GOOGLE_MAPS_API_KEY)
TELEGRAM_API_URL = f"https://api.telegram.org/bot{SECRET_BOT_TOKEN}/sendMessage"
GEOLOCATION_API_URL = f"https://www.googleapis.com/geolocation/v1/geolocate?key={GOOGLE_MAPS_API_KEY}"

def get_location_from_wifi(wifi_scan_data: str):
    """
    Parses the device's custom wifi scan string and uses the Google Geolocation
    API to estimate latitude and longitude.
    """
    wifi_access_points = []
    networks = wifi_scan_data.split(';')
    for net in networks:
        parts = net.split(',')
        if len(parts) == 2:
            mac, rssi = parts
            wifi_access_points.append({"macAddress": mac, "signalStrength": int(rssi)})

    if not wifi_access_points:
        return None

    try:
        response = requests.post(GEOLOCATION_API_URL, json={"wifiAccessPoints": wifi_access_points}, timeout=10)
        response.raise_for_status()
        location_data = response.json()
        return location_data['location'] # Returns {'lat': ..., 'lng': ...}
    except requests.exceptions.RequestException as e:
        print(f"Error calling Geolocation API: {e}")
        return None

def get_human_address(location: dict):
    """
    Uses the Google Geocoding API (via reverse geocode) to turn lat/lng
    coordinates into a human-readable address.
    """
    try:
        reverse_geocode_result = gmaps.reverse_geocode((location['lat'], location['lng']))
        if reverse_geocode_result:
            return reverse_geocode_result[0]['formatted_address']
        return "Address not found."
    except Exception as e:
        print(f"Error during reverse geocoding: {e}")
        return "Could not resolve address."

@app.get("/send_alert")
def send_alert(event_message: str, wifi_scan: str = None):
    """
    Receives an alert and an optional Wi-Fi scan payload. If the scan is present,
    it attempts to geolocate the device and add the location to the alert.
    
    Example: /send_alert?event_message=Fall%20detected&wifi_scan=AA:BB:CC:DD:EE:FF,-55;...
    """
    if not event_message:
        raise HTTPException(status_code=400, detail="event_message cannot be empty.")

    print(f"Received alert: '{event_message}', Wi-Fi Scan: '{'Yes' if wifi_scan else 'No'}'")

    final_message = f"Guardian Alert: {event_message}"
    
    if wifi_scan and wifi_scan != "none":
        # Step 1: Get Lat/Lng from Wi-Fi scan
        coords = get_location_from_wifi(wifi_scan)
        
        if coords:
            # Step 2: Get human address from Lat/Lng
            human_address = get_human_address(coords)
            final_message += f"\n\n📍 *Approx. Location:*\n{human_address}"
            # Step 3: Add a direct Google Maps link
            final_message += f"\n\n🗺️ *Map Link:*\nhttps://www.google.com/maps?q={coords['lat']},{coords['lng']}"
        else:
             final_message += "\n\n📍 *Location:*\nCould not be determined."

    params = {'chat_id': SECRET_CHAT_ID, 'text': final_message, 'parse_mode': 'Markdown'}

    try:
        response = requests.get(TELEGRAM_API_URL, params=params, timeout=10)
        response.raise_for_status()
        print(f"Successfully forwarded alert to Telegram. Response: {response.json()}")
        return {"status": "success", "message": "Alert forwarded"}
    except requests.exceptions.RequestException as e:
        print(f"Error sending request to Telegram: {e}")
        raise HTTPException(status_code=502, detail=f"Failed to send request: {e}")

@app.get("/")
def read_root():
    return {"status": "Guardian Alert Server is running"}

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)
