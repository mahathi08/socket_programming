import socket

sockfd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
dest_addr = ("127.0.0.1",12345)

sockfd.connect(dest_addr)
print("vn(me) connected....")
    
msg = "A"
sockfd.send(msg.encode())
print("msg sent :",msg)

buf = sockfd.recv(1023)
if buf:
    print("received msg :", buf.decode())

sockfd.close()


