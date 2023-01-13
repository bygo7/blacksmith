import sys

hex_bit = sys.argv[1]
print(bin(int(hex_bit, 16)).zfill(32), len(bin(int(hex_bit, 16)).zfill(32)))