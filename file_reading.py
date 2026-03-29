import struct

with open("./db/tables/school/btc_ticks.data", "rb") as f:
    while chunk := f.read(8):  # read 8 bytes at a time
        value = struct.unpack("q", chunk)[0]  # 'q' = signed 8-byte int
        print(value)