import sys

input_string = sys.argv[1]
input_bits = []
for i in range(1, len(sys.argv)):
    input_bits.append(sys.argv[i])

out = 0
for bit in input_bits:
    out += 2 ** int(bit)
print(hex(out))