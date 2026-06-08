#!/usr/bin/env python3
"""
Simple slow backend for testing the proxy.
Returns a response after 2 seconds to simulate latency.
"""
import socket
import time
import sys

def run_backend(port=3000):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('127.0.0.1', port))
    s.listen(10)
    
    print(f"Backend listening on port {port}...")
    print("Each request will take 2 seconds to respond\n")
    
    request_count = 0
    try:
        while True:
            try:
                client, addr = s.accept()
                request_count += 1
                req_id = request_count
                
                print(f"[{req_id}] Request from {addr[0]}")
                
                # Read request
                req = client.recv(4096).decode()
                lines = req.split('\r\n')
                print(f"[{req_id}] {lines[0]}")
                
                # Simulate processing delay
                print(f"[{req_id}] Processing... (2 seconds)")
                time.sleep(2)
                
                # Send response
                response = (
                    f"HTTP/1.1 200 OK\r\n"
                    f"Content-Type: text/plain\r\n"
                    f"Content-Length: 50\r\n"
                    f"Connection: close\r\n"
                    f"\r\n"
                    f"This is response #{req_id}, backend is working!"
                )
                
                client.sendall(response.encode())
                client.close()
                print(f"[{req_id}] Response sent\n")
                
            except Exception as e:
                print(f"Error handling request: {e}")
    
    except KeyboardInterrupt:
        print("\nShutting down...")
    finally:
        s.close()

if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    run_backend(port)
