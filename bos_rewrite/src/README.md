# BOS Implementation (C++)
### Table of Contents

* [Directory Structure](#directory-structure)
* [Battery Abstraction Layer](#battery-abstraction-layer)
    * [Aggregate Batteries](#aggregate-batteries)
    * [Partitioned Batteries](#partitioned-batteries)
* [Battery Operating System](#battery-operating-system)
* [Splitter Policies](#splitter-policies)

### Directory Structure
[node.hpp][node]: Defines the various battery types as well as a general node in the BOS  
[refresh.hpp][refresh]: Defines the refresh modes of a battery  
[scale.hpp][scale]: Defines the _scale_ struct used for representing battery capacity and charge proportions  
[event\_t.hpp][event\_t]: Defines the _event\_t_ struct used for representing battery events  
[BatteryInterface.cpp][BatteryInterface]: Defines the _Battery_ class and specifies the members within the class. The _Battery_ class defines important member functions for scheduling/setting the current of a battery as well as refreshing the current information that is known about the battery.   
[PhysicalBattery.cpp][PhysicalBattery]: Defines the _PhysicalBattery_ class and specifies members within the class. Physical Batteries should implement the **refresh** and **set_current** functions.  
[VirtualBattery.cpp][VirtualBattery]: Defines the _VirtualBattery_ class and specifies members within the class.   
[BatteryDirectory.hpp][BatteryDirectory]: Defines the _BatteryDirectory_ class and specifies the members within the class. The _BatteryDirectory_ class represents the graph topology used to manage partioned or aggregated batteries. The class provides member functions for adding edges as well as determining the parent/children of a battery in the graph.  
[BatteryStatus.cpp][BatteryStatus]: Defines the _BatteryStatus_ struct and specifies the members within the struct. The _BatteryStatus_ struct maintains important information about a battery such as the voltage and current of the battery.   
[AggregateBattery.cpp][AggregateBattery]: Defines the _AggregateBattery_ class and specifies the members within the class. The **refresh** and **schedule_set_current** functions (defined in the _Battery_ class found in the [BatteryInterface] file) are overwritten to follow the correct procedure for an aggregate battery.    
[PartitionManager.cpp][PartitionManager]: Defines the _PartitionManager_ class and specifies the members within the class. The **refresh** and **schedule_set_current** functions (defined in the _Battery_ class found in the [BatteryInterface] file) are overwritten to follow the correct procedure for a partition manager. The _PartitionManager_ is responsible for managing the _PartitionBatteries_ by forwarding the sum of current events to the source and maintaining the partition policies among the batteries.   
[PartitionBattery.cpp][PartitionBattery]: Defines the _PartitionBattery_ class and specifies members within the class. The **refresh** and **schedule_set_current** functions are overwritten to follow the correct procedure for a partitioned battery. Commands are sent up to the partition manager before being sent to the corresponding source batteries.   
[DynamicBattery.cpp][DynamicBattery]: Defines the _DynamicBattery_ class and specifies members within the class. This class allows for battery drivers to be written and used without recompiling the entirety of BOS. The **refresh** and **set_current** functions are written in a dynamic library and those functions are loaded into the _DynamicBattery_.   
[BatteryDirectoryManager.cpp][BatteryDirectoryManager]: Defines the _BatteryDirectoryManager_ class and specifies the members within the class. The battery directory manager is responsible for creating batteries and inserting them into the directory. The battery directory also removes batteries from the directory.  
[BOS.cpp][BOS]: Defines the _BOS_ class and specifies the members within the class. The Battery Operating System runs locally on a machine and allows for batteries to be created locally or across a network. Battery commands are written to named FIFOs on the local machine. BOS reads these commands and performs corresponding actions. Battery commands can also be sent across a network. BOS listens to these commands and performs the corresponding actions.    
[ClientBattery.cpp][ClientBattery]: Defines the _ClientBattery_ class and specifies the members within the class. The ClientBattery is specifically useful for sending battery commands across the network that _BOS_ can interpret. The same API is shown (**getStatus** and **schedule_set_current**) and these commands are serialized and sent over the network.    
[Admin.cpp][Admin]: Defines the _Admin_ class and specifies the members within the class. Admin allows a user to send commands that are either sent over a network or written to an admin FIFO locally. A user is presented with functions to create a multitude of batteries. These commands are then serialized and sent over the specified medium.    
[FifoBattery.cpp][FifoBattery]: Defines the _FifoBattery_ class and specifies the members within the class. The FifoBattery is similar to the _ClientBattery_ except it sends commands to the named FIFOs. Similarly, the functions **getStatus** and **schedule_set_current** are provided and these commands serialize the information and write it to the named FIFOs.   
[ProtoParameters.cpp][ProtoParameters]: Defines a few functions for parsing serialized commands.  
[util.cpp][util]: Provides useful utility functions for error checking, logging, etc.  
[device\_drivers][drivers]: directory holding all physical battery drivers (used to make dynamic library)

[node]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/node.hpp 

[refresh]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/refresh.hpp

[scale]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/scale.hpp

[event\_t]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/event_t.hpp

[util]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/util.hpp

[BatteryInterface]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/BatteryInterface.cpp 

[PhysicalBattery]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/PhysicalBattery.cpp

[VirtualBattery]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/VirtualBattery.cpp

[DynamicBattery]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/DynamicBattery.cpp

[FifoBattery]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/FifoBattery.cpp

[Admin]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/Admin.cpp

[BOS]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/BOS.cpp

[ProtoParameters]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/ProtoParameters.cpp

[BatteryDirectoryManager]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/BatteryDirectoryManager.cpp

[BatteryDirectory]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/BatteryDirectory.cpp 

[BatteryStatus]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/BatteryStatus.cpp

[AggregateBattery]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/AggregateBattery.cpp

[PartitionManager]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/PartitionManager.cpp

[PartitionBattery]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/PartitionBattery.cpp

[ClientBattery]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/src/ClientBattery.cpp

[driver]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/tree/bos_rewrite/bos_rewrite/src/device_drivers

### Battery Abstraction Layer
The Battery Abstraction Layer (BAL) is a software abstraction for battery eneergy storage. Its intended use case is battery systems being used as distributed energy resources 
(DERs) in the electric grid. More information on the BAL can be found in the [BAL Design Document][BAL Design Document].

_Logical batteries_ are batteries that conform to the BAL. There are two types of logical batteries: **physical** batteries and **virtual** batteries. 

Physical batteries are implemented as BAL drivers that map a particular battery management system (BMS) API to the BAL API. In other words, physical batteries are the actual batteries themselves. 
The BAL drivers allow for control of the physical batteries to integrate with the BAL API. For example, scheduling the current of a battery is one part of the functionality that the BAL API provides.
 The BAL driver is thus responsible for providing the functionality of setting the current on the battery itself. 

Virtual batteries create new batteries with different characteristics out of existing logical batteries. A virtual battery can either be formed from a physical battery or from other virtual batteries.
 There are three main types of virtual batteries:
 
 * **Aggregate** batteries
 * **Partitioned** batteries
 * **Networked** batteries 

 Further detail on these three batteries types can be found in the [BAL Design Document]. 

 #### Aggregate Batteries

 The implementation of aggregate batteries can be found in the [BAL Design Document]. The aggregate batteries use the "variable current" option to compute the aggregate battery status when the source batteries are not in a balanced state. 

 #### Partitioned Batteries 

The implementation of partitioned batteries is a bit more complicated than others. This is because battery partitions sharing a common source battery must coordinate outside of the BAL API.
For example, under a proportional splitter policy, a battery partition must know the proportion of the source battery's resources that it has been allotted. However, it can't know this unless it knows about the allotments
of all its sibling battery partitions. To address this, the implementation of battery partitioning is divided into two parts: 

 * Partition Policies
 * Partition Managers 

[BAL Design Document]: https://github.com/obinnoromjr/BOS/blob/main/doc/Task%202.2%20BAL%20Document.pdf

#### Battery Operating System 
The Battery Operating System (BOS) manages topologies of logical batteries (batteries conforming to the BAL)

#### Partition Policies

