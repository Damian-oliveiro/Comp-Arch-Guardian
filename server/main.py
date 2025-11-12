# main.py
import os
import requests
from fastapi import FastAPI, HTTPException
import uvicorn
from dotenv import load_dotenv

# --- Configuration ---
# Load environment variables from a .env file
load_dotenv()

# Get secrets from environment variables
SECRET_BOT_TOKEN = os.getenv("SECRET_BOT_TOKEN")
SECRET_CHAT_ID = os.getenv("SECRET_CHAT_ID")

# --- Validation ---
# Ensure that the secrets were loaded correctly. If not, the server will refuse to start.
if not SECRET_BOT_TOKEN or not SECRET_CHAT_ID:
    raise ValueError(
        "Missing required environment variables. "
        "Please ensure a .env file with SECRET_BOT_TOKEN and SECRET_CHAT_ID is present."
    )
# ---------------------

# Initialize the FastAPI application
app = FastAPI()

# Define the base URL for the Telegram Bot API
TELEGRAM_API_URL = f"https://api.telegram.org/bot{SECRET_BOT_TOKEN}/sendMessage"

@app.get("/send_alert")
def send_alert(event_message: str):
    """
    This endpoint receives an alert from the IoT device via an HTTP GET request
    and forwards it securely to the Telegram API.
    
    Example request from device: GET /send_alert?event_message=Fall%20detected
    """
    if not event_message:
        raise HTTPException(status_code=400, detail="event_message parameter cannot be empty.")

    print(f"Received alert from Guardian device: '{event_message}'")

    # Prepare the parameters for the HTTPS request to the Telegram API
    params = {
        'chat_id': SECRET_CHAT_ID,
        'text': f"Guardian V4 Alert: {event_message}"
    }

    try:
        # Send the secure HTTPS GET request to the Telegram API
        response = requests.get(TELEGRAM_API_URL, params=params, timeout=10)
        response.raise_for_status() # Raise an exception for HTTP error codes

        print(f"Successfully forwarded alert to Telegram. Server response: {response.json()}")
        return {"status": "success", "message": "Alert forwarded to Telegram"}

    except requests.exceptions.RequestException as e:
        # This catches all network-related errors (timeouts, DNS, etc.)
        print(f"Error sending request to Telegram: {e}")
        raise HTTPException(status_code=502, detail=f"Failed to send request to Telegram: {e}")

@app.get("/")
def read_root():
    """
    A simple root endpoint to verify that the alert server is running.
    Access this in a browser at http://<your-server-ip>:8000/
    """
    return {"status": "Guardian Alert Server is running"}

# This block allows the script to be run directly with 'python main.py'
if __name__ == "__main__":
    # The --host '0.0.0.0' makes the server accessible from other devices on your local network
    uvicorn.run(app, host="0.0.0.0", port=8000)
