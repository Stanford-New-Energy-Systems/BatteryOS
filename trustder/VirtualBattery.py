"""
This file should contain a "client"-side virtual battery class 
"""
import BatteryInterface
class VirtualBatteryUser(BatteryInterface.Battery):
    """
    This should be the user-local implementation of VirtualBattery 
    """
    def __init__(self, bos_handle, voltage, reserve_capacity, max_discharging_current, max_charging_current):
        """
        assuming the BOS has approved the creation of virtual battery 
        """ 
        self.bos_handle = bos_handle
        self.chargeable = False
        self.dischargeable = True
        
        # voltage 
        self._voltage = voltage 

        self._current_range = range(-max_charging_current, max_discharging_current)
        self.current_range = (max_discharging_current, max_charging_current)

        self.actual_current = 0
        self.claimed_current = 0
        
        self.reserved_capacity = reserve_capacity
        self.remaining_capacity = reserve_capacity
    
    def refresh(self): 
        """
        Just get everything from the BOS 
        """
        pass

    def read_current_meter(self): 
        """
        TODO 
        """
        # should send a request to the client side and retrieve the information 
        self.actual_current = 0
        return self.actual_current
    
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
        # # BOS side don't need to tell the BOS
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
        # TODO is there a way to regulate this? 
    
    def _set_dischargeable(self, is_dischargeable): 
        """
        should be able to shutdown the virtual battery 
        """
        self.dischargeable = is_dischargeable
        # TODO is there a way to regulate this? 

