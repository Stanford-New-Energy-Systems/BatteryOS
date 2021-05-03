# Policy.py
# Policies for splitter battery management

from BatteryInterface import *
import BOSErr
from BOSNode import *

class Policy(BOSNode):
    def __init__(self, lookup):
        super().__init__()
        self._lookup = lookup

class SplitterPolicy(Policy):
    def __init__(self, srcname, lookup):
        super().__init__(lookup)
        self._srcname = srcname

    def _source(self): return self._lookup(self._srcname)

    def refresh(self):
        src = self._lookup(self._srcname)
        src.refresh()

    def get_status(self, dstname: str):
        '''
        Get status of splitter battery with name `dstname`.
        '''
        raise NotImplementedError

    def add_child(*args):
        raise NotImplementedError

    def remove_child(*args):
        raise NotImplementedError
    

class ProportionalPolicy(SplitterPolicy):
    def __init__(self, srcname, lookup):
        super().__init__(srcname, lookup)
        self._scales = dict()   # map from splitter battery name to scale
        self._currents = dict() # map from splitter battery name to current
        self._socs = dict()     # map from splitter battery name to state of charge

    def get_status(self, dstname: str) -> BatteryStatus:
        src = self._source()
        status = src.get_status()
        scale = self._scales[dstname]
        return BatteryStatus(status.voltage,
                             self._currents[dstname],
                             self._socs[dstname],
                             status.max_capacity * scale.get_max_capacity(),
                             status.max_discharging_current * scale.get_max_discharge_rate(),
                             status.max_charging_current * scale.get_max_charge_rate(),
                             )

    def set_current(self, dstname: str):
        scale = self._scales[dstname]
        src = self._source()

        # verify target_current is within bounds
        status = src.get_status()
        if target_current > status.max_discharging_current * scale.get_max_discharge_rate():
            raise BOSErr.NoResources
        if -target_current > status.max_charging_current * scale.get_max_charge_rate():
            raise BOSErr.NoResources

        # set new target current for battery
        self._currents[dstname] = target_current

        # set new net current for source battery
        new_net_current = sum(self._currents.values())
        src.set_current(new_net_current)

    def add_child(self, name: str, newscale: Scale):
        if name in self._scales:
            raise BOSErr.InvalidArgument
        
        # make sure all scale components will still <= 1.0
        total_soc = 0
        total_max_cap = 0
        total_discharge = 0
        total_charge = 0
        for scale in list(self._scales.values()) + [newscale]:
            total_soc += scale.get_state_of_charge()
            total_max_cap += scale.get_max_capacity()
            total_discharge += scale.get_max_discharge_rate()
            total_charge += scale.get_max_charge_rate()

        for total in [total_soc, total_max_cap, total_discharge, total_charge]:
            if total > 1:
                raise BOSErr.NoResources

        self._scales[name] = newscale
        self._currents[name] = 0
        self._socs[name] = 0 # TODO: allow specifying state of charge, too

    def remove_child(self, name: str):
        if name not in self._scales:
            raise BOSErr.InvalidArgument
        current = self._currents[name]
        if current != 0:
            self.set_current(name, 0)
        del self._currents[name]
        del self._socs[name]
        del self._scales[name]

        
class TranchePolicy(SplitterPolicy):
    def __init__(self, srcname, lookup):
        super().__init__(srcname, lookup)
        self._tranches = list()

    def get_status(self, dstname: str) -> BatteryStatus:
        assert len(self._tranches) > 0
        src = self._source()
        status = src.get_status()

        # fill each splitter battery in sequence
        for (curname, tgtstatus) in self._tranches:
            curstatus = BatteryStatus(status.voltage,
                                      min(status.current, tgtstatus.current),
                                      min(status.state_of_charge, tgtstatus.state_of_charge),
                                      min(status.max_capacity, tgtstatus.max_capacity),
                                      min(status.max_discharging_current,
                                          tgtstatus.max_discharging_current),
                                      min(status.max_charging_current,
                                          tgtstatus.max_charging_current),
                                      )
            status.current -= curstatus.current
            status.state_of_charge -= curstatus.state_of_charge
            status.max_capacity -= curstatus.max_capacity
            status.max_discharging_current -= curstatus.max_discharging_current
            status.max_charging_current -= curstatus.max_charging_current

            if curname == dstname:
                return curstatus

        raise BOSErr.InvalidArgument
    
    def set_current(self, dstname: str, target_current):
        curstatus = self.get_status(dstname)
        if not (-curstatus.max_charging_current <= target_current and
                target_current <= curstatus.max_discharging_current):
            raise BOSErr.InvalidArgument

        curstatus.current = target_current

        current_sum = sum(map(lambda (_, status): status.current, self._tranches))
        src = self._source()
        src.set_current(current_sum)
        

    def add_child(self, name: str, status: BatteryStatus, pos: int):
        '''
        If `pos == 0`, then insert at the front.
        If `pos == -1`, then insert at the back.
        Otherwise, insert at the index given in `pos`.
        '''
        if pos == -1:
            self._tranches.append((name, status))
        elif pos >= 0:
            self._tranches.insert(pos, (name, status))
        else:
            raise BOSErr.InvalidArgument


    def remove_child(self, name: str):
        self.set_current(name, 0)

        for i, (curname, _) in enumerate(self._tranches):
            if curname == name:
                del self._tranches[i]
                return

        raise BOSErr.InvalidArgument
