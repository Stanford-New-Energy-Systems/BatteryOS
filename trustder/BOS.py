import signal
import JBDBMS
import BatteryInterface

class VirtualBattery(BatteryInterface.Battery):
    def __init__(self, bos): 
        self.bos = bos
        self.chargeable = False
        self.dischargeable = False
        # ???
        self.current = 0
    
    def refresh(self): 
        self.bos.refresh_all()
    
    def get_voltage(self): 
        return self.bos.get_voltage()
    
    def get_current(self): 
        # ??? 
        return self.current

    def set_current(self, current): 
        #???
        self.current = current
        self.bos.currrent_change()
    
    def get_maximum_capacity(self): 
        return self.bos.get_maximum_capacity()
    
    def get_current_capacity(self): 
        return self.bos.get_current_capacity()
    
    def set_chargeable(self, is_chargeable): 
        self.chargeable = True
    
    def set_dischargeable(self, is_dischargeable): 
        self.dischargeable = True


def sigalrm_handler(signum, frame): 
    # ??? synchronized code ???
    pass

class BOS: 
    def __init__(self, voltage, bms_list=[]):
        self.bms_list = bms_list 
        self.virtual_battery_list = []
        self._voltage = voltage

    def new_virtual_battery(self): 
        v_battery = VirtualBattery(self)
        self.virtual_battery_list.append(v_battery)
        return v_battery

    def refresh_all(self): 
        for bms in self.bms_list: bms.refresh()
    
    def get_voltage(self): 
        return self._voltage

    def get_maximum_capacity(self): 
        max_capacity = 0
        for bms in self.bms_list: 
            max_capacity += bms.get_maximum_capacity()
        return max_capacity

    def get_current_capacity(self): 
        current_capacity = 0
        for bms in self.bms_list: 
            current_capacity += bms.get_current_capacity()
        return current_capacity

    def currrent_change(self): 
        self.refresh_all()
        virtual_battery_currents = 0
        for vb in self.virtual_battery_list:
            virtual_battery_currents += vb.current
        




