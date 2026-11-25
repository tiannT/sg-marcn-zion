#!/usr/bin/env python3
"""127.0.0.1:10022 -> actual running VM1; no workstation tunnel."""
from pathlib import Path
import select
import socket
import socketserver
import sys
from common import load, refresh_known, runtime_status

root = Path(sys.argv[1]).resolve()
config = load(root / 'config.json')


class Handler(socketserver.BaseRequestHandler):
    def handle(self):
        try:
            status = runtime_status(config)
            if status['state'] != 1 or status['image'] not in (1, 2, 3):
                return
            refresh_known(root, status)
            address = {1: '10.0.2.15', 2: '10.0.2.15', 3: '10.0.2.15'}[status['image']]
            with socket.create_connection((address, 22), timeout=5) as guest:
                self.request.settimeout(10)
                guest.settimeout(10)
                peers = [self.request, guest]
                while True:
                    readable, _, _ = select.select(peers, [], [], 30)
                    for source in readable:
                        data = source.recv(65536)
                        if not data:
                            return
                        (guest if source is self.request else self.request).sendall(data)
        except (OSError, RuntimeError, ValueError) as exc:
            print('proxy connection:', str(exc), flush=True)


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


with Server(('127.0.0.1', 10022), Handler) as server:
    print('ZION_COMPAT_PROXY_READY bind=127.0.0.1:10022', flush=True)
    server.serve_forever()
