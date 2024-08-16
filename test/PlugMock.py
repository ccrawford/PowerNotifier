import time
import requests
from threading import Event

def send_request(value):
    url = f"http://192.168.4.1/current?value={value}"
    try:
        response = requests.get(url)
        print(f"Sent request: {url}")
        if response.status_code == 200:
            print("Request successful")
        else:
            print(f"Request failed with status code: {response.status_code}")
    except requests.RequestException as e:
        print(f"Request failed: {e}")

def process_input(current, duration):
    end_time = time.time() + duration
    while time.time() < end_time:
        if current != -1:
            send_request(current)
        else:
            print("Simulating lost connection")
        Event().wait(0.5)

def main():
    input_file = "input.txt"  # Name of your input file
    
    with open(input_file, 'r') as file:
        lines = file.readlines()
    
    for line in lines:
        current, duration = map(float, line.strip().split(','))
        process_input(current, duration)
    
    # After processing all inputs, keep sending 0 amps
    print("Input exhausted. Continuously sending 0 amps.")
    while True:
        send_request(0)
        Event().wait(0.5)

if __name__ == "__main__":
    main()