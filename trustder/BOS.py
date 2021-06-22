import threading
import signal
import time
import json
import typing as T
from BatteryInterface import *
from BOSNode import *
from JBDBMS import JBDBMS
import Interface
from Interface import BLEConnection
from Interface import Connection
from util import *
import BOSErr
from Policy import *

class NullBattery(Battery):
    '''
    A virtual leaf-node battery whose status is all 0's (except for voltage).
    Note that the ability to specify the voltage is important so that 
    it can be aggregated with other batteries.
    '''
    
    def __init__(self, name, voltage):
        '''Construct a null battery with the given name and voltage.'''
        super().__init__(name)
        self._voltage = voltage

    def refresh(self):
        # nothing to refresh, since this battery is a virtual leaf
        pass

    def get_status(self):
        return BatteryStatus(self._voltage, 0, 0, 0, 0, 0)

    def set_current(self, current):
        # the current can only ever be 0
        if current != 0:
            raise BOSErr.CurrentOutOfRange
        
    @staticmethod
    def type(): return "Null"
    
    _KEY_VOLTAGE = "voltage"
    
    def serialize(self):
        return self._serialize_base({self._KEY_VOLTAGE: self._voltage})

    @staticmethod
    def _deserialize_derived(d: dict):
        return NullBattery(d[NullBattery._KEY_VOLTAGE])

    
class PseudoBattery(PhysicalBattery):
    '''
    A virtual leaf-node battery that returns a pre-specified status every time it is queried.
    '''
    
    def __init__(self, name, iface, addr, status: BatteryStatus):
        '''
        Construct a new pseudo battery on the given interface and at the given address with
        with the specified status.
        '''
        super().__init__(name, iface, addr)
        self._status = status
        self.set_meter(self._status.state_of_charge)

    def __str__(self): return str(self.serialize())
        
    @staticmethod
    def type(): return "Pseudo"

    def get_status(self):
        with self._lock:
            old_meter = self.get_meter()
            self.update_meter(self._status.current, self._status.current) # update meter
            new_meter = self.get_meter()
            self._status.state_of_charge += new_meter - old_meter
            # self._status.state_of_charge = self.get_meter() # TODO: Why did I add this?
            return self._status

    def set_current(self, target_current):
        with self._lock:
            old_current = self.get_status().current
            if not (target_current >= -self._status.max_charging_current and
                    target_current <= self._status.max_discharging_current):
                raise BOSErr.CurrentOutOfRange
            self._status.current = target_current
            new_current = self.get_status().current
            self.update_meter(old_current, new_current)

    def set_status(self, status):
        '''Set a new status of this battery.'''
        with self._lock:
            self.update_meter(self._status.current, status.current)
            self._status = status

    _KEY_IFACE = "iface"
    _KEY_ADDR = "addr"
    _KEY_STATUS = "status"

    def serialize(self):
        with self._lock:
            return self._serialize_base({self._KEY_STATUS: self._status.serialize()})

    @staticmethod
    def _deserialize_derived(d):
        return PseudoBattery(d[PseudoBattery._KEY_IFACE], d[PseudoBattery._KEY_ADDR],
                             d[PseudoBattery._KEY_STATUS])


class NetworkBattery(Battery):
    '''
    A virtual battery that mirrors another logical battery on a different BOS node.
    It connects to a remote BOS node server and queries it about the remote battery.

    For the implementation of the BOS node server, see 'Server.py'.
    '''
    
    def __init__(self, name, remote_name, iface, addr, *args):
        '''
        Create a network battery with the name 'name' that connects to a remote battery with the
        name 'remote_name' on another BOS node at the network address 'addr' over the interface
        'iface'.
        '''
        super().__init__(name)
        self._remote_name = remote_name
        self._conn = Connection.create(iface, addr, *args)
        self._conn.connect()
        self._status = None    # Cached status
        self._timestamp = None
        self.refresh()

    def _send_recv(self, req):
        req_bytes = bytes(json.dumps(req), 'ASCII')
        self._conn.write(req_bytes)
        resp_str = self._conn.readstr()
        resp = json.loads(resp_str)
        return resp

    def _resp_get_body(self, resp):
        if 'response' not in resp:
            raise BOSErr.BadResponse(resp)
        body = resp['response']
        if body is None and 'error' in resp:
            error = resp['error']
            print(f'{self._name}: server error; {error}')
            raise BOSErr.ServerError(error)
        return body

    def refresh(self):
        # force a refresh of the remote battery status and cache it.
        with self._lock: 
            req = {
                'request': 'get_status',
                'name': self._remote_name,
            }
            resp = self._send_recv(req)
            self._status = BatteryStatus.from_json(self._resp_get_body(resp))
            # self._status = self._resp_get_body(resp)
            self._timestamp = time.time() # TODO: need to set true timestamp.
            current = self._status.current
            self.update_meter(current, current)

    def set_current(self, target_current):
        with self._lock:
            old_current = self._status.current
            req = {
                'request': 'set_current',
                'name': self._remote_name,
                'current': target_current,
            }
            resp = self._send_recv(req)
            ok = self._resp_get_body(resp)
            new_current = self.get_current() # TODO: this is expensive
            self.update_meter(old_current, new_current)

    def get_status(self):
        # return the cached status of the remote battery, re-caching it if necessary.
        with self._lock:
            if time.time() - self._timestamp >= self._sample_period:
                self.refresh()
            self.update_meter(self._status.current, self._status.current)
        return self._status

    
class AggregatorBattery(Battery):
    '''
    A virtual battery that aggregates multiple logical batteries into one logical battery.
    '''
    
    def __init__(self, name, voltage, voltage_tolerance, srcnames: T.List[str], lookup):
        '''
        Construct a new aggregator battery.
        `voltage` is the reported voltage of the aggregated battery. 
        `voltage_tolerance` indicates how much the source voltages are allowed to differ from the 
        virtual voltage.
        `srcnames` is a list of battery names (strings)
        `lookup` is a function that resolves a battery name into a battery object.
        '''
        super().__init__(name)
        self._lookup = lookup
        self._srcnames = srcnames
        self._voltage = voltage
        self._voltage_tolerance = voltage_tolerance

        if len(srcnames) == 0:
            raise BOSErr.NoBattery

        # check voltages within given tolerance
        for srcname in srcnames:
            source = self._lookup(srcname)
            v = source.get_voltage()
            if not (v >= voltage - voltage_tolerance and v <= voltage + voltage_tolerance):
                raise BOSErr.VoltageMismatch

    def refresh(self):
        # TODO: Check to make sure voltages still in range.
        current = self.get_current()
        self.update_meter(current, current)

    def get_status(self):
        '''
        Compute the aggregate status.
        The algorithm (when discharging):
        - max capacity (MC): sum of source max capacities
        - state of charge (SOC): sum of source states of charge
        - current: sum of source currents
        - max discharging current (MDC):
          1. Compute SOC C-rate for each source battery, defined as (src.SOC / src.MDC)
          2. Take the minimum SOC C-rate among the sources and use this as the SOC C-rate of the
             aggregate battery.
          3. Multiply this aggregate SOC C-rate by the SOC to get the maximum discharging current.
        - max charging current (MCC):
          1. Compute inverse SOC C-rate for each source battery, defined as ((src.MC - src.SOC) / src.MCC).
          2. Take the minimum inverse SOC C-rate among the sources and use this as the SOC C-rate of
             the aggregate battery.
          3. Multiply this aggregate inverse SOC C-rate by (MC - SOC) to get the maximum charging
             current.
        '''
        s_max_capacity = 0
        s_cur_capacity = 0
        s_max_discharge_time = 0
        s_max_charge_time = 0
        s_current = 0

        for srcname in self._srcnames:
            source = self._lookup(srcname)
            s_max_capacity += source.get_maximum_capacity()
            s_cur_capacity += source.get_state_of_charge()
            (max_discharge_rate, max_charge_rate) = source.get_current_range()
            s_max_discharge_time = max(s_max_discharge_time,
                                       source.get_state_of_charge() / max_discharge_rate)
            s_max_charge_time = max(s_max_charge_time, (source.get_maximum_capacity() -
                                                        source.get_state_of_charge()) /
                                    max_charge_rate)
            s_current += source.get_current()

        s_max_discharge_rate = s_cur_capacity / s_max_discharge_time
        s_max_charge_rate = (s_max_capacity - s_cur_capacity) / s_max_charge_time

        # update meter
        self.update_meter(s_current, s_current)

        return BatteryStatus(self._voltage,
                             s_current,
                             s_cur_capacity,
                             s_max_capacity,
                             s_max_discharge_rate,
                             s_max_charge_rate)

    def set_current(self, target_current):
        '''
        Set the discharging/charging current of the aggregate battery.
        Each source battery must be set to a discharging/charging current proportional to its SOC,
        so that all source batteries will be depleted / fully charged at the same time.
        The algorithm:
        1. Compute the time to discharge/charge (TTD/TTC) for the aggregate battery:
          TTD = agg.SOC / target_current
          TTC = (agg.MC - agg.SOC) / -target_current
        2. For each source battery src, set the current using the aggregator's TTD/TTC:
          src.set_current(src.SOC / TTD)
          src.set_current((src.MC - src.SOC) / TTC)
        '''
        
        if verbose:
            print(f'set current {target_current}')
        
        old_current = self.get_status().current
        
        (max_discharge_rate, max_charge_rate) = self.get_current_range()
        if not (target_current >= -max_charge_rate and target_current <= max_discharge_rate):
            raise BOSErr.CurrentOutOfRange

        sources = [self._lookup(srcname) for srcname in self._srcnames]

        if target_current > 0: # discharging
            charge = self.get_state_of_charge()
            time = charge / target_current
            for source in sources:
                current = source.get_state_of_charge() / time
                source.set_current(current)
        elif target_current < 0: # charging
            charge = self.get_maximum_capacity() - self.get_state_of_charge()
            time = charge / -target_current
            for source in sources:
                current = (source.get_maximum_capacity() - source.get_state_of_charge()) / time
                source.set_current(current)
        else: # none
            for source in sources:
                source.set_current(0)

        new_current = self.get_status().current
        self.update_meter(old_current, new_current)
        

    @staticmethod
    def type(): return "Aggregator"

    _KEY_SOURCES = "sources"
    _KEY_VOLTAGE = "voltage"
    _KEY_VOLTAGE_TOLERANCE = "voltage_tolerance"
    
    def serialize(self):
        return self._serialize_base({self._KEY_SOURCES: self._srcnames,
                                     self._KEY_VOLTAGE: self._voltage,
                                     self._KEY_VOLTAGE_TOLERANCE: self._voltage_tolerance})

    @staticmethod
    def _deserialize_derived(d: dict, lookup):
        return AggregatorBattery(d[AggregatorBattery._KEY_VOLTAGE],
                                 d[AggregatorBattery._KEY_VOLTAGE_TOLERANCE],
                                 d[AggregatorBattery._KEY_SOURCES],
                                 lookup)
                                 

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
    
            
class BOSDirectory:
    '''
    An structure that stores a map of battery names to battery objects and stores 
    the logical battery topology of BOS.
    The topology is a directed acyclic graph (DAG), where the nodes are battery names.
    
    This structure has 2 purposes:
    (i) provide a dynamic mapping of battery names to battery objects that can be changed at runtime
    (ii) track when a battery is in use or is free.
    
    Purpose (i):
    Having a mapping from battery names to battery objects that is queried at runtime is essential,
    since this allows batteries to be replaced when BOS is running. Suppose the battery name 
    "batt" is mapped to the physical battery <battA>. Then, the user can rebind "batt" to <battB>
    atomically. From the view of the rest of the battery topology, nothing changed, except the
    status reported by "batt" may change.

    Purpose (ii):
    BOS somehow needs to know when a battery is free or is in use. One way to achieve this is to
    maintain a full representation of the battery topology. If a node has no children in the DAG,
    then it is free; otherwise, it is in use. This is the approach the current implementation of 
    BOSDirectory takes.
    An alternative and perhaps superior approach would be to add a reference count to each 
    battery object. This would make BOSDirectory obsolete.
    '''
    
    def __init__(self, lock: threading.RLock):
        self._map = dict() # map battery names to (battery, children)
        self._lock = lock

    def __contains__(self, name: str):
        with self._lock:
            return name in self._map

    def __getitem__(self, name):
        with self._lock:
            if name not in self:
                raise BOSErr.BadName(name)
            return self._map[name]

    def __str__(self):
        with self._lock:
            strd = dict([(k, tuple(map(str, v))) for (k, v) in self._map.items()])
            return str(strd)

    def keys(self):
        with self._lock:
            return self._map.keys()

    def add_node(self, name: str, node: BOSNode, parents=set()):
        with self._lock:
            if name in self:
                raise BOSErr.NameTaken
            self._map[name] = (node, parents)

    def remove_node(self, name: str):
        with self._lock:
            raise NotImplementedError

    def get_parents(self, name: str):
        with self._lock:
            return self._map[name][1]

    def get_children(self, name: str):
        with self._lock:
            children = set()
            for child in self._map:
                if name in self.get_parents(child):
                    children.add(child)
            return children


    def replace_node(self, name: str, newnode: BOSNode):
        with self._lock:
            raise NotImplementedError

    def isfree(self, name: str) -> bool:
        return len(self.get_children(name)) == 0

    def isused(self, name: str) -> bool:
        return not self.isfree(name)
            
        


#     def refresh(self):
#         for name in self._directory:
#             node = self._directory[name][0]
#             if isinstance(node, Battery):
#                 node.refresh()
    

    def print_status(self):
        with self._lock:
            for src in self._map:
                node = self._map[src][0]
                print('{}:'.format(src))
                if isinstance(node, Battery):
                    status = node.get_status()
                    print('\tcurrent:         {}A'.format(status.current))
                    print('\tstate of charge: {}Ah'.format(status.state_of_charge))
                    print('\tmax capacity:    {}Ah'.format(status.max_capacity))
                    print('\tmax discharge:   {}A'.format(status.max_discharging_current))
                    print('\tmax charge:      {}A'.format(status.max_charging_current))
                    print('\tmeter:           {}Ah'.format(node.get_meter()))
                elif isinstance(node, SplitterPolicy):
                    status = node.get_splitter_status()
                    print('\tsource: {}'.format(status["source"]))
                    parts = status["parts"]
                    print('\tparts: {}'.format(len(parts)))
                    for part in parts:
                      print('\t\t{}:'.format(part["name"]))
                      print('\t\t\tscale:   {}'.format(part["scale"]))
                      print('\t\t\tcurrent: {}A'.format(part["current"]))

                  
    def visualize(self):
        '''
        This is experimental and hasn't been tested it a while.
        '''
        with self._lock:
            import networkx as nx
            import matplotlib.pyplot as plt
            g = nx.DiGraph()
            for src in self._map:
                for dst in self._map[src][1]:
                    g.add_edge(dst, src)
            nx.draw_networkx(g)
            plt.show()

    
        
class BOS:
    # Map battery type string to battery class
    battery_types = dict([(battery.type(), battery) for battery in
                         [NullBattery, PseudoBattery, AggregatorBattery, SplitterBattery,
                          JBDBMS]])
    
    def __init__(self, lock=threading.RLock()):
        self._directory = BOSDirectory(lock)
        # self._ble = Interface.BLE()
        self._lookup = lambda name: self._directory[name][0]

    def __str__(self):
        return '{{directory = {}}}'.format(self._directory)
    
    def list(self):
        return self._directory.keys()

    def make_null(self, name: str, voltage) -> NullBattery:
        ''' Construct a null battery with the given name and voltage '''
        battery = NullBattery(name, voltage)
        battery.reset_meter()
        self._directory.add_node(name, battery)
        return battery
        
        
    def make_battery(self, name: str, kind: str, iface: Interface, addr: str, *args):
        ''' 
        Make a physical battery.
        Args:
        - name: name of the physical battery
        - kind: battery type, as string (see BOS.battery_types)
        - iface: interface to connect to battery on
        - addr: address to connect to battery at
        - *args: any additional arguments to pass to specific physical battery constructor
        '''
        if kind == JBDBMS.type():l
            battery = JBDBMS(name, iface, addr, *args)
        else:
            battery_type = self.battery_types[kind]
            battery = battery_type(name, iface, addr, *args)
            battery.reset_meter()
        self._directory.add_node(name, battery)
        return battery

    def make_network(self, name: str, remote_name: str, iface: Interface, addr: str, *args):
        ''' Make a networked battery. '''
        battery = NetworkBattery(name, remote_name, iface, addr, *args)
        self._directory.add_node(name, battery)
        return battery
    
    def make_aggregator(self, name: str, sources: T.List[str], voltage, voltage_tolerance):
        ''' Make an aggregator battery. '''
        battery = AggregatorBattery(name, voltage, voltage_tolerance, sources, self._lookup)
        battery.reset_meter()
        self._directory.add_node(name, battery, set(sources))
        return battery

    def make_splitter_policy(self, policyname: str, policytype, parent: str, initname: str, *policyargs) -> SplitterPolicy:
        '''
        Make a splitter policy. Initializes the policy with one managed battery.
        Args:
        - policyname: name of policy to create
        - policytype: type constructor that will be used to instantiate the battery
        - parent: source of the splitter policy
        - initname: name of the initialization battery
        - *policyargs: subtype-specific arguments to pass to policy constructor
        '''
        if not issubclass(policytype, SplitterPolicy):
            raise BOSErr.InvalidArgument

        policy = policytype(parent, self._lookup, initname, *policyargs)
        self._directory.add_node(policyname, policy, {parent})
        
        # add initial battery
        # TODO: sample period
        init_battery = SplitterBattery(initname, 0, policyname, self._lookup)
        self._directory.add_node(initname, init_battery, {policyname})
        init_battery.reset_meter()
        return policy

    def make_splitter_battery(self, batteryname: str, policyname: str, sample_period, tgt_status,
                              *policyargs):
        '''
        Make a splitter battery.
        This creates a new splitter battery under the exiting policy named `policyname`. 
        It attempts to create the new partition battery with the target status `tgt_status`.
        '''
        battery = SplitterBattery(batteryname, sample_period, policyname, self._lookup)
        self._directory.add_node(batteryname, battery, {policyname})
        policy = self._lookup(policyname)
        policy.add_child(batteryname, tgt_status, *policyargs)
        battery.reset_meter()
        return battery
    

    def get_status(self, name: str) -> BatteryStatus:
        return self._directory[name][0].get_status()

    def set_current(self, name: str, current):
        self.lookup(name).set_current(current)

    def start_background_refresh(self, name: str):
        ''' Start background refresh on the given battery. This spawns another thread that does
        the refreshing in the background. '''
        self.lookup(name).start_background_refresh()

    def stop_background_refresh(self, name: str):
        ''' Stop background refresh on the given battery. This stops the background refresher 
        thread. It may take a couple seconds, depending on the sample period, to stop. '''
        self.lookup(name).stop_background_refresh()

    def refresh(self, name: str):
        self.lookup(name).refresh()
    
    def free_battery(self, name: str):
        # UNTESTED
        return self._directory.free_battery(name)
    
    def replace_battery(self, name: str, *args):
        # UNTESTED
        newbattery = self.make_battery(name, *args)
        self._directory.replace_battery(name, newbattery)
        return battery

    def print_status(self):
        self._directory.print_status()
    
    def visualize(self):
        self._directory.visualize()

    def lookup(self, name: str):
        return self._lookup(name)

    def get_parents(self, name: str):
        return self._directory.get_parents(name)

    def get_children(self, name: str):
        return self._directory.get_children(name)


# The following is just for basic testing of a hard-coded topology.
# It's much easier to use the interpreter to prototype topologies -- see Interpreter.py.
import util
if __name__ == '__main__':
    util.bos_time = DummyTime(0)
    bos = BOS()

    pseudo1 = bos.make_battery("pseudo1", "Pseudo", Interface.BLE, "pseudo1",
                              BatteryStatus(7000, 0, 500, 1000, 12, 12))
    pseudo2 = bos.make_battery("pseudo2", "Pseudo", Interface.BLE, "pseudo2",
                               BatteryStatus(7000, 0, 500, 1000, 12, 12))
    agg = bos.make_aggregator("agg", ["pseudo1", "pseudo2"], 7000, 500, 100)

    # bos.print_status()
    
    agg.set_current(12)

    bos.print_status()

    print('=====')
    
    util.bos_time.tick(3600)

    # splitter
    splitter = bos.make_splitter_policy("splitter", ProportionalPolicy, "agg", "splitter_free")

    # bos.print_status()

    free_status = bos._lookup("splitter_free").get_status()
    bos.make_splitter_battery("part1", "splitter", 0,
                              BatteryStatus(free_status.voltage,
                                            0,
                                            free_status.state_of_charge / 2,
                                            free_status.max_capacity / 2,
                                            free_status.max_discharging_current / 2,
                                            free_status.max_charging_current / 2,
                                            ),
                              "splitter_free",
                              )

    bos.print_status()

    print('=========')

    util.bos_time.tick(3600)

    bos.print_status()
    
    # bos.visualize()
    
