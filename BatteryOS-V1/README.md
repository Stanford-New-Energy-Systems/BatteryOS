# BOS Implementation

## Table of Contents
- [Directory Structure](#DirectoryStructure)
- [BAL](#BALandLogicalBatteries)
  - [Aggregator Batteries](#AggregatorBatteries)
  - [Splitter Batteries](#SplitterBatteries)
  - [Null and Pseudo Batteries](#NullandPseudoBatteries)
- [BOS](#BOS)
- [Splitter Policies](#SplitterPolicies)
- [Expected SOC](#ExpectedSOC)
- [Caching and Refreshing](#CachingandRefreshing)
  - [Background Refreshing](#BackgroundRefreshing)
- [Thread Safety](#ThreadSafety)
- [BOS Interpreter](#BOSInterpreter)
- [Todo](#Todo)

## Directory Structure
- [BatteryInterface.py](BatteryInterface.py): contains interfaces defining the BAL API, including the battery status returned by get_status() and an abstract logical battery class.
- [BOS.py](BOS.py): contains definitions of virtual batteries, including aggregator, splitter, and networked batteries. Also contains the definition of the BOS object, which provides an API for constructing and managing the battery topology.
- [JBDBMS.py](JBDBMS.py): a BAL driver for JBD BMS.
- [Policy.py](Policy.py): contains the splitter policy interface and implementations of proportional and tranched policies, as discussed in the BAL Design Document.
- [Interface.py](Interface.py): contains communication interface definitions for use with BAL drivers and networked batteries.
- [Interpreter.py](Interpreter.py): an interpreter that wraps BOS for easy prototyping of battery topologies.
- [Server.py](Server.py): the server module of BOS that responds to exposes a remote procedure call interface to other BOS nodes. Used by networked batteries to export batteries across BOS nodes.
- [BOSErr.py](BOSErr.py): BOS error definitions.
- [util.py](util.py): common utility functions.
- [demo/](demo/): interpreter scripts that construct test topologies.
- [doc/](doc/): various documentatoin. As far as I can tell, the only useful thing in this directory currently is [JBD_REGISTER_MAP.md](doc/JBD_REGISTER_MAP.md).
- [etc/](etc/): random files.

## BAL and Logical Batteries
For info on the Battery Abstraction Layer, see the BAL Design Document.
_Logical batteries_ are batteries that conform to the BAL.
There are two types of logical batteries: physical batteries and virtual batteries.

Physical batteries are implemented as BAL drivers that map a particular battery management system (BMS) API to the BAL API.
Currently, the only BAL driver we have written is for JBD BMS, contained in [JBDBMS.py](JBDBMS.py).

Virtual batteries create new batteries with different characteristics out of existing logical batteries.
There are three main types of virtual batteries:
- Aggregator battery
- Splitter battery
- Networked battery

See the BAL Design Document for details on these 3 types. 
All three of these virtual batteries are implemented in [BOS.py](BOS.py).

### Aggregator Batteries
The implemention of aggregator batteries is straightforward after understanding the BAL Design Document.
It uses the "variable current" option to compute the aggregate battery status when the source batteries aren't in a balananced state.

**TODO**: we still need to add new fields to the BatteryStatus class for the _ideal_ max discharging/charging currents (MDC/MCC).
Currently, AggregatorBattery only reports the current suboptimal MDC/MCC if the source batteries are unbalanced.

### Splitter Batteries
The implementation of splitter batteries (a.k.a. battery partitions) is a bit more complicated than the others.
This is because battery partitions sharing a common source battery must coordinate outside of the BAL API.
For example, under a proportional splitter policy, a battery partition must know the proportion of the source battery's resources that it has been allotted; however, it can't know this unless it knows about the allotments of all its sibling battery partitions.
To address this difficulty, the implementation of battery partitioning is divided into two parts:
- Splitter policies (implemented as SplitterPolicy in [Policy.py](Policy.py))
- Splitter batteries (implemented as SplitterBattery in [BOS.py](BOS.py)).
Splitter batteries don't do much -- they just forward each BAL API call to the splitter policy that manages them.
For a description of how splitter policies work, see [Splitter Policies](#SplitterPolicies).

### Null and Pseudo Batteries
Other virtual battery types implemented include a null battery, whose status is always zero, and a pseudo battery, which returns a fixed status.
Both of these battery types are most useful for testing purposes; however, null batteries can also be used when swapping out batteries in an existing topology if removing the battery from the topology would cause issues.
For example, consider the case of logical battery _A_ that has a networked battery _B_ connected to in on another BOS node. If you remove _A_, then remote queries by _B_ will return errors; 
however, if you replace _A_ with a null battery, then the remote queries by _B_ will succeed but return a null status, showing that the remote battery is no longer usable by intention, not accident.

## BOS
BatteryOS (BOS) manages topologies of logical batteries, i.e., batteries conforming to BAL. 

## Splitter Policies
Splitter policies manage one or more splitter batteries; each splitter battery is managed by exactly one splitter policy.
The role of splitter policies is to distribute source battery resources among all battery partitions.
See the BAL Design Document for a detailed description of the 3 different policies we've discussed.
Currently, only the _proportional policy_ is fully implemented and tested (Policy.ProportionalPolicy).
The _tranche policy_ is mostly implemented but untested, and it should only be used for reference going forward.

One feature of BOS that splitter policies require is the ability of logical batteries to track their expected state of charge.
This feature is implemented using a meter for each logical battery. For more implementation details (and bugs), see [Expected SOC](#ExpectedSOC).

**TODO**: Write a policy that generalizes the tranche policy and reservation policies. 
These two policies differ only in the direction in which deficits are distributed to battery partitions. 
You can use the incomplete _tranche policy_ implementation as reference. 

## Expected SOC
Logical batteries estimate their own expected state of charge (SOC) using a meter.
As described in [Splitter Policies](#SplitterPolicies), splitter policies must know the expected SOC of each battery partition.
Instead of just adding a mechanism for calculating expected SOC to SplitterBattery's, we added it to all logical batteries (in `BatteryInterface.Battery`) in case it becomes useful in other situations as well.

Essentially, under the current implementation logical batteries compute the midpoint Riemann sum of the max (dis)charging current over time,
where the intervals of the definite integral are demarcated by invokations of the battery's BAL interface.

To explain it in a procedural way:
Logical batteries all have a `meter` field that holds the present estimation of their expected SOC in amp-hours (Ah).
This value is updated by the `Battery.update_meter()` method, which is called whenever BOS issues a BAL API call to the battery (e.g. `Battery.get_status()`, `Battery.set_current()`, or `Battery.refresh()`).
To update its meter, the logical battery calculates the time since its last meter update and multiplies that by the average of the begin and end currents of the present interval.
Note that care must be taken when making instantaneous changes to the current, namely in `Battery.set_current()` -- the end current of the interval is the current _before_ the new current is set.

Also, `update_meter()` requires the end current of the present interval (`endcurrent`) and begin current of the next interval (`begincurrent`) as arguments.
For most meter updates, for example the update during the `get_status()` method, `begincurrent == endcurrent`, since the current won't be instantaneously changing as an effect of the BAL API call.
However, `set_current()` _does_ instantaneously change the current (theoretically), so it sets `endcurrent = self.get_current()`, then sets the new current `newcurrent` on the logical battery, then sets `begincurrent = newcurrent`, and then invokes `self.update_meter(endcurrent, begincurrent)`.
Another way to think about the importance of instantaneous change of current is in terms of Riemann sums: to correctly do a Riemann sum of a discontinuous function, you need to use the left-hand limit (`endcurrent`) for the left interval and the right-hand limit (`begincurrent`) for the right interval.

**TODO**: there appears to be a bug somewhere in the metering system so that some logical batteries report incorrect meter values. This needs to be fixed!

## Caching and Refreshing
Caching battery statuses in BOS is essential for decent performance, since battery topologies can be arbitrarily deep.
All batteries that must communicate with external devices to obtain status information, including networked batteries and physical batteries, cache battery statuses. 
Other batteries, for example aggregator or splitter batteries, have no need to cache the status since the status can be recomputed quickly in software.

The counterpart to caching is refreshing, the act of retrieving the battery status.
A refresh is implicitly issued if the cached status is stale; it can also be explicitly issued as part of the BAL API (as `Battery.refresh()`) at any time.
Note that refreshes are _not_ recursive in the battery topology;
that is, if you refresh a network battery, it doesn't also refresh the remote source battery on the other BOS node.
Perhaps recursive refreshes would be useful as well; it possibly could be a parameter to the BAL API `refresh()` method.

### Background Refreshing
In the default operating mode, battery status refreshes are only triggered when the user calls `get_status()` on the battery and the cached status is stale. 
This can have negative ramifications for the accuracy of the [expected SOC](#ExpectedSOC), since if `get_status()` is called infrequently, there will be fewer intervals to compute the Riemann sum with, resulting in a less accurate approximation.
To alleviate this issue, we introduced optional _background refreshing_ on a per-battery basis, which refreshes the battery in the background at a parameterized frequency.
This is achieved using multithreading, thus introducing thread safety issues, which are discussed in the section [Thread Safety](#ThreadSafety).

## Thread Safety
Due to background refreshing, there may be other threads accessing the BOS directory or reading or writing the status of a battery at any given time.
As a result, we added a lock to the BOS directory (which stores the battery topology) and a lock to each logical battery.
Almost all operations on a battery are wrapped in a lock context as to prevent race conditions.

## BOS Interpreter
The BOS interpreter is used for prototyping and testing. 
See [Interpreter.py](Interpreter.py) for implementation details and a list of commands.
The interpreter allows you to easily construct arbitrary battery topologies.
Also, you can write scripts for predefined topologies and execute them in the interpreter.
You can take a look at some scripts for basic topologies we used on the testbed under the [demos/](demos/) directory.

## Todo
- Tranche and reservations policies are not fully implemented. They need to be implemented as one generalized splitter policy class, with parameterized directions for how to distribute surpluses and deficits.
  - Also, look into how to distribute stats other than SOC, for example max capacity, in these policies. One method is to simply give the highest tranche all the surplus max capacity (under a tranche policy).
- The BatteryStatus structure currently only contains the present maximum discharging/charging current, not the ideal maximum. This is important for aggregate batteries, whose present MDC/MCC may be less than the ideal MDC/MCC. The ideal max MDC/MCC should be added to `BatteryInterface.BatteryStatus`.
- There is currently no timestamping of statuses that conveys the staleness of the status reported through `get_status()`. This needs to be added somehow.
