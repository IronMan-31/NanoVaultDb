import asyncio
import websockets
import json

# ─────────────────────────────────────────────────────
# Binance provides TWO order book streams:
#
# 1. bookTicker  → L1 only (best bid + best ask)
#    fires on every single change to best bid/ask
#    ~100-500 messages/sec for BTC
#
# 2. depth@100ms → L2 (top 5/10/20 levels)
#    fires every 100ms with a diff update
#    shows full depth, not just best price
# ─────────────────────────────────────────────────────

SYMBOL = "btcusdt"

async def l1_book_ticker():
    """
    L1: best bid and best ask only.
    Fires on every change. Lowest latency.
    This is what you use for OBI on raw ticks.
    """
    url = f"wss://stream.binance.com:9443/ws/{SYMBOL}@bookTicker"
    print(f"\n{'='*55}")
    print(f"  L1 Book Ticker  --  {SYMBOL.upper()}")
    print(f"{'='*55}")
    print(f"{'UPDATE ID':<14} {'BID PRICE':>14} {'BID QTY':>12}  {'ASK PRICE':>14} {'ASK QTY':>12}  {'SPREAD':>10}  {'OBI':>7}")
    print(f"{'-'*55}{'-'*55}")

    async with websockets.connect(url) as ws:
        count = 0
        async for msg in ws:
            d = json.loads(msg)

            update_id  = d['u']
            bid_price  = float(d['b'])
            bid_qty    = float(d['B'])
            ask_price  = float(d['a'])
            ask_qty    = float(d['A'])

            spread = ask_price - bid_price
            # order book imbalance: positive = more bid pressure
            obi = (bid_qty - ask_qty) / (bid_qty + ask_qty + 1e-9)

            print(
                f"{update_id:<14} "
                f"{bid_price:>14.2f} {bid_qty:>12.5f}  "
                f"{ask_price:>14.2f} {ask_qty:>12.5f}  "
                f"{spread:>10.2f}  "
                f"{obi:>+7.3f}"
            )

            count += 1
            if count >= 20:   # print 20 updates then stop
                break


async def l2_depth():
    """
    L2: top 20 price levels on each side.
    Fires every 100ms as a diff (only changed levels).
    Use this for deep order book strategies.
    """
    url = f"wss://stream.binance.com:9443/ws/{SYMBOL}@depth20@100ms"
    print(f"\n{'='*55}")
    print(f"  L2 Depth (top 20 levels)  --  {SYMBOL.upper()}")
    print(f"{'='*55}")

    async with websockets.connect(url) as ws:
        count = 0
        async for msg in ws:
            d = json.loads(msg)

            bids = d['bids']   # list of [price, qty], best first
            asks = d['asks']   # list of [price, qty], best first

            print(f"\n--- snapshot #{count+1} "
                  f"(lastUpdateId={d['lastUpdateId']}) ---")
            print(f"{'LEVEL':<6} {'BID PRICE':>14} {'BID QTY':>12}  "
                  f"{'ASK PRICE':>14} {'ASK QTY':>12}")
            print(f"{'-'*6} {'-'*14} {'-'*12}  {'-'*14} {'-'*12}")

            rows = max(len(bids), len(asks))
            for i in range(min(rows, 10)):  # show top 10 levels
                bp = float(bids[i][0]) if i < len(bids) else 0
                bq = float(bids[i][1]) if i < len(bids) else 0
                ap = float(asks[i][0]) if i < len(asks) else 0
                aq = float(asks[i][1]) if i < len(asks) else 0
                print(f"{i+1:<6} {bp:>14.2f} {bq:>12.5f}  "
                      f"{ap:>14.2f} {aq:>12.5f}")

            # total depth imbalance across all 20 levels
            total_bid = sum(float(b[1]) for b in bids)
            total_ask = sum(float(a[1]) for a in asks)
            obi_deep  = (total_bid - total_ask) / (total_bid + total_ask + 1e-9)
            print(f"\n  total bid depth: {total_bid:.4f}  "
                  f"total ask depth: {total_ask:.4f}  "
                  f"deep OBI: {obi_deep:+.4f}")

            count += 1
            if count >= 5:    # print 5 snapshots then stop
                break


async def combined():
    """
    Combined: trade + L1 book ticker on one connection.
    This is the packet that maps to your 64-byte TickPacket.
    """
    url = (f"wss://stream.binance.com:9443/stream"
           f"?streams={SYMBOL}@trade/{SYMBOL}@bookTicker")

    print(f"\n{'='*70}")
    print(f"  Combined trade + L1 book  --  {SYMBOL.upper()}")
    print(f"{'='*70}")
    print(f"{'TYPE':<10} {'PRICE':>12} {'QTY':>10} "
          f"{'SIDE':<7} {'BID':>12} {'ASK':>12} {'SPREAD':>8}")
    print(f"{'-'*85}")

    last_bid = last_ask = 0.0

    async with websockets.connect(url) as ws:
        count = 0
        async for raw in ws:
            msg  = json.loads(raw)
            data = msg['data']
            etype = data.get('e', '')

            if etype == 'bookTicker':
                last_bid = float(data['b'])
                last_ask = float(data['a'])

            elif etype == 'trade':
                price  = float(data['p'])
                qty    = float(data['q'])
                # m=True means buyer was maker -> SELL aggressor
                side   = "SELL" if data['m'] else "BUY "
                spread = last_ask - last_bid if last_ask > 0 else 0

                print(
                    f"{'TRADE':<10} {price:>12.2f} {qty:>10.5f} "
                    f"{side:<7} {last_bid:>12.2f} {last_ask:>12.2f} "
                    f"{spread:>8.2f}"
                )
                count += 1
                if count >= 15:
                    break


async def main():
    print("Choose which stream to show:")
    print("  1 = L1 book ticker (best bid/ask only)")
    print("  2 = L2 depth (top 20 levels)")
    print("  3 = combined trade + L1 (your 64-byte packet)")

    choice = input("Enter 1, 2, or 3: ").strip()

    if choice == "1":
        await l1_book_ticker()
    elif choice == "2":
        await l2_depth()
    elif choice == "3":
        await combined()
    else:
        print("running all three sequentially...")
        await l1_book_ticker()
        await l2_depth()
        await combined()

if __name__ == "__main__":
    asyncio.run(main())