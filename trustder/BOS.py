import signal
import BatteryInterface
import JBDBMS
import VirtualBattery
# def sigalrm_handler(signum, frame): 
#     # ??? synchronized code ???
#     pass

class BOS: 
    def __init__(self, voltage, bms_list=[], sample_period):
        self.bms_list = bms_list 
        self.virtual_battery_list = []
        self._voltage = voltage
        self.sample_period = sample_period

    def new_virtual_battery(self, reserve_capacity, max_discharging_current, max_charging_current): 
        # check it first! 
        # if ...
        # return None
        v_battery = VirtualBattery.VirtualBattery(\
            self, reserve_capacity, max_discharging_current, max_charging_current, self.sample_period)
        self.virtual_battery_list.append(v_battery)
        return v_battery

    def refresh_all(self): 
        # also refresh the bus voltage! 
        # should get the real time voltage from the bus 
        # but we don't have it yet 
        self._voltage = 3
        for bms in self.bms_list: 
            bms.refresh()
        return
    
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

    # def currrent_change(self): 
    #     self.refresh_all()
    #     virtual_battery_currents = 0
    #     for vb in self.virtual_battery_list:
    #         virtual_battery_currents += vb.current
        

    def main_loop(self): 
        while 1: 
            # handle inputs 
            # 
            # refresh 
            self.refresh_all()
            # handle output requests 
            # 
            pass



