import enum
import pygatt
import socket
import sys

class Interface(enum.Enum):
    BLE = "BLE"
    UART = "UART"
    TCP = "TCP"
    # TODO: add more
    
class Connection:
    def __init__(iface: Interface, addr: str):
        self._iface = iface
        self._addr = addr

    def get_iface(self) -> Interface: return self._iface
    def get_addr(self) -> str: return self._addr

    def is_connected(self) -> bool: 
        raise NotImplementedError

    def connect(self):
        raise NotImplementedError

    def read(self, nbytes: int) -> bytes:
        raise NotImplementedError

    def write(self, data: bytes):
        raise NotImplementedError

    def close(self):
        raise NotImplementedError

    
class BLEConnection(Connection):
    class DataHandler: 
        def __init__(self): 
            self.global_data = [None for _ in range(10)]
            self.data_length = 0
        def __call__(self, handle, value): 
            self.global_data[self.data_length] = value
            self.data_length += 1
    
    def __init__(self, addr: str, write_uuid: str, read_uuid: str):
        super(BLEConnection, self).__init__(Interface.BLE, addr)
        self._adapter = pygatt.GATTToolBackend(search_window_size=2048)
        self._device = None
        self._connected = False
        self._write_uuid = write_uuid
        self._read_uuid = read_uuid
        self._data_handler = DataHandler()

    def is_connected(self) -> bool: 
        return self._connected

    def connect(self):
        self._adapter.start()
        self._device = self._adapter.connect(self.get_addr())
        time.sleep(1) # not sure why this is necessary...
        # this is to wake the BMS up from sleep 
        # same for JBDBMS.query_info
        # could be changed to shorter time?
        self._connected = True
    
    def read(self): 
        print("Please write to the device to retrieve information", file=sys.stderr)
        return b''
    
    def write(self) -> bytes: 
        pass


class TCPConnection(Connection):
    def __init__(addr: str, port: int):
        super(TCPConnection, self).__init__(Interface.TCP, addr)
        self._port = port
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._connected = False
    
    def is_connected(self) -> bool: 
        return self._connected

    def connect(self): 
        self._socket.connect((addr, port))
        self._connected = True

    def get_port(self) -> int: 
        return self._port
    
    def get_socket(self) -> socket.socket: 
        return self._socket
    
    def read(self, nbytes: int) -> bytes: 
        if not self._connected: 
            print("TCPConnection.read: socket not connected, no byte is read", file=sys.stderr)
            return b''
        # note: 
        # socket.recv does not guarantee the proper number of bytes is received
        # and it returns how many bytes are read 
        chunks = []
        bytes_read = 0
        while bytes_read < nbytes: 
            chunk = self._socket.recv(nbytes - bytes_read)
            if chunk == b'': 
                print("TCPConnection.read: connection broken", file=sys.stderr)
                self._socket.close()
                self._connected = False
                break
            chunks.append(chunk)
            bytes_read += len(chunk)
        return b''.join(chunks)
    
    def write(self, data: bytes) -> int: 
        if not self._connected: 
            print("TCPConnection.write: socket not connected, no byte is written", file=sys.stderr)
            return 0
        msg_length = len(data)
        bytes_sent = 0
        while bytes_sent < msg_length: 
            num_bytes_sent = self._socket.send(data[bytes_sent:])
            if num_bytes_sent == 0:
                print("TCPConnection.write: connection broken", file=sys.stderr)
                self._socket.close()
                self._connected = False
                break
            bytes_sent += num_bytes_sent
        return bytes_sent
    
    def close(self): 
        self._socket.close()
        self._connected = False




