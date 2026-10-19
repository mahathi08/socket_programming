import socket
import sys
import struct

def receive(sockfd,size):
    data = bytearray()
    while len(data) < size:
        b = sockfd.recv(size - len(data))
        if not b:
            raise ConnectionError("Connection closed")
        data.extend(b)
    return bytes(data)

def send_frame(sockfd,tmsg,body):
    sockfd.sendall(struct.pack("!cH",tmsg,len(body))+body)

def receive_frame(sockfd):
    h=receive(sockfd,3)
    tmsg,l=struct.unpack("!cH",h)
    return tmsg,receive(sockfd,l)


if len(sys.argv) != 3:
    print("Usage: python3 main.py <ON_IP> <VN_UDP_PORT>")
    sys.exit(1)

on_ip = sys.argv[1]
vn_udp_port = int(sys.argv[2])
vn_ip = "172.23.32.143"
vn_name = chr(ord('A')+vn_udp_port-5001)

udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

try:
    udp_sock.bind((vn_ip,vn_udp_port))
    # print(f"vn({vn_udp_port})udp_sock bound....")

    sockfd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sockfd.connect((on_ip,5000))
    print(f"vn({vn_name}) connected to on....")
        
    conn = (socket.inet_aton(vn_ip)+struct.pack("!H",vn_udp_port))
    send_frame(sockfd,b"C",conn)

    tmsg,bod=receive_frame(sockfd)
    if tmsg != b"L":
        raise ValueError("Unexpected message type")
    k=bod[0]
    if len(bod)!=1+k*11:
        raise ValueError("invalid link-state length")
    neigh = []
    p = 1
    for i in range(k):
        nod=chr(bod[p])
        p+=1
        ip=socket.inet_ntoa(bod[p:p+4])
        p+=4
        port=struct.unpack("!H",bod[p:p+2])[0]
        p+=2
        cost=struct.unpack("!I",bod[p:p+4])[0]
        p+=4
        neigh.append((nod,ip,port,cost))
    neigh.sort(key=lambda x:(x[3]!=0,x[0]))
    print(f"neigh link_state of vn({vn_name}):",",".join(f"{nod}={cost}" for nod,ip,port,cost in neigh))

finally:    
    if "sockfd" in locals():
        sockfd.close()
    udp_sock.close()
