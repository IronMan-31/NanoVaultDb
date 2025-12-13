import socket
import time

s = socket.socket()
s.connect(("127.0.0.1", 6969))

s.sendall(b"CREATE DATABASE demo;")  

s.sendall(b"CREATE TABLE demo (")
time.sleep(0.2)

s.sendall(b"id int")
time.sleep(0.2)

s.sendall(b");")
print(s.recv(4096).decode())

# Multiple SQLs in one send
s.sendall(b"DROP TABLE demo;DROP DATABASE demo;")
print(s.recv(4096).decode())
print(s.recv(4096).decode())

# Exit
s.sendall(b"exit;")
s.close()
