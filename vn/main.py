import socket
HOST = "127.0.0.1"
PORT = 5000

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.connect((HOST, PORT))
print("Connected to ON")

message = "A"
client.send(message.encode())
print("Sent: A")

data = client.recv(1024)
print("Received LINK-STATE:")
print(data.decode())

client.close()