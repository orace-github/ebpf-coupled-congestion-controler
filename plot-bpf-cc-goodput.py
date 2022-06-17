import matplotlib.pyplot as plt
import sys
filepath1 = sys.argv[1]
filepath2 = sys.argv[2]
filepath3 = sys.argv[3]

cc1 = sys.argv[4]
cc2 = sys.argv[5]
bw = sys.argv[6]
"""delay = sys.argv[7]
retr1 = sys.argv[8]
retr2 = sys.argv[9]
loss  = sys.argv[10]"""
with open(filepath1, 'r') as f:
    lines = f.readlines()
    x = [float(line.split()[0]) for line in lines]
    y = [float(line.split()[1]) for line in lines]
with open(filepath2, 'r') as f:
    lines = f.readlines()
    x1 = [float(line.split()[0]) for line in lines]
    y1 = [float(line.split()[1]) for line in lines]

with open(filepath3, 'r') as f:
    lines = f.readlines()
    x2 = [float(line.split()[0]) for line in lines]
    y2 = [float(line.split()[1]) for line in lines]

plt.xlabel('Time (s)')
plt.ylabel('Goodput (Mbps)')
#plt.title(bw + 'Mbps, delay ' + delay + 'ms loss ' + loss + '%' )
vegas, bpf_cubic, tt = plt.plot(x ,y, x1, y1, x2, y2)
plt.legend([vegas, (vegas, bpf_cubic)], [ cc1, cc2])


plt.show()
