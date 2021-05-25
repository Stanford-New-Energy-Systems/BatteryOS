# Server.py -- BOS server

import socket
import sys
import enum
import os
import select
import BOS
import json

MAXREQLEN = 1024
MAXRESPLEN = 1024

    

def log(*args):
    print('[BOS Server] ', end='')
    print(*args)

class InvalidRequest(Exception):
    def __init__(self, *args):
        if len(args) >= 1:
            arg0 = 'Invalid request: ' + args[0]
            args = (arg0,) + args[1:]
        super().__init__(*args)
        
class NoField(InvalidRequest):
    def __init__(self, field):
        super().__init__(f'missing field "{field}"')
    
        
class BOSServer:
    DEFAULT_PORT = 4004
    
    def __init__(self, bos: BOS.BOS, port=DEFAULT_PORT, backlog=10):
        self._bos = bos
        self._port = port
        self._fd2sock = dict()
        self._listen_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._listen_sock.bind(('', self._port))
        self._listen_sock.listen(backlog)
        self._poll = select.poll()
        self._add_socket(self._listen_sock)

    def _add_socket(self, sock):
        fd = sock.fileno()
        self._fd2sock[fd] = sock
        self._poll.register(fd, select.POLLIN)

    def _remove_socket(self, sock):
        fd = sock.fileno()
        del self._fd2sock[fd]
        self._poll.unregister(fd)

    def loop(self):
        while True:
            self._loop_body()
        
    def _loop_body(self):
        ready = self._poll.poll()
        for (fd, events) in ready:
            sock = self._fd2sock[fd]
            if sock is self._listen_sock:
                # handle new connection
                conn, addr = sock.accept()
                self._add_socket(conn)
            elif events & ~select.POLLIN:
                if events & (select.POLLHUP | select.POLLRDHUP):
                    log(f'Socket {sock} closed')
                elif events & select.POLLERR:
                    log(f'Error on port {sock}')
                elif events & select.POLLNVAL:
                    log(f'Socket {sock} not open')
                self._poll.unregister(sock)
            else:
                try:
                    self._serve(sock)
                except InvalidRequest as e:
                    log(e)
    
    def _serve(self, sock):
        # existing connection
        req_bytes = sock.recv(MAXREQLEN)
        resp_bytes = None
        try:
            resp_bytes = self._req_handle(req_bytes)
        except InvalidRequest as e:
            log(e)
            resp = {
                'response': None,
                'error': str(e),
            }
            respstr = json.dumps(resp)
            resp_bytes = bytes(respstr, 'ASCII')
            
        sock.send(resp_bytes)

    def _req_handle(self, req_bytes) -> bytes:
        reqstr = req_bytes.decode('ASCII').strip()
        req = None
        try:
            req = json.loads(reqstr)
        except:
            raise InvalidRequest(f'invalid JSON {reqstr}')
        handlers = {
            'get_status': self._req_GET_STATUS,
            'set_current': self._req_SET_CURRENT,
        }
        kind = None
        try:    kind = req['request']
        except: raise NoField("request")
        handler = None
        try:    handler = handlers[kind]
        except: raise InvalidRequest(f'bad type {kind}')
        resp = handler(req)
        respstr = json.dumps(resp)
        resp_bytes = bytes(respstr, 'ASCII')
        return resp_bytes

    def _req_lookup(self, d, field):
        try:
            return d[field]
        except:
            raise NoField(field)
        
    def _req_GET_STATUS(self, req):
        name = self._req_lookup(req, 'name')
        status = self._bos.get_status(name)
        resp = {
            'response': status.to_json(),
        }
        return resp

    def _req_SET_CURRENT(self, req):
        name = self._req_lookup(req, 'name')
        current = self._req_lookup(req, 'current')
        self._bos.set_current(name, current)
        resp = {'response': None}
        return resp
        

import threading
import time
import Interpreter
if __name__ == '__main__':
    interp = Interpreter.Interpreter()
    
    server = BOSServer(interp._bos)
    server_thread = threading.Thread(target=server.loop, args=())
    server_thread.start()

    interp.run()
