################
# Policies for battery partitioning
#
# This file defines an abstract policy class and provides concrete implementations of
# policies we discussed in Spring 2021.
#
# 


# Policy.py
# Policies for splitter battery management

from BatteryInterface import *
import BOSErr
from BOSNode import *
from SplitterBattery import SplitterBattery

class Policy(BOSNode):
    def __init__(self, lookup):
        super().__init__()
        self._lookup = lookup

class SplitterPolicy(Policy):
    '''
    An abstract policy for splitting batteries. 

    Splitter policies decide what status to report for each partition, how to add new partitions,
    and how to remove existing partitions.
    '''

    def __init__(self, srcname, lookup):
        super().__init__(lookup)
        self._srcname = srcname

    def _source(self): return self._lookup(self._srcname)

    def get_splitter_status(self):
        return {"source": self._srcname}

    def refresh(self):
        # src = self._lookup(self._srcname)
        # src.refresh()
        pass

    def get_status(self, dstname: str):
        '''
        Get status of splitter battery with name `dstname`.
        '''
        raise NotImplementedError

    def add_child(self, name, target_status: BatteryStatus, *args):
        raise NotImplementedError

    def remove_child(self, name, *args):
        raise NotImplementedError

    def reset_meter(self, name: str):
        raise NotImplementedError
    

class ProportionalPolicy(SplitterPolicy):
    '''
    A proportional battery partitioning policy.
    Each partition receives a fixed proportion of the underlying battery's resources.
    This means that if the underlying battery's state of charge (SOC), e.g., fluctuates
    unexpectedly, the surplus or deficit SOC will be split between the different battery partitions
    based on the propotion of each partition's expected SOC to the shared battery's total expected
    SOC.
    '''
    
    def __init__(self, srcname, lookup, initname):
        super().__init__(srcname, lookup)
        self._scales = dict()   # map from splitter battery name to scale
        self._currents = dict() # map from splitter battery name to current

        # add initial battery
        self._scales[initname] = Scale(1, 1, 1, 1)
        self._currents[initname] = 0

        
    def _parts(self):
        return self._scales.keys()

    
    def get_splitter_status(self):
        parts = []
        for name in self._scales:
            part = {
                "name": name,
                "scale": self._scales[name],
                "current": self._currents[name],
            }
            parts.append(part)
        return super().get_splitter_status() | {
            "parts": parts,
        }
    

    def get_status(self, dstname) -> BatteryStatus:
        src = self._source()
        status = src.get_status()
        scale = self._scales[dstname]
        # get this partition's expected SOC
        expected_soc = self._lookup(dstname).get_meter()
        # get the underlying battery's total expected SOC
        total_expected_soc = sum([self._lookup(name).get_meter() for name in self._parts()])
        total_actual_soc = self._source().get_status().state_of_charge
        actual_soc = expected_soc / total_expected_soc * total_actual_soc
        return BatteryStatus(status.voltage,
                             self._currents[dstname],
                             actual_soc,
                             status.max_capacity * scale.get_max_capacity(),
                             status.max_discharging_current * scale.get_max_discharge_rate(),
                             status.max_charging_current * scale.get_max_charge_rate(),
                             )
    

    def set_current(self, dstname: str, target_current):
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


    def add_child(self, name: str, target_status: BatteryStatus, from_name: str) -> Battery:
        '''
        Add a new managed battery to this splitter policy.
        name: name of battery to add
        target_status: the desired parameters of this battery
        from_name: name of battery to split this battery off of (must be free and managed by
                   this policy)
        '''
        if name in self._parts():
            raise BOSErr.InvalidArgument
        if from_name not in self._parts():
            raise BOSErr.InvalidArgument

        from_battery = self._lookup(from_name)
        from_status = from_battery.get_status()
        if from_status.current != 0:
            raise BOSErr.BatteryInUse

        target_status.voltage = from_status.voltage

        actual_status = BatteryStatus(target_status.voltage,
                                      0,
                                      min(target_status.state_of_charge,
                                          from_status.state_of_charge),
                                      min(target_status.max_capacity,
                                          from_status.max_capacity),
                                      min(target_status.max_discharging_current,
                                          from_status.max_discharging_current),
                                      min(target_status.max_charging_current,
                                          from_status.max_charging_current),
                                      )

        # compute scale
        total_status = self._source().get_status()
        scale = Scale(actual_status.state_of_charge / total_status.state_of_charge,
                      actual_status.max_capacity / total_status.max_capacity,
                      actual_status.max_discharging_current / total_status.max_discharging_current,
                      actual_status.max_charging_current / total_status.max_charging_current,
                      )
        self._scales[name] = scale
        self._currents[name] = 0
        
        
        # update free battery that was split from
        self._scales[from_name] -= scale
        self._lookup(from_name).reset_meter()
        
        
        return actual_status
    

    def remove_child(self, name: str, to_name: str):
        '''
        Remove managed battery from splitter policy.
        name: name of battery to remove
        to_name: name of battery to merge state of charge, capacity, etc. into
        '''
        if name not in self._parts():
            raise BOSErr.InvalidArgument
        if self._currents[name] != 0:
            raise BOSErr.BatteryInUse
        
        rm_scale = self._scales[name]
        rm_soc = self._lookup(name).get_meter()
        to_battery = self._lookup(to_name)
        to_soc = to_battery.get_meter()
        to_soc += rm_soc
        to_battery.set_meter(to_soc)
        

    def reset_meter(self, name: str):
        '''
        Reset the meter of the named managed battery.
        This will set the meter readout to be the proportional amount of the actual source charge
        read from source.get_status().
        '''
        total_soc = self._source().get_status().state_of_charge
        soc = total_soc * self._scales[name].get_state_of_charge()
        self._lookup(name).set_meter(soc)

        
class TranchePolicy(SplitterPolicy):
    '''
    A policy managing tranched battery partitions.
    For a description of how tranched policies work, see the BAL Design Document.
    NOTE: This code doesn't work, but the ideas behind it are correct, I think. 
    NOTE: This should be merged with the Reservation Policy into a common implementation that 
          parameterizes the direction in which surpluses and deficits are distributed.
    '''
    
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

        current_sum = sum(map(lambda t: t[1].current, self._tranches))
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


class Scale:
    def __init__(self, state_of_charge, max_capacity, max_discharge_rate,
                 max_charge_rate):
        for scale in [state_of_charge, max_capacity, max_discharge_rate, max_charge_rate]:
            if not (scale >= 0.0 and scale <= 1.0):
                raise BOSErr.InvalidArgument
            
        self._state_of_charge = state_of_charge
        self._max_capacity = max_capacity
        self._max_discharge_rate = max_discharge_rate
        self._max_charge_rate = max_charge_rate

    def __str__(self) -> str:
        return '{{soc = {}, max_capacity = {}, max_discharge = {}, max_charge = {}}}'.format(
            self._state_of_charge,
            self._max_capacity,
            self._max_discharge_rate,
            self._max_charge_rate,
            )

    @staticmethod
    def scale_all(scale): return SplitterBattery.Scale(scale, scale, scale, scale)
    
    def get_state_of_charge(self): return self._state_of_charge
    def get_max_capacity(self): return self._max_capacity
    def get_max_discharge_rate(self): return self._max_discharge_rate
    def get_max_charge_rate(self): return self._max_charge_rate
    
    def serialize(self):
        return {"state_of_charge": self._state_of_charge,
                "max_capacity": self._max_capacity,
                "max_discharge_rate": self._max_discharge_rate,
                "max_charge_rate": self._max_charge_rate}
    

    def __sub__(self, other):
        if self._state_of_charge >= other._state_of_charge and \
           self._max_capacity >= other._max_capacity and \
           self._max_discharge_rate >= other._max_discharge_rate and \
           self._max_charge_rate >= other._max_charge_rate:
            return Scale(self._state_of_charge - other._state_of_charge,
                         self._max_capacity - other._max_capacity,
                         self._max_discharge_rate - other._max_discharge_rate,
                         self._max_charge_rate - other._max_charge_rate,
                         )
        else:
            raise BOSErr.NoResources
        
        
