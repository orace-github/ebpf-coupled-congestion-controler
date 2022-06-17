#!/usr/bin/python3

from mininet.topo import Topo
from mininet.net import Mininet
from mininet.node import Node
from mininet.log import setLogLevel, info
from mininet.cli import CLI
import sys
import os
import time

"""            c3    s1    
               |     |
     topo      |     |
         c1----r0--r1----s2
               |    | 
               |    | 
               |    | 
              c2    s3   
"""

class LinuxRouter( Node ):
    "A Node with IP forwarding enabled."

    def config( self, **params ):
        super( LinuxRouter, self).config( **params )
        # Enable forwarding on the router
        self.cmd( 'sysctl net.ipv4.ip_forward=1' )
        self.cmd( 'sysctl net.ipv6.conf.all.forwarding=1' )

    def terminate( self ):
        self.cmd( 'sysctl net.ipv4.ip_forward=0' )
        self.cmd( 'sysctl net.ipv6.conf.all.forwarding=0' )
        super( LinuxRouter, self ).terminate()
        
class NetworkTopo( Topo ):
    
    def build( self, **_opts ):
        r0IP = '192.168.0.1/24'  # IP address for r0-eth1
        r1IP = '192.168.1.1/24'  # IP address for r1-eth1
        
        r0 = self.addNode( 'r0', cls=LinuxRouter, ip=r0IP )
        r1 = self.addNode( 'r1', cls=LinuxRouter, ip=r1IP )
        
        c1 = self.addHost( 'c1', ip='192.168.0.100/24',
                           defaultRoute='via 192.168.0.1' )
        s1 = self.addHost( 's1', ip='192.168.3.100/24',
                           defaultRoute='via 192.168.3.1' )
                           
        c2 = self.addHost( 'c2', ip='192.168.4.100/24',
                           defaultRoute='via 192.168.4.1' )
        s2 = self.addHost( 's2', ip='192.168.5.100/24',
                           defaultRoute='via 192.168.5.1' )
                           
        c3 = self.addHost( 'c3', ip='192.168.6.100/24',
                           defaultRoute='via 192.168.6.1' )
        s3 = self.addHost( 's3', ip='192.168.7.100/24',
                           defaultRoute='via 192.168.7.1' )
                           
        self.addLink( c1, r0, intfName2='r0-eth1',
                      params2={ 'ip' : r0IP} )  
        self.addLink( c2, r0, intfName2='r0-eth3',
                      params2={ 'ip' : '192.168.4.1/24'} ) 
        self.addLink( c3, r0, intfName2='r0-eth4',
                      params2={ 'ip' : '192.168.6.1/24'} )  
        self.addLink( r0, r1,  intfName1='r0-eth2', intfName2='r1-eth1', params1={ 'ip' : '192.168.1.2/24' },params2={ 'ip' : '192.168.1.1/24' })          
        
        self.addLink( r1, s1, intfName1='r1-eth2',
                      params1={ 'ip' : '192.168.3.1/24' } )
        self.addLink( r1, s2, intfName1='r1-eth3',
                      params1={ 'ip' : '192.168.5.1/24' })
                      
        self.addLink( r1, s3, intfName1='r1-eth4',
                      params1={ 'ip' : '192.168.7.1/24' })
                           

    
def run_s1(net):
    """
    net['s1'].cmd('./server 192.168.3.100 4441 sr.log ss.log &')
    """
    time.sleep(1)
    
    
def run_c1(net):
    """
    net['c1'].cmd('time bash test.sh s.mp4 192.168.3.100 4441 &')
    """
    time.sleep(1)
    

def run_s2(net):
    time.sleep(4)
    
    
def run_c2(net):
    time.sleep(4)
    
def run_s3(net):
    time.sleep(4)
    
    
def run_c3(net):
    time.sleep(4)
    
    
def run():
    topo = NetworkTopo()
    net = Mininet( topo=topo )  
    net.start()
    net['c1'].cmd('ip -6 addr add fc00::2/48 dev c1-eth0')
    net['c1'].cmd('ethtool -K c1-eth0 tso off gso off gro off lro off')
    net['c1'].cmd("sysctl -w net.ipv4.tcp_rmem='10240 87380 " + buff_sizestr + "'")
    net['c1'].cmd("sysctl -w net.ipv4.tcp_wmem='10240 87380 " + buff_sizestr + "'")
    
    net['s1'].cmd('ip -6 addr add fc00:0:3::2/48 dev s1-eth0')
    net['s1'].cmd('ethtool -K s1-eth0 tso off gso off gro off lro off')
    net['s1'].cmd("sysctl -w net.ipv4.tcp_rmem='10240 87380 " + buff_sizestr + "'")
    net['s1'].cmd("sysctl -w net.ipv4.tcp_wmem='10240 87380 " + buff_sizestr + "'")
    
    net['s2'].cmd('ip -6 addr add fc00:0:5::2/48 dev s2-eth0')
    net['s2'].cmd('ethtool -K s2-eth0 tso off gso off gro off lro off')
    net['s2'].cmd("sysctl -w net.ipv4.tcp_rmem='10240 87380 " + buff_sizestr + "'")
    net['s2'].cmd("sysctl -w net.ipv4.tcp_wmem='10240 87380 " + buff_sizestr + "'")
    
    net['c2'].cmd('ip -6 addr add fc00:0:4::2/48 dev c2-eth0')
    net['c2'].cmd('ethtool -K c2-eth0 tso off gso off gro off lro off')
    net['c2'].cmd("sysctl -w net.ipv4.tcp_rmem='10240 87380 " + buff_sizestr + "'")
    net['c2'].cmd("sysctl -w net.ipv4.tcp_wmem='10240 87380 " + buff_sizestr + "'")
    
    net['c3'].cmd('ip -6 addr add fc00:0:6::2/48 dev c3-eth0')
    net['c3'].cmd('ethtool -K c3-eth0 tso off gso off gro off lro off')
    net['c3'].cmd("sysctl -w net.ipv4.tcp_rmem='10240 87380 " + buff_sizestr + "'")
    net['c3'].cmd("sysctl -w net.ipv4.tcp_wmem='10240 87380 " + buff_sizestr + "'")
    
    net['s3'].cmd('ip -6 addr add fc00:0:7::2/48 dev s3-eth0')
    net['s3'].cmd('ethtool -K s3-eth0 tso off gso off gro off lro off')
    net['s3'].cmd("sysctl -w net.ipv4.tcp_rmem='10240 87380 " + buff_sizestr + "'")
    net['s3'].cmd("sysctl -w net.ipv4.tcp_wmem='10240 87380 " + buff_sizestr + "'")
    
    net['r0'].cmd('ip -6 addr add fc00::1/48 dev r0-eth1')
    net['r0'].cmd('ip -6 addr add fc00:0:1::1/48 dev r0-eth2')
    net['r0'].cmd('ip -6 addr add fc00:0:4::1/48 dev r0-eth3')
    net['r0'].cmd('ip -6 addr add fc00:0:6::1/48 dev r0-eth4')
    net['r0'].cmd('ethtool -K r0-eth1 tso off gso off gro off lro off')
    net['r0'].cmd('ethtool -K r0-eth2 tso off gso off gro off lro off')
    net['r0'].cmd('tc qdisc add dev r0-eth2 root handle 1: tbf rate ' + rate +'mbit burst '+ burststr + ' limit '+ limitstr)
    net['r0'].cmd('tc qdisc add dev r0-eth2 parent 1: handle 2: netem delay '+delay+'ms ' +jitter+ 'ms  loss ' + loss + '%')
    
    net['r1'].cmd('ip -6 addr add fc00:0:1::2/48 dev r1-eth1')
    net['r1'].cmd('ip -6 addr add fc00:0:3::1/48 dev r1-eth2')
    net['r1'].cmd('ip -6 addr add fc00:0:5::1/48 dev r1-eth3')
    net['r1'].cmd('ip -6 addr add fc00:0:7::1/48 dev r1-eth4')
    net['r1'].cmd('ethtool -K r1-eth1 tso off gso off gro off lro off')
    net['r1'].cmd('ethtool -K r1-eth2 tso off gso off gro off lro off')
    net['r1'].cmd('tc qdisc add dev r1-eth1 root handle 1: tbf rate ' + rate +'mbit burst '+ burststr + ' limit '+ limitstr)
    net['r1'].cmd('tc qdisc add dev r1-eth1 parent 1: handle 2: netem delay '+delay+'ms ' +jitter+ 'ms  loss ' + loss + '%')
    
    net['s1'].cmd('ip -6 route add default via fc00:0:3::1 dev s1-eth0')   
    net['c1'].cmd('ip -6 route add default via fc00::1 dev c1-eth0')
    net['s2'].cmd('ip -6 route add default via fc00:0:5::1 dev s2-eth0') 
    net['c2'].cmd('ip -6 route add default via fc00:0:4::1 dev c2-eth0') 
    net['s3'].cmd('ip -6 route add default via fc00:0:7::1 dev s3-eth0') 
    net['c3'].cmd('ip -6 route add default via fc00:0:6::1 dev c3-eth0') 
    
    net['r0'].cmd('ip -6 route add fc00:0:3::/48 via fc00:0:1::2 dev r0-eth2')
    net['r0'].cmd('ip -6 route add fc00:0:5::/48 via fc00:0:1::2 dev r0-eth2')
    net['r0'].cmd('ip -6 route add fc00:0:7::/48 via fc00:0:1::2 dev r0-eth2')
    net['r1'].cmd('ip -6 route add fc00::/48 via fc00:0:1::1 dev r1-eth1')
    net['r1'].cmd('ip -6 route add fc00:0:4::/48 via fc00:0:1::1 dev r1-eth1')
    net['r1'].cmd('ip -6 route add fc00:0:6::/48 via fc00:0:1::1 dev r1-eth1')
    
    net['c1'].cmd('ip route add default via 192.168.0.1 dev c1-eth0')
    net['c2'].cmd('ip route add default via 192.168.4.1 dev c2-eth0')
    net['c3'].cmd('ip route add default via 192.168.6.1 dev c3-eth0')
    net['s1'].cmd('ip route add default via 192.168.3.1 dev s1-eth0')
    net['s2'].cmd('ip route add default via 192.168.5.1 dev s2-eth0')
    net['s3'].cmd('ip route add default via 192.168.7.1 dev s3-eth0')
    
    
    net['r0'].cmd('ip route add 192.168.3.0/24 via 192.168.1.1 dev r0-eth2')
    net['r0'].cmd('ip route add 192.168.5.0/24 via 192.168.1.1 dev r0-eth2')
    net['r0'].cmd('ip route add 192.168.7.0/24 via 192.168.1.1 dev r0-eth2')
    net['r1'].cmd('ip route add 192.168.0.0/24 via 192.168.1.2 dev r1-eth1')
    net['r1'].cmd('ip route add 192.168.4.0/24 via 192.168.1.2 dev r1-eth1')
    net['r1'].cmd('ip route add 192.168.6.0/24 via 192.168.1.2 dev r1-eth1')
    
    """
       limit = 2*delay*rate,  delay(s), rate(bit/s)
       burst = 15KB
    
    
    net['r0'].cmd('tc qdisc add dev r0-eth2 root handle 1: tbf rate ' + rate +'mbit burst '+ burststr + ' limit '+ limitstr)
    net['r0'].cmd('tc qdisc add dev r0-eth1 parent 1: handle 2: netem delay '+delay+'ms ' +jitter+ 'ms  loss ' + loss + '%')
    net['r0'].cmd('tc qdisc add dev r0-eth2 parent 1: handle 2: netem delay '+delay+'ms ' +jitter+ 'ms  loss ' + loss + '%')
        
    """
    pid1 = os.fork()
    if(pid1 == 0):
        run_s1(net)
    else: 
        pid2 = os.fork()
        if(pid2 == 0):
            run_s2(net)
        else:
            time.sleep(2)
            pid3 = os.fork()
            if(pid3 == 0):
                run_c1(net)
            else:
                pid4 = os.fork()
                if(pid4 == 0):
                    run_c2(net)
                else:
                    for i in [1 , 2,  3 , 4]:
                        i = os.wait()
                        print(i)             
                    CLI( net )
                    net.stop()
if __name__ == '__main__':
    setLogLevel( 'info' )
    delay = sys.argv[1]
    rate =  sys.argv[2]
    loss =  sys.argv[3]
    jitter = sys.argv[4]
    limit = 2 * int(delay) * int(rate) * 1000
    burst = (int(rate) * 1000000)/(250*8)
    limitstr = str(limit)
    burststr = str(burst)
    buff_size = 16*int(rate)*1000000*int(delay)*0.001/8
    print(buff_size)
    buff_sizestr = str(int(buff_size))
    run()
