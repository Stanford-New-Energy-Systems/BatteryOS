import signal
import JBDBMS
import BatteryInterface

class VirtualBattery(BatteryInterface.Battery):
    """
    The interface used for communication between BOS & physical batteries and BOS & virtual batteries
    """
    def __init__(self, bos, reserve_capacity, max_discharging_current, max_charging_current, sample_period): 
        self.bos = bos
        self.chargeable = False
        self.dischargeable = True
        
        # voltage 
        self._voltage = bos.get_voltage()

        self._current_range = range(-max_charging_current, max_discharging_current)
        self.current_range = (max_discharging_current, max_charging_current)

        self.actual_current = 0
        self.claimed_current = 0
        
        self.reserved_capacity = reserve_capacity
        self.remaining_capacity = reserve_capacity

        self.sample_period = sample_period

        # how does the actual drawing vs the claimed drawing compare
        # if you claimed to draw 1A, but you drawn 2A, your credit will be -1Ah after an hour! 
        self.credit = 0
    
    def refresh(self): 
        # self.bos.refresh_all()
        # refresh the virtual batteries 
        # and update the capacity according to the measured current values 
        # what TODO  
        # the underlying BOS will handle the refresh of the physical batteries 

        # perhaps refresh the associated current meter? 
        # this should be from a current meter, we just assume that we have it 
        # but we don't have it right now, we just assume we have it 
        self.actual_current = 0
        # also estimate the remaining capacity 
        self._voltage = self.bos.get_voltage()
        self.remaining_capacity -= (self.claimed_current * self._voltage) * self.sample_period
        self.credit = (self.claimed_current - self.actual_current) * self._voltage * self.sample_period
        # perhaps use a more accurate approach... 

        if self.get_current_capacity <= 0: 
            self._set_dischargeable(False)
        else: 
            self._set_dischargeable(True)
        if self.get_current_capacity >= self.get_maximum_capacity(): 
            self._set_chargeable(False)
        else: 
            self._set_chargeable(True)
        pass
    
    def get_voltage(self): 
        """
        the voltage is fixed 
        """
        return self._voltage
    
    def get_current(self): 
        """
        The actual current the you are drawing 
        """
        return self.actual_current
    
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

    def set_current(self, target_current): 
        """
        Now submit a current contract 
        it's a contract that the user is drawing target_current in the future  
        if charging, please set it to negative current 
        otherwise it's positive 

        return value: fail / succeed 
        """
        if (target_current not in self._current_range): 
            return False
        if (target_current > 0 and self.get_current_capacity() <= 0): 
            # self._set_dischargeable(False)
            return False
        if (target_current < 0 and self.get_current_capacity() >= self.get_maximum_capacity()): 
            # self._set_chargeable(False)
            return False 
        
        self.claimed_current = target_current
        # ? do we need to do that, or the BOS will figure out itself? 
        # self.bos.currrent_change()
        return True

    def get_current_range(self): 
        """
        Returns (max_discharging_current, max_charging_current)
        e.g. (20, 20)
        """
        return self.current_range

    def get_credit(self): 
        """
        how much do I owe? 
        """
        return self.credit

    def _set_chargeable(self, is_chargeable): 
        """
        should be able to shutdown the virtual battery 
        """
        self.chargeable = is_chargeable
        # is there a way to regulate this? 
    
    def _set_dischargeable(self, is_dischargeable): 
        """
        should be able to shutdown the virtual battery 
        """
        self.dischargeable = is_dischargeable
        # is there a way to regulate this? 



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



