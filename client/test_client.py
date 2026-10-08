import socket
import time
import sys

def send_command(s, *args):
    # Formulate RESP Array: *<len>\r\n$<len>\r\n<arg>...
    resp = f"*{len(args)}\r\n"
    for arg in args:
        arg_str = str(arg)
        resp += f"${len(arg_str)}\r\n{arg_str}\r\n"
    s.sendall(resp.encode('utf-8'))
    data = s.recv(4096).decode('utf-8')
    return data

def parse_resp(data):
    if not data:
        return None
    prefix = data[0]
    content = data[1:].strip()
    if prefix == '+':
        return content # Simple string
    elif prefix == '-':
        return f"Error: {content}"
    elif prefix == ':':
        return int(content) # Integer
    elif prefix == '$':
        # Bulk string
        lines = data.split('\r\n')
        length = int(lines[0][1:])
        if length == -1:
            return None
        return lines[1]
    elif prefix == '*':
        # Array
        lines = data.split('\r\n')
        count = int(lines[0][1:])
        results = []
        i = 1
        while i < len(lines) and len(results) < count:
            if lines[i].startswith('$'):
                l = int(lines[i][1:])
                if l == -1:
                    results.append(None)
                else:
                    results.append(lines[i+1])
                    i += 1
            i += 1
        return results
    return data

def run_tests(port=6379):
    print(f"Connecting to MiniRedis server at 127.0.0.1:{port}...")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', port))
    print("[CONNECTED] TCP handshake successful!\n")

    # 1. PING
    res = parse_resp(send_command(s, "PING"))
    print(f"PING -> {res} (Expected: PONG)")
    assert res == "PONG"

    # 2. SET & GET
    res = parse_resp(send_command(s, "SET", "user", "baadaldev"))
    print(f"SET user baadaldev -> {res} (Expected: OK)")
    assert res == "OK"

    res = parse_resp(send_command(s, "GET", "user"))
    print(f"GET user -> {res} (Expected: baadaldev)")
    assert res == "baadaldev"

    # 3. Numeric INCR & INCRBY
    res = parse_resp(send_command(s, "INCR", "counter"))
    print(f"INCR counter -> {res} (Expected: 1)")
    assert res == 1

    res = parse_resp(send_command(s, "INCRBY", "counter", 50))
    print(f"INCRBY counter 50 -> {res} (Expected: 51)")
    assert res == 51

    # 4. List RPUSH & LRANGE
    res = parse_resp(send_command(s, "RPUSH", "technologies", "C++", "Redis", "DistributedSystems"))
    print(f"RPUSH technologies -> {res} items (Expected: 3)")
    assert res == 3

    res = parse_resp(send_command(s, "LRANGE", "technologies", 0, -1))
    print(f"LRANGE technologies 0 -1 -> {res}")
    assert res == ["C++", "Redis", "DistributedSystems"]

    # 5. TTL & Expiration
    send_command(s, "SET", "short_lived_key", "secret123", "EX", 1)
    ttl = parse_resp(send_command(s, "TTL", "short_lived_key"))
    print(f"TTL short_lived_key -> {ttl}s (Expected: 1)")

    print("Sleeping 1.5s for TTL expiration...")
    time.sleep(1.5)
    expired_get = parse_resp(send_command(s, "GET", "short_lived_key"))
    print(f"GET short_lived_key after expire -> {expired_get} (Expected: None)")
    assert expired_get is None

    # 6. Hash Operations (HSET, HGET, HEXISTS, HLEN, HGETALL, HDEL)
    res = parse_resp(send_command(s, "HSET", "profile:baadal", "name", "Baadal", "role", "Engineer"))
    print(f"HSET profile:baadal -> {res} (Expected: 2)")
    assert res == 2

    res = parse_resp(send_command(s, "HGET", "profile:baadal", "name"))
    print(f"HGET profile:baadal name -> {res} (Expected: Baadal)")
    assert res == "Baadal"

    res = parse_resp(send_command(s, "HEXISTS", "profile:baadal", "role"))
    print(f"HEXISTS profile:baadal role -> {res} (Expected: 1)")
    assert res == 1

    res = parse_resp(send_command(s, "HLEN", "profile:baadal"))
    print(f"HLEN profile:baadal -> {res} (Expected: 2)")
    assert res == 2

    res = parse_resp(send_command(s, "HGETALL", "profile:baadal"))
    print(f"HGETALL profile:baadal -> {res}")
    assert "name" in res and "Baadal" in res and "role" in res and "Engineer" in res

    res = parse_resp(send_command(s, "HDEL", "profile:baadal", "role"))
    print(f"HDEL profile:baadal role -> {res} (Expected: 1)")
    assert res == 1

    # 7. Set Operations (SADD, SMEMBERS, SISMEMBER, SCARD, SREM)
    res = parse_resp(send_command(s, "SADD", "skills", "C++", "Redis", "DistributedSystems"))
    print(f"SADD skills -> {res} (Expected: 3)")
    assert res == 3

    res = parse_resp(send_command(s, "SCARD", "skills"))
    print(f"SCARD skills -> {res} (Expected: 3)")
    assert res == 3

    res = parse_resp(send_command(s, "SISMEMBER", "skills", "C++"))
    print(f"SISMEMBER skills C++ -> {res} (Expected: 1)")
    assert res == 1

    res = parse_resp(send_command(s, "SISMEMBER", "skills", "Rust"))
    print(f"SISMEMBER skills Rust -> {res} (Expected: 0)")
    assert res == 0

    res = parse_resp(send_command(s, "SMEMBERS", "skills"))
    print(f"SMEMBERS skills -> {res}")
    assert isinstance(res, list) and len(res) == 3

    res = parse_resp(send_command(s, "SREM", "skills", "DistributedSystems"))
    print(f"SREM skills DistributedSystems -> {res} (Expected: 1)")
    assert res == 1

    # 8. Type command
    res = parse_resp(send_command(s, "TYPE", "user"))
    print(f"TYPE user -> {res} (Expected: string)")
    assert res == "string"

    res = parse_resp(send_command(s, "TYPE", "skills"))
    print(f"TYPE skills -> {res} (Expected: set)")
    assert res == "set"

    # 9. Multi MSET & MGET
    res = parse_resp(send_command(s, "MSET", "lang1", "C++", "lang2", "Python"))
    print(f"MSET lang1 C++ lang2 Python -> {res} (Expected: OK)")
    assert res == "OK"

    res = parse_resp(send_command(s, "MGET", "lang1", "lang2", "nonexistent_key"))
    print(f"MGET lang1 lang2 nonexistent_key -> {res}")
    assert res == ["C++", "Python", None]

    # 10. DBSIZE
    dbsize = parse_resp(send_command(s, "DBSIZE"))
    print(f"DBSIZE -> {dbsize} keys")

    s.close()
    print("\n=======================================================")
    print("  ALL CLIENT LIVE INTEGRATION TESTS PASSED SUCCESSFULLY! ")
    print("=======================================================\n")

if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 6379
    run_tests(port)
