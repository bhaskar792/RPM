# RPM
Round Trip Time Per Minute
This was a project proposed by Matt Mathis, Google for pointing out the significance of RTT on a webpage. Code involves contributions from Toke Høiland-Jørgensen, Redhat.

## Team
Ajay Bharadwaj
Bhaskar Kataria
Narayan Pai

Here we implement a qdisc which contains two queues, input queue and output queue. Input queue buffers data for a specific interval and after the interval is completed, both the queues get exchanged and pending packets in output queue gets removed.

## steps to run
sudo python3 run-shell.py - It will setup a namespace based topology (h1---h2) and ping from h1 to h2


## To install on interface
- sudo ./install-module
- sudo ./install-tc-support
- sudo tc qdisc replace dev <Iface> root rg interval 100  (interval in ms)
