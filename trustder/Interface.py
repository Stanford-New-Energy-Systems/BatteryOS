import enum

class Interface(enum.Enum):
    BLE = auto
    UART = auto
    TCP = auto
    # TODO: add more
    
class Connection:
    def __init__(iface: Interface, addr: str):
        self._iface = iface
        self._addr = addr

    def get_iface(self) -> Interface: return self._iface
    def get_addr(self) -> str: return self._addr

    def read(self, nbytes: int) -> bytes:
        raise NotImplementedError

    def write(self, data: bytes):
        raise NotImplementedError

    def close(self):
        raise NotImplementedError

    
class BLE:
    def __init__(self):
        self._adapter = pygatt.GATTToolBackend(search_window_size=2048)
        self._adapter.start()


    def connect(self, addr: str):
        device = self._adapter.connect(addr)
        time.sleep(1) # not sure why this is necessary...
        return device
