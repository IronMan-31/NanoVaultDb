import requests

# Binance depth endpoint
url = "https://api.binance.com/api/v3/depth"

params = {
    "symbol": "BTCUSDT",
    "limit": 10   # number of levels
}

response = requests.get(url, params=params)
data = response.json()

bids = data["bids"]
asks = data["asks"]

print("\n===== ORDER BOOK (BTCUSDT) =====\n")

print("ASKS (SELL ORDERS)")
for price, qty in asks:
    print(f"Price: {price}  Quantity: {qty}")

print("\n-------------------------------\n")

print("BIDS (BUY ORDERS)")
for price, qty in bids:
    print(f"Price: {price}  Quantity: {qty}")