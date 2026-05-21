#!/usr/bin/env python3
import http.server
import socket
import socketserver
import threading

TELEMETRY_PORT = 9000
REST_PORT = 8080
CURRENT_CMD = ""

class RestHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        global CURRENT_CMD
        body = CURRENT_CMD.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        return


def telemetry_server():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", TELEMETRY_PORT))
    sock.listen(5)
    print(f"[mock-gateway] TCP telemetry listening on {TELEMETRY_PORT}")

    while True:
        conn, addr = sock.accept()
        print(f"[mock-gateway] client connected: {addr}")
        with conn:
            while True:
                data = conn.recv(1024)
                if not data:
                    break
                print(f"[telemetry] {data.decode(errors='replace').strip()}")


def rest_server():
    with socketserver.TCPServer(("0.0.0.0", REST_PORT), RestHandler) as httpd:
        print(f"[mock-gateway] REST command endpoint on {REST_PORT}")
        httpd.serve_forever()


def stdin_loop():
    global CURRENT_CMD
    print("Enter command body to serve (e.g. sample_period_ms=200, reconnect, flush)")
    print("Empty line clears command")
    while True:
        CURRENT_CMD = input("cmd> ").strip()


if __name__ == "__main__":
    threading.Thread(target=telemetry_server, daemon=True).start()
    threading.Thread(target=rest_server, daemon=True).start()
    stdin_loop()
