import socket
import time

def send(sock, cmd):
    print(">>", cmd.strip())
    sock.sendall(cmd.encode())
    time.sleep(0.1)
    resp = sock.recv(8192).decode()
    print(resp)

s = socket.socket()
s.connect(("127.0.0.1", 6970))
send(s, "ADD STRATEGY FROM FILE '/home/pranab/nano_db_latest/m_way_tree.cpp';")
send(s, '''ENABLE STRATEGY "again" ("10") on symbol 1 column_no 1 ticks 100;''')
send(s,";")


# send(s, """
# CREATE TABLE StudentRolls (
#     id INT PRIMARY KEY AUTO_INCREMENT,
#     roll_no INT NOT NULL
# );
# """)

# send(s, "INSERT INTO StudentRolls (roll_no) VALUES (10);")
# send(s, "INSERT INTO StudentRolls (roll_no) VALUES (20);")
# send(s, "INSERT INTO StudentRolls (roll_no) VALUES (30);")

# send(s, "SELECT * FROM StudentRolls;")

# send(s, 'DELETE FROM StudentRolls WHERE roll_no < "25";')

# send(s, "SELECT * FROM StudentRolls;")

# s.close()
# # time.sleep(2)

# s = socket.socket()
# s.connect(("127.0.0.1", 6969))

# send(s, "USE vaccumtest;")
# send(s, "INSERT INTO StudentRolls (roll_no) VALUES (10);")

# send(s, "SELECT * FROM StudentRolls;")

# Exit cleanly
send(s, "exit;")
s.close()
