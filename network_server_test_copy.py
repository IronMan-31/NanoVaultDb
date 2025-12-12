import socket

s = socket.socket()
s.connect(("127.0.0.1", 6969))

s.sendall(b"CREATE DATABASE demo")
print(s.recv(4096).decode())

s.sendall(b"CREATE TABLE demo (id int);\n")
print(s.recv(4096).decode())

s.sendall(b"DROP TABLE demo\n")
print(s.recv(4096).decode())

s.sendall(b"DROP DATABASE demo")
print(s.recv(4096).decode())

while True : # for testing multi clients
    pass
s.sendall(b"exit")
s.close()
