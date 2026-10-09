#!/usr/bin/env python3
"""
DSD Remote — Development HTTP Server
Serves WebClient files with aggressive no-cache headers so Chrome on the
Samsung A9 always fetches the latest assets without manual cache clearing.

Usage:
    python serve.py [port]      # default port: 8080

Do NOT use `python -m http.server` for development — it does not send
Cache-Control headers, causing stale CSS/JS on Android Chrome.
"""

import http.server
import socketserver
import sys
import os

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080

# Serve files from the directory that contains this script
SERVE_DIR = os.path.dirname(os.path.abspath(__file__))


class NoCacheHandler(http.server.SimpleHTTPRequestHandler):
    """SimpleHTTPRequestHandler with no-cache headers on every response."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=SERVE_DIR, **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        super().end_headers()

    def log_message(self, format, *args):  # noqa: A002
        # Minimal log: method + path + status
        print(f"  {self.command} {self.path}  →  {args[1]}")


class ReusableTCPServer(socketserver.TCPServer):
    allow_reuse_address = True


if __name__ == "__main__":
    os.chdir(SERVE_DIR)
    with ReusableTCPServer(("", PORT), NoCacheHandler) as httpd:
        print(f"DSD Remote dev server — http://0.0.0.0:{PORT}")
        print(f"Serving: {SERVE_DIR}")
        print("Cache-Control: no-store on all responses")
        print("Press Ctrl+C to stop.\n")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nServer stopped.")
