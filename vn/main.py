import socket

sockfd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
dest_addr = ("172.23.32.143",5000)

try:
    sockfd.connect(dest_addr)
    print("vn(me) connected....")
        
    msg = "10.0.0.1 5001"
    sockfd.sendall(msg.encode())
    print("msg sent :",msg)

    buf = sockfd.recv(1023)
    if buf:
        print("received msg :", buf.decode())
    else:
        print("connection closed by on")
except OSError as e:
    print("socket error : ", e)

finally:    
    sockfd.close()


