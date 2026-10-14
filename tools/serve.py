#!/usr/bin/env python3
"""Serve the web flasher at http://localhost:8000 (Web Serial works on localhost without HTTPS)."""
import http.server, os, socketserver, threading, webbrowser

WEB = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "web"))
PORT = 8000


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = dict(http.server.SimpleHTTPRequestHandler.extensions_map,
                          **{".bin": "application/octet-stream", ".json": "application/json"})

    def __init__(self, *a, **k):
        super().__init__(*a, directory=WEB, **k)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


if __name__ == "__main__":
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("127.0.0.1", PORT), Handler) as srv:
        url = "http://localhost:%d/" % PORT
        print("Serving the flasher at", url, "(Ctrl+C to stop). Use Chrome or Edge.")
        threading.Timer(0.6, lambda: webbrowser.open(url)).start()
        try:
            srv.serve_forever()
        except KeyboardInterrupt:
            pass
