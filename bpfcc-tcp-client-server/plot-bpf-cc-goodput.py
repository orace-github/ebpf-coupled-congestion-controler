from cProfile import label
import matplotlib.pyplot as plt
import sys
filepath = sys.argv[1]
cc = sys.argv[2]
with open(filepath, 'r') as f:
    lines = f.readlines()
    x = [float(line.split()[0]) for line in lines]
    y = [float(line.split()[1]) for line in lines]

plt.xlabel('Time (s)')
plt.ylabel('Goodput (Mbps)')
#plt.title(bw + 'Mbps, delay ' + delay + 'ms loss ' + loss + '%' )
bpfcc = plt.plot(x ,y, label=cc)
plt.legend()
plt.show()
