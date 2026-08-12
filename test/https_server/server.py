#!/usr/bin/env python3
"""
Local HTTPS echo server for HttpsClient testing.

Endpoints:
    GET  /get  → echo method, path, headers as JSON
    POST /post → echo method, path, headers, body as JSON

Starts on 127.0.0.1:0 (OS picks port), prints "READY:<port>" to stdout.
The test harness reads that line, then sends requests to localhost:<port>.
When done, the harness kills the process by closing its stdin pipe (PCLOSE).
No shutdown endpoint needed.
"""

import json
import os
import ssl
import sys
import traceback
from http.server import HTTPServer, BaseHTTPRequestHandler

# All diagnostics go to stdout (NOT stderr) so the C++ _popen caller can read them.
# The contract: print exactly "READY:<port>" on success, "FATAL:<reason>" on failure.
# Everything else on stdout is treated as diagnostic noise and ignored by the harness.

CERT_DIR = os.path.dirname(os.path.abspath(__file__))
CERT = os.path.join(CERT_DIR, "cert.pem")
KEY  = os.path.join(CERT_DIR, "key.pem")


def to_json_bytes(obj):
    payload = json.dumps(obj, ensure_ascii=False, indent=2)
    return payload.encode("utf-8")


class EchoHandler(BaseHTTPRequestHandler):
    """Echo back request details as JSON."""

    def _headers_dict(self):
        return {k: v for k, v in self.headers.items()}

    def _send_json(self, status, obj):
        body = to_json_bytes(obj)
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        # Echo back any X-* headers the client sent
        for k, v in self.headers.items():
            if k.lower().startswith("x-"):
                self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        self._send_json(200, {
            "method": "GET",
            "path": self.path,
            "headers": self._headers_dict(),
        })

    def do_POST(self):
        content_len = int(self.headers.get("Content-Length", 0))
        raw_body = self.rfile.read(content_len)
        try:
            body_text = raw_body.decode("utf-8")
        except UnicodeDecodeError:
            body_text = raw_body.hex()

        self._send_json(200, {
            "method": "POST",
            "path": self.path,
            "headers": self._headers_dict(),
            "body": body_text,
        })

    # Suppress per-request log lines so stdout stays clean for the "READY" line
    def log_message(self, fmt, *args):
        pass


def main():
    try:
        if not os.path.exists(CERT) or not os.path.exists(KEY):
            print(f"FATAL: cert/key missing in {CERT_DIR}")
            print("Generate with: "
                  "openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem "
                  "-days 36500 -nodes -subj '/CN=localhost'")
            sys.exit(1)

        port = int(sys.argv[1]) if len(sys.argv) > 1 else 0

        server = HTTPServer(("127.0.0.1", port), EchoHandler)

        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(CERT, KEY)
        server.socket = ctx.wrap_socket(server.socket, server_side=True)

        actual_port = server.socket.getsockname()[1]
        print(f"READY:{actual_port}", flush=True)

        server.serve_forever()
    except Exception as e:
        print(f"FATAL: {e}")
        traceback.print_exc(file=sys.stdout)
        sys.exit(1)


if __name__ == "__main__":
    main()
