
class Battery: 
    def __init__(self): 
        raise NotImplementedError
        pass
    def refresh(self): 
        """
        Refresh the state of the battery 
        """
        pass
    def get_voltage(self): 
        """
        Return the voltage of the last refresh action
        """
        pass
    def get_current(self): 
        """
        Returns the current of the last refresh action
        """
        pass
    def get_maximum_capacity(self): 
        """
        Returns the maximum capacity of the last refresh action 
        """
        pass
    def get_current_capacity(self): 
        """
        Returns the capacity remaining of the last refresh action
        """
        pass
    def set_current(self, target_current):
        """
        Sets the current of the battery could pass through 
        could be positive or negative, 
        if positive, the battery is discharging, 
        if negative, the battery is charging 
        """ 
        pass
    def get_max_current_range(self): 
        """
        Returns (max_discharging_current, max_charging_current)
        e.g. (20, -20)
        """
        pass

