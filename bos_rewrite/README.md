Battery Operating System
==================================

Note: the src folder has more specific documentation about the system

Introduction
------------

The Battery Operating System (BOS) is designed for virtualizing energy storage batteries typically 
used in combination with distributed energy resources (DERs). BOS provides support for aggregating 
and partitioning physical or virtual batteries into complex topologies that can be used for a variety 
of use cases. BOS is written in C++ and uses named FIFOs to send commands to batteries on local systems.

Physical Batteries
------------------

Using this library, physical batteries of varying vendors can be controlled through a common API. BOS 
supports the control of physical batteries through device drivers. To build a driver compatible with our system,
two functions must be implemented: **refresh()** and **set_current(current)**. The first function returns 
the status of the battery. The status values returned from this function include: 

- Voltage  (in mV) 
- Current  (in mA)
- Capacity (in mAh)
- Max Capacity (in mAh)
- Max Charging Current (in mA)
- Max Disharging Current (in mA)
- Time (when the refresh event occurred)

Upon construction of a battery, a _maximum staleness value_ and _refresh mode_ is chosen. The maximum staleness 
parameter determines how frequently the status of the battery is queried. The refresh mode parameter determines 
if this querying is done passively. If an **active** refresh mode is chosen, the status of the battery will be 
updated periodically with a period of the max staleness. However if a **lazy** refresh mode is chosen, the status
of the battery will be updated only when both getStatus() is called **and** it's been longer than the maximum 
staleness since the battery has been updated.   

Our library currently supports a few different physical batteries including a JBD Battery Management System (BMS),
a Sonnen battery, and batteries that conform to the IEC61850 specifications. The code for these battery drivers can 
be found within the [src][src] folder. BOS also supports the addition of new battery drivers. This is done through a 
dynamic library. All device drivers are written in the [device\_drivers][drivers] folder. A dynamic library is created 
from this library allowing for the driver functions to be linked and loaded into the program. 

Virtual Batteries
------------------

The functionality of a few different virtual batteries is presented in this library. Specifically, an **AggregateBattery**
and **PartitionBattery** are virtual batteries that support the aggregation and partition necessary to virtualize the physical 
batteries. An AggregateBattery takes multiple constituent batteries and ensures that these batteries behave as if they were a
single battery. This means that batteries with various states of charge and power outputs will behave as one cohesive unit, 
providing a constant maximum power output throughout an entire charge or discharge cycle. An AggregateBattery achieves this by 
using the C-Rates of the constituent batteries. More information can be found in the [documentation][doc] directory. A
PartitionBattery takes a single source battery and splits it into multiple smaller child batteries. The capacity and power 
output is split amongst the child batteries based on proportions chosen when the PartitionBattery is created. This partitioning 
can be done proportionally (each child battery has an equal proportion of the capacity and power output of the source) or 
the child batteries could be split such that some of the batteries have large power outputs and lower capacities while others 
have large capacities and low power outputs. Each child battery can send charge/discharge commands based on individual uses.
These commands are summed together and the source battery operates based on the sum of the commands. More information about 
the functionality of virtual batteries can be found in the [doc] directory.


Logical Batteries
------------------

Logical Batteries refer to batteries that are either physical or virtual. Logical Batteries provide a common API that supports
two functions: **getStatus()** and **schedule_set_current(current, start, end)**. The getStatus() function returns the status 
of the logical battery. The schedule\_set\_current function allows for charge/discharge events to be scheduled ahead of time. 
The benefit of this functionality is explained clearly in the [doc] folder. Because these systems are susceptible to network 
disconnections, scheduling charge/discharge events ensure that a battery can meet the requirements of a user even if a network
disconnection occurs. 


Named FIFOs
-----------

In order to control the batteries on a local system, BOS uses named FIFOs to communicate with the batteries. An admin FIFO is 
created upon construction of BOS. The admin allows for batteries to be created or destroyed. It also shut downs the entire system
and removes all the batteries. Whenever a battery is created, an input and output FIFO are created. The input FIFO receives commands
(getStatus() or schedule\_set\_current()) and the output FIFO is used to write responses from the commands (i.e. the status of the 
battery from a getStatus() command). When a battery is removed, its corresponding FIFOs are also removed. 

We use [Protobuf][proto] to serialize the messages to the FIFOs. The proto files showing the structure of the serialized messages 
can be found in the [protobuf][protobuf] folder. The **FifoBattery** found in the [src] directory provides a battery that remains
consistent with the common API of a logical battery. Whenever a getStatus or schedule\_set\_current command is called on a FifoBattery,
the command is serialized and written into the corresponding FIFO. The response is then parsed and outputted to standard output. 


Remote Batteries
------------------

BOS also supports the usage of networked batteries. Networked Batteries are logical batteries that are controlled remotely over a network.
Commands are sent across a network and BOS responds to these commands and performs the corresponding actions. We provide a **ClientBattery**
that behaves similary to the FifoBattery. Instead of serializing commands and writing to a named FIFO, the ClientBattery serializes the
commands and sends them over a network. 


Tests
------------------

The [tests][tests] directory provides various tests that highlight the functionality of the system. More information for how to recreate 
and run the tests can be found in the directory.

[src]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/tree/bos_rewrite/bos_rewrite/src/device_drivers
[drivers]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/tree/bos_rewrite/bos_rewrite/src/device_drivers
[doc]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/tree/bos_rewrite/bos_rewrite/doc
[proto]: https://developers.google.com/protocol-buffers
[protobuf]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/tree/bos_rewrite/bos_rewrite/protobuf
[tests]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/tree/bos_rewrite/bos_rewrite/tests
