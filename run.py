from nest.experiment import *
from nest.topology import *
from nest.routing.routing_helper import RoutingHelper
import nest.config as config
import time

import multiprocessing
import os



config.set_value('assign_random_names', False)
# config.set_value('delete_namespaces_on_termination', False)

# TOPOLOGY
#   h1 ---- h2


# Verify no errors in qdisc
if os.system('./install-module') != 0:  # TODO doesn't seem to work
    exit()
if os.system('./install-tc-support') != 0: 
    exit()
# Create nodes

#h = host
h1 = Node('h1')
h2 = Node('h2')

# Create interfaces
(h2_h1, h1_h2) = connect(h2, h1, interface1_name='h2_h1', interface2_name='h1_h2')

# Set IPv4 Addresses
h1_h2.set_address('10.0.1.2/24')
h2_h1.set_address('10.0.1.1/24')



def sender_proc():
    os.system('ping 10.0.1.1 -i 0.1')


def setup_host(node, interfaces):
    with node:
        for interface in interfaces:
            os.system('tc qdisc replace dev ' + interface.name + ' root rg')
            tcpdump_process = multiprocessing.Process (target = tcpdump_proc, args=(interface,))
            tcpdump_process.start ()

def tcpdump_proc (interface):
    os.system ('timeout 10 tcpdump -i ' + interface.name + ' -w ' + interface.name +'.pcap')

setup_host(h1, [h1_h2])


with h2:
    receiver_process = multiprocessing.Process(
        target=tcpdump_proc, args=(h2_h1,))
    receiver_process.start()

# Ensure routers and receivers have started
time.sleep(1)

with h1:
    sender_process = multiprocessing.Process(
        target=sender_proc)
    sender_process.start()
sender_process.join()
receiver_process.join()


## Show tc qdisc stats
with h1:
    print('--h1--')
    os.system('tc -s qdisc show')
with h2:
    print('--h2--')
    os.system('tc -s qdisc show')

