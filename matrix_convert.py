import sys


# Gets DRAM_MTX matrix as a input
length = 34
in_matrix = [""] * length
for i in range(length):
    in_matrix[i] = input().lstrip('\t| ').split(',')[0][2:][::-1]
if in_matrix[-1] == "":
    print("Input Error")
    quit()
print("----------------------------------------")
out = [ [0] * length for _ in range(length)]

for i in range(length):
    for j in range(length):
        if in_matrix[i][j] == '0':
            out[j][i] = '0'
        else:
            out[j][i] = '1'

for i in range(length - 1, -1, -1):
    print("0b" + ''.join(out[i]) + ",")