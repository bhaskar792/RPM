
import time

import multiprocessing
import os


# TOPOLOGY
#   h1 ---- h2


# Verify no errors in qdisc
if os.system('./install-module') != 0:
    exit()
if os.system('./install-tc-support') != 0: 
    exit()
# Create nodes

os.system("ip netns add h1")
os.system("ip netns exec h1 ip link set dev lo up")
os.system("ip netns add h2")
os.system("ip netns exec h2 ip link set dev lo up")
os.system("ip link add h2_h1 type veth peer name h1_h2")
os.system("ip link set h2_h1 netns h2")
os.system("ip link set h1_h2 netns h1")

os.system("ip netns exec h2 ip link set dev h2_h1 up")
os.system("ip netns exec h1 ip link set dev h1_h2 up")
os.system("ip netns exec h1 ip address add 10.0.1.2/24 dev h1_h2")
os.system("ip netns exec h2 ip address add 10.0.1.1/24 dev h2_h1")

os.system('tc -n h1 qdisc replace dev h1_h2 root rg')


def sender_proc():
    os.system('sudo ip netns exec h1 ping 10.0.1.1 -A')

# Ensure routers and receivers have started
time.sleep(1)


sender_process = multiprocessing.Process(
    target=sender_proc)
sender_process.start()

sender_process.join()

print('--h1--')
os.system('sudo ip netns exec h1 tc -s qdisc show')

print('--h2--')
os.system('sudo ip netns exec h2 tc -s qdisc show')

# Cleanup
os.system('sudo ip netns delete h1')
os.system('sudo ip netns delete h2')
