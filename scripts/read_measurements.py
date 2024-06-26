import argparse
import asyncio
import datetime
import sys
import time
import websockets

DHT11_ID = 3
ADC_ID = 4

async def handler(websocket, output_file=sys.stdout):
    async for message in websocket:
        if len(message) < 1:
            continue
        id = message[0]
        unix_time = int(time.time())
        if id == DHT11_ID:
            if len(message) == 1:
                print(f"[{unix_time}] dht11 sent short message", file=sys.stderr)
            elif len(message) == 2:
                print(f"[{unix_time}] dht11 had error ({message[1]:02X})", file=sys.stderr)
            else:
                humidity = message[1]
                temperature = message[2]
                print(f"[{unix_time}] t={temperature} h={humidity}", file=output_file)
        elif id == ADC_ID:
            if len(message) == 1:
                print(f"[{unix_time}] adc sent short message", file=sys.stderr)
            elif len(message) == 2:
                print(f"[{unix_time}] adc had error ({message[1]:02X})", file=sys.stderr)
            else:
                adc = (message[2] << 8) | message[1]
                print(f"[{unix_time}] a={adc}", file=output_file)
        else:
            print(f"[{unix_time}] invalid return id ({id:02X})", file=sys.stderr)
            continue

        # flush output so tee works in realtime
        if output_file == sys.stdout:
            sys.stdout.flush()

async def sender(websocket, period):
    while True:
        await websocket.send(bytearray([DHT11_ID]))
        now = datetime.datetime.now()
        time_str = f"{now.hour:02d}:{now.minute:02d}:{now.second:02d}"
        print(f"[{time_str}][dht11] sent request", file=sys.stderr)
        await asyncio.sleep(period)

        await websocket.send(bytearray([ADC_ID]))
        now = datetime.datetime.now()
        time_str = f"{now.hour:02d}:{now.minute:02d}:{now.second:02d}"
        print(f"[{time_str}][adc] sent request", file=sys.stderr)
        await asyncio.sleep(period)

async def main():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("--url", default="192.168.1.137:80", type=str, help="URL to connect to")
    parser.add_argument("--period", default=5, type=float, help="Seconds between dht11 requests")
    args = parser.parse_args()

    URL = f"ws://{args.url}/api/v1/websocket"
    async for websocket in websockets.connect(URL):
        try:
            print(f"Connection opened to {URL}", file=sys.stderr)
            await asyncio.gather(handler(websocket), sender(websocket, args.period))
        except websockets.ConnectionClosed:
            print(f"Connection closed from {URL}", file=sys.stderr)
            continue

if __name__ == '__main__':
    asyncio.run(main())
