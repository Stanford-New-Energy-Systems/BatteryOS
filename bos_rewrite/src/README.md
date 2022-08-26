# BOS Implementation (C++)
### Table of Contents

* [Directory Structure](#directory-structure)
* [Battery Abstraction Layer](#battery-abstraction-layer)
    * [Aggregate Batteries](#aggregate-batteries)
    * [Partitioned Batteries](#partitioned-batteries)
* [Battery Operating System](#battery-operating-system)
* [Splitter Policies](#splitter-policies)
* ToDo

### Directory Structure
[node.hpp][node]: Defines the various battery types as well as a general node in the BOS  
[BatteryInterface.cpp][BatteryInterface]: Defines the _Battery_ class and specifies the members within the class. The _Battery_ class defines important member functions for scheduling/setting the current of a battery as well as refreshing the current information that is known about the battery. The VirtualBattery_ and _PhysicalBattery_ classes are also defined in this file.  
[BatteryDirectory.hpp][BatteryDirectory]: Defines the _BOSDirectory_ class and specifies the members within the class. The _BOSDirectory_ class represents the graph topology used to manage partioned or aggregated batteries. The class provides member functions for adding edges as well as determining the parent/children of a battery in the graph.  
[BatteryStatus.cpp][BatteryStatus]: Defines the _BatteryStatus_ struct and specifies the members within the struct. The _BatteryStatus_ struct maintains important information about a battery such as the voltage and current of the battery. This file also provides functions for serializing and deserializing data such as the _BatteryStatus_.  
[AggregateBattery.cpp][Aggregate]: Defines the _AggregatorBattery_ class and specifies the members within the class. The **refresh** and **schedule_set_current** functions (defined in the _Battery_ class found in the [BatteryInterface] file) are overwritten to follow the correct procedure for an aggregate battery.  
[BALSplitter.cpp][BALSplitter]: Defines the _BALSplitter_ class and specifies the members within the class. The **refresh** and **schedule_set_current** functions (defined in the _Battery_ class found in the [BatteryInterface] file) are overwritten to follow the correct procedure for an partitioned battery. The _BALSplitterType_ enum class is also defined to specify the different policies that a partitioned battery can have. The _BALSplitter_ class also defines refresh functions specific to the partitioned battery policy.  
[NetworkBattery.hpp][NetworkBattery]: Defines the _NetworkBattery_ class specifies the members within the class. The _NetworkBattery_ class provides functionality for referring to batteries through a TCP connection. The **refresh** and **schedule_set_current** functions (defined in the _Battery_ class found in the [BatteryInterface] file) are overwritten to follow the correct procedure for a battery connected to a network.  
[RPC.cpp][RPC]: Provides server functionality  
[Connections.hpp][Connections]: Provides various server/client connection functions  
[util.cpp][util]: Provides useful utility functions for error checking, logging, etc.  
[JBDBMS.cpp][JBDBMS]: Driver for JDBBMS


[node]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/main/bosinc/BOSNode.hpp

[BatteryInterface]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/tree/main/bosinc

[BatteryDirectory]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/main/bosinc/BOSDirectory.hpp

[BatteryStatus]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/main/bosinc/BatteryStatus.cpp

[Aggregator]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/main/bosinc/AggregatorBattery.cpp

[BALSplitter]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/main/bosinc/BALSplitter.cpp

[NetworkBattery]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/main/bosinc/NetworkBattery.hpp

[RPC]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/main/bosinc/RPC.hpp

[Connections]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/main/bosinc/Connections.hpp

[util]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/main/bosinc/util.cpp

[JBDBMS]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/main/bosinc/JBDBMS.cpp

### Battery Abstraction Layer
The Battery Abstraction Layer (BAL) is a software abstraction for battery eneergy storage. Its intended use case is battery systems being used as distributed energy resources (DERs) in the electric grid. More information on the BAL can be found in the [BAL Design Document][BAL Design Document].

_Logical batteries_ are batteries that conform to the BAL. There are two types of logical batteries: **physical** batteries and **virtual** batteries. 

Physical batteries are implemented as BAL drivers that map a particular battery management system (BMS) API to the BAL API. In other words, physical batteries are the actual batteries themselves. The BAL drivers allow for control of the physical batteries to integrate with the BAL API. For example, scheduling the current of a battery is one part of the functionality that the BAL API provides. The BAL driver is thus responsible for providing the functionality of setting the current on the battery itself. 

Virtual batteries create new batteries with different characteristics out of existing logical batteries. A virtual battery can either be formed from a physical battery or from other virtual batteries. There are three main types of virtual batteries:
 
 * **Aggregate** batteries
 * **Partitioned** batteries
 * **Networked** batteries 

 Further detail on these three batteries types can be found in the [BAL Design Document]. 

 #### Aggregate Batteries

 The implementation of aggregate batteries can be found in the [BAL Design Document]. The aggregate batteries use the "variable current" option to compute the aggregate battery status when the source batteries are not in a balanced state. 

 #### Partitioned Batteries 

 The implementation of partitioned batteries (i.e. splitted batteries) is a bit more complicated than others. This is because battery partitions sharing a common source battery must coordinate outside of the BAL API. For example, under a proportional splitter policy, a battery partition must know the proportion of the source battery's resources that it has been allotted. However, it can't know this unless it knows about the allotments of all its sibling battery partitions. To address this, the implementation of battery partitioning is divided into two parts: 

 * Splitter Policies
 * Splitter Batteries 

[BAL Design Document]: https://www.google.com

#### Battery Operating System 
The Battery Operating System (BOS) manages topologies of logical batteries (batteries conforming to the BAL)

#### Splitter Policies
