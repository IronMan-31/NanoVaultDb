# NanoVDB 🚀

NanoVDB is a lightweight **C++ based relational database engine** built from scratch.  
It supports **persistent storage, indexing, concurrency, networking, TTL memory store, and background vacuuming**, all without external database dependencies.

---

## 👨‍💻 Authors

- **Shivam Kumar**
- **Pranab Pandey**

---

## ✨ Features

### 📦 Core Database
- CREATE / DROP DATABASE
- CREATE / DROP TABLE
- INSERT
- SELECT
- UPDATE 
- DELETE
- WHERE clause evaluation
- AUTO_INCREMENT primary keys
- Persistent on-disk storage
- Memory commands for temporary storage

### 🌳 Indexing
- **B+ Tree indexing**
- Primary key index
- Unique column index support
- Automatic rebuild on server restart
- Index-safe UPDATE and DELETE

### 🧹 Vacuum System
- Background **vacuum thread**
- Cleans deleted rows from `.data` and `.index`
- Rewrites compact files
- Rebuilds B+ Trees after cleanup
- Runs automatically on restart and periodically

### 🧠 In-Memory Store
- Redis-like MEMORY commands
- TTL support
- Background expiry scheduler
- Thread-safe using shared mutexes

### 🌐 Network Server
- TCP-based SQL server
- Multi-client support
- Thread-per-client model
- Graceful shutdown with signal handling
- Safe background thread cleanup

---

## 🗂️ Storage Layout

```bash
db/
├── school.shivam.db # Database metadata and tables Schema (JSON)
├── tables/
│ └── school/
│ ├── studentrolls.data
│ ├── studentrolls.index
│ └── studentrolls.delete
```

- `.data`   → raw column values
- `.index`  → row index + offsets
- `.delete` → tombstone flags
- `.shivam.db` → schema metadata

---

### Architecture Overview

### Insert
1. Validate schema & constraints
2. Allocate primary key
3. Append data to `.data`
4. Write offsets to `.index`
5. Update B+ Tree

### Delete
1. Mark row as deleted in `.delete`
2. Keep data intact (lazy delete)

### Update
1. Scan rows
2. Match WHERE clause
3. Mark old row deleted
4. Insert new row with updated values
5. Maintain index consistency

### Vacuum
1. Skip deleted rows
2. Rewrite compact `.data` and `.index`
3. Replace old files atomically
4. Rebuild B+ Trees

---

## 🧪 Tested Scenarios

- Insert → Update → Delete → Restart → Vacuum
- Cross-database operations
- Persistent recovery after restart
- AUTO_INCREMENT correctness
- B+ Tree rebuild integrity
- Concurrent client queries
- Memory TTL expiry
- Crash-safe shutdown

---

##  Network Usage

### For testing

- For general purpose testing of DB we have created a main.cpp file in that file inside the testSQLs vector we have put down some examples. For running it

```bash
g++ main.cpp -o main.exe
./main.exe
```

### Start Server

- For communicating with the network server. Run -
```bash
g++ network_server.cpp -o network.exe
./network.exe
```

### Example Client

- Here below is an example python file for communicating with server after running it.
- We are also providing you with a sample python file for communicating 

```bash
network_server_test.py
```

```bash
import socket

s = socket.socket()
s.connect(("127.0.0.1", 6969))

s.sendall(b"SELECT * FROM StudentRolls;")
print(s.recv(4096).decode())

s.sendall(b"exit;")
s.close()
```

## Memory Commands

- For temporary storage like for OTP's Memory commands are also there.
- There are 10 threads allocated for handling memory set commands. One thread apart from them is there for cleaning expired keys. 

```bash
MEMORY KEY=a VALUES=123 TTL=5;
MEMORY GET KEY=a;
```

## Concurrency Model

- Global DB mutex for schema operations.
- Table-level mutexes for row operations.
- Shared mutex for memory store.
- Atomic shutdown flags.
- Safe background thread joins.

## Why NanoVDb?

- Demonstrates real DB internals:

Indexing
Storage engines
Vacuuming
Transaction-like updates

- Excellent foundation for:

WAL
Transactions
MVCC
Query planner
Joins
