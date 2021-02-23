import signal
import JBDBMS
import BatteryInterface

class VirtualBattery(BatteryInterface.Battery):
    def __init__(self, bos, reserve_capacity): 
        self.bos = bos
        self.chargeable = False
        self.dischargeable = False
        # ???
        self._voltage = bos.get_voltage()
        self.actual_current = 0
        self.claimed_current = 0
        self.reserved_capacity = reserve_capacity
        self.remaining_capacity = reserve_capacity

        # how does the actual drawing vs the claimed drawing compare
        # if you claimed to draw 1A, but you drawn 2A, your credit will be -1Ah after an hour! 
        self.credit = 0
    
    def refresh(self, measured_current, sample_period): 
        # self.bos.refresh_all()
        # refresh the virtual batteries 
        # and update the capacity according to the measured current values 
        # TODO 
        pass
    
    def get_voltage(self): 
        """
        the voltage is fixed 
        """
        return self.bos.get_voltage()
    
    def get_current(self): 
        """
        The actual current the you are drawing 
        """
        return self.actual_current

    def set_current(self, target_current): 
        """
        Now submit another current request 
        it's a contract that the user is drawing target_current in the future  
        """
        self.claimed_current = target_current
        self.bos.currrent_change()
    
    def get_maximum_capacity(self): 
        """
        Just the reserved capacity 
        """
        return self.reserved_capacity
    
    def get_current_capacity(self): 
        """
        if you keep running at your claimed current, what's your remaining charge... 
        """
        return self.remaining_capacity
        # return self.bos.get_current_capacity()
    
    def set_chargeable(self, is_chargeable): 
        """
        should be able to shutdown the virtual battery 
        """
        self.chargeable = True
    
    def set_dischargeable(self, is_dischargeable): 
        """
        should be able to shutdown the virtual battery 
        """
        self.dischargeable = True

    


# def sigalrm_handler(signum, frame): 
#     # ??? synchronized code ???
#     pass

class BOS: 
    def __init__(self, voltage, bms_list=[], sample_period):
        self.bms_list = bms_list 
        self.virtual_battery_list = []
        self._voltage = voltage

        self.sample_period = sample_period

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
        

    def main_loop(self): 
        while 1: 
            # handle inputs 
            # 
            # refresh 
            self.refresh_all()
            # handle output requests 
            # 
            pass



