# import socket
# import sys

# if len(sys.argv) != 3:
#     print("Usage: python3 main.py <ON_IP> <VN_UDP_PORT>")
#     sys.exit(1)

# on_ip = sys.argv[1]
# vn_udp_port = int(sys.argv[2])
# vn_ip = "172.23.32.143"

# udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# try:
#     udp_sock.bind((vn_ip,vn_udp_port))
#     print("vn udp_sock bound....")

#     sockfd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
#     sockfd.connect((on_ip,5000))
#     print("vn connected to on....")
        
#     msg = f"{vn_ip} {vn_udp_port}"
#     sockfd.sendall(msg.encode())
#     print("after connect info sent :",msg)

#     buf = sockfd.recv(1023)
#     if buf:
#         neigh=buf.decode()
#         print("neigh info :", neigh)
#     else:
#         print("connection closed by on")

# except OSError as e:
#     print("socket error : ", e)

# finally:    
#     if "sockfd" in locals():
#         sockfd.close()
#     udp_sock.close()


import socket

on_ip = "172.23.32.143"
vn_ip = "172.23.32.143"
vn_udp_port = 5001

udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

try:
    udp_sock.bind((vn_ip,vn_udp_port))
    print("vn udp_sock bound....")

    sockfd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sockfd.connect((on_ip,5000))
    print("vn connected to on....")
        
    msg = f"{vn_ip} {vn_udp_port}"
    sockfd.sendall(msg.encode())
    print("after connect info sent :",msg)

    buf = sockfd.recv(1023)
    if buf:
        neigh=buf.decode()
        print("neigh info :", neigh)
    else:
        print("connection closed by on")

except OSError as e:
    print("socket error : ", e)

finally:    
    if "sockfd" in locals():
        sockfd.close()
    udp_sock.close()

