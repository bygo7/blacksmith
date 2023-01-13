import sys

input_string = sys.argv[1]
out_str = ""
for idx, bit in enumerate(input_string[::-1]):
    if bit == '1':
        out_str += str(idx) + " "
print(out_str)
