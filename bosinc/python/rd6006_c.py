# requirement: rd6006, minimalmodbus
from rd6006 import RD6006
class RD6006PowerSupply: 
    def __init__(self, address):
        self._device = RD6006(address)
        self._device.enable = 0

    def set_current(self, amps: float):
        self.disable()
        self._device.current = amps
        self.enable()

    def get_current(self) -> float:
        return self._device.current

    def set_voltage(self, volts: float):
        self.disable()
        self._device.voltage = volts
        self.enable()

    def get_voltage(self) -> float:
        return self._device.voltage
    
    def enable(self):
        self._device.enable = 1
    
    def disable(self): 
        self._device.enable = 0



DEBUG = 1

def rd6006_create(address: str) -> RD6006PowerSupply:
    if DEBUG:
        print("rd6006_create: address={}".format(address))
        return (address, 4.3, 4)
    device = RD6006PowerSupply(address=address)
    device.enable = 0
    return device

def rd6006_enable(device: RD6006PowerSupply):
    if DEBUG:
        print("rd6006_enable: device={}".format(device))
        return True
    device.enable()
    return True

def rd6006_disable(device: RD6006PowerSupply): 
    if DEBUG:
        print("rd6006_disable: device={}".format(device))
        return True
    device.disable()
    return True

def rd6006_set_current(device: RD6006PowerSupply, amps: float): 
    if DEBUG:
        print("rd6006_set_current: device={}, amps={}".format(device, amps))
        return True
    device.set_current(amps)
    return True

def rd6006_get_current(device: RD6006PowerSupply) -> float: 
    if DEBUG:
        print("rd6006_get_current: device={}".format(device))
        return 4.3
    return device.get_current()




