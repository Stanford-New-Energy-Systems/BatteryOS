from BatteryInterface import Battery
class SplitterBattery(Battery):
    '''
    A virtual battery that is a partition of a shared source battery.
    Splitter batteries are managed by splitter policies, which determine what proportion of the 
    source resources this battery partition should receive.
    As a result, most functions are delegated to the associated splitter policy.

    You can create new splitter batteries through splitter policies; DO NOT directly instantiate
    a new splitter battery.
    '''
    
    def __init__(self, name, sample_period, policyname, lookup):
        ''' 
        DO NOT call this function directly; use a splitter policy to create a new splitter 
        battery instead
        '''
        super().__init__(name)
        self._sample_period = sample_period
        self._policyname = policyname
        self._lookup = lookup

    @staticmethod
    def type(): return "Splitter"

    def _policy(self): return self._lookup(self._policyname)
                     
    def refresh(self):
        self._policy().refresh()
        current = self.get_current()
        self.update_meter(current, current)
        
    def get_status(self):
        status = self._policy().get_status(self._name)
        self.update_meter(status.current, status.current)
        return status

    def set_current(self, target_current):
        old_current = self.get_status().current
        self._policy().set_current(self._name, target_current)
        new_current = self.get_status().current
        self.update_meter(old_current, new_current)

    def reset_meter(self):
        self._policy().reset_meter(self._name)
    