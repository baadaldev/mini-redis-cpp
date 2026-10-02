import socket
import time
import sys

def benchmark(port=6379, total_ops=5000):
    print(f"============================================================")
    print(f"  MiniRedis C++ High-Throughput Performance Benchmark       ")
    print(f"  Target: 127.0.0.1:{port} | Operations: {total_ops} per suite")
    print(f"============================================================\n")

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', port))

    def run_suite(name, cmd_generator):
        start = time.perf_counter()
        for i in range(total_ops):
            args = cmd_generator(i)
            resp = f"*{len(args)}\r\n"
            for a in args:
                s_a = str(a)
                resp += f"${len(s_a)}\r\n{s_a}\r\n"
            s.sendall(resp.encode('utf-8'))
            res = s.recv(1024)
        elapsed = time.perf_counter() - start
        ops_per_sec = total_ops / elapsed
        avg_latency_ms = (elapsed / total_ops) * 1000
        print(f"  [{name:6}] {total_ops} ops in {elapsed:.3f}s | {ops_per_sec:10.1f} ops/sec | Avg latency: {avg_latency_ms:.3f} ms")

    run_suite("PING", lambda i: ["PING"])
    run_suite("SET",  lambda i: ["SET", f"bench_key_{i}", f"val_{i}"])
    run_suite("GET",  lambda i: ["GET", f"bench_key_{i}"])
    run_suite("INCR", lambda i: ["INCR", "bench_counter"])

    s.close()
    print("\nBenchmark completed successfully!\n")

if __name__ == "__main__":
    p = int(sys.argv[1]) if len(sys.argv) > 1 else 6379
    ops = int(sys.argv[2]) if len(sys.argv) > 2 else 5000
    benchmark(p, ops)
