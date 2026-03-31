import pty, os, time, sys, select

def test_pty():
    m, s = pty.openpty()
    pid = os.fork()
    if pid == 0:
        os.setsid()
        os.dup2(s, 0)
        os.dup2(s, 1)
        os.dup2(s, 2)
        os.close(m)
        os.close(s)
        os.execl("build/main", "build/main")
    else:
        os.close(s)
        time.sleep(2)  # Wait for DB to start

        # Send CREATE HFT TABLE command
        create_sql = b'CREATE HFT TABLE btc_ticks (timestamp DOUBLE PRECISION 0, price DOUBLE PRECISION 10, volume DOUBLE PRECISION 2, side DOUBLE PRECISION 0) SYMBOL 1 TOP;\n'
        os.write(m, create_sql)
        time.sleep(1)

        # Output to file
        with open("/tmp/db_pty_out.txt", "wb") as f:
            while True:
                r, _, _ = select.select([m], [], [], 2.0)
                if not r:
                    break
                try:
                    data = os.read(m, 4096)
                    if not data:
                        break
                    f.write(data)
                except OSError:
                    break

if __name__ == '__main__':
    test_pty()
