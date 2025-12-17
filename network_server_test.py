import socket
import time

s = socket.socket()
s.connect(("127.0.0.1", 6969))

s.sendall(b"MEMORY KEY=a VALUES=123 TTL=5;")  
print(s.recv(4096).decode())

s.sendall(b"MEMORY GET KEY=a;")  
print(s.recv(4096).decode())

time.sleep(5)

s.sendall(b"MEMORY GET KEY=a;")  
print(s.recv(4096).decode())

# Exit
s.sendall(b"exit;")
s.close()
