Tests
==================================

Introduction
------------

This directory contains multiple different tests that show how virtualization and the Battery
Operating System function. In order to run one of the tests, protobuf must be installed onto 
the machine. To install protobuf, run the command **make protobuf**. A description of each of 
the tests can be found below:

- [testMerge][merge]: This test evaluates how events are merged together when a battery schedules 
a current event and then later changes the event before it occurs. For example, if a battery is 
scheduled to discharge at 5kW from noon to 2pm, and then later another event is scheduled to 
discharge at 10kW from 1pm to 2pm, the battery will discharge at 5kW from noon to 1pm followed by 
10kW from 1pm to 2pm. We test a few different merge possibilities and ensure that the system works 
properly. The executable can be formed by using the command **make merge**.

- [testAggregate][aggregate]: This test shows how physical batteries can be aggregated together. In this 
test, two physical batteries with varying power outputs and capacities are aggregated together to form
a single aggregate battery. A discharge command is sent to the aggregate battery and each physical battery 
prints out its output current as a result. The status of the battery is also printed to make sure the status 
is consistent with the defintion of an aggregate battery as found in the [BAL Design Document][design]. The
executable can be formed by using the command **make aggregate**.

- [testPartition][partition]: This test shows how logical batteries can be partitioned. In this test, two physical 
batteries are aggregated together, and then partitioned into two batteries of equal size. These two betteries are 
aggregated together and then partitioned again. Current events are scheduled to these partitions and the values 
traverse up the topology to the physical batteries. The executable can be formed using the command **make partition**. 

- [testDirectory][directory]: This test demonstrates the functionality of the directory that holds the batteries.
A few physical batteries are created and added to the directory. Edges are added to the directory as well to establish
the graphical topology between batteries. A parent battery is removed to test the recursive battery removal from the 
directory. 

- [testDirectoryManager][manager]: This test demonstrates the functionality of the directory manager. The directory manager
provides APIs for creating a multitude of various batteries. These functions automatically create a specified battery as 
well as add the corresponding edges within the directory. In this test, a somewhat complex battery topology is created. Once 
this topology is created, current commands are scheduled at the leaves of the topology to test how well these values propogate
up the nodes of the graph. A visual representation of the battery topology created in this test can be found in the [documentation][doc]
folder under the title [test\_topology][topology]. The executable can formed by using the command **make manager**. 

- [bos][bos]: This file is a supplement to some of the other tests ([testFifo][fifo] and [testJBDBMS][jbd]). This code is responsible for creating a 
directory to hold the batteries as well as creating an admin input/output fifo. After this directory is created, the Battery Operating 
System is ready to respond to commands written to the admin fifos. Additional batteries fifos will be created with the batteries 
directory and commands written to those corresponding files will also be responded to. The executable can be formed using the command
**make bos**.

- [testFifo][fifo]: This file is used to test the named fifos for communication with BOS. In order to run this executable, the [bos][bos] 
executable must first be compiled and executed. A complex topology of batteries is created by first first writing to the admin fifos 
to construct the topology. A sequence of current events are scheduled starting at the leaf nodes of the topology. The commands travel up
the nodes of the topology until they reach the physical batteries that execute the command. This test shows how aggregate batteries, 
partition batteries, and physical batteries work together in a topology. This test shows BOS's ability to combine requests coming from 
various partitions through isolation. A visual representation of the battery topology created in this test can be found in the [documentation][doc]
folder under the title [test\_topology][topology]. The executable can be formed using the command **make fifoTest**.

- [socket][socket]: This file is a supplement to some of the other tests ([testSocket][socketTest]). This code is responsible for creating a
directory to hold the batteries as well as creating network sockets for connections to the admin and subsequent batteries. The Battery 
Operating System waits for connections to the admin socket and responds to commands sent over the network. Subsequent batteries that are 
created are given their own independent socket and those individual commands are responded to as well. The executable can be formed using
**make socket**.

- [testSocket][socketTest]: This file is used to test the network sockets for communication with BOS. In order to run this executable, the
[socket][socket] executable must first be compiled and executed. The same topology created in the [testFifo][fifo] is created. However, in
this file the batteries are first created through communication with the admin socket. Individual current events are scheduled by communication
through the network sockets for the leaf batteries in the topology. This tests analyzes the accuracy and the ability for the commands to traverse
up the topology. The output of this test should be the same as the fifo test. A visual representation of the topology used in this test can be found
in [documentation][doc] folder under the title [test\_topology][topology]. The executable can be formed using **make socketTest**.

- [testPseudo][pseudo]: This file is used to test the Pseudo battery (which is a software battery). A Pseudo battery is created and discharged for a 
minute. Every six seconds, the status of the battery is printed to ensure that the battery is operating in the correct manner. The executable can be 
formed using **make pseudo**. 

- [testDynamic][dynamic]: This file is used to test dynamic batteries whose functions are compiled in a dynamic library. The function names of the drivers
are provided and the Battery Operating System is responsible for linking them. The executable can be formed using **make dynamic**.

- [testJBDBMS][jbd]: This file is used to test the JBD Battery Management System (BMS). The function names of the battery driver are provided and the
Battery Operating System is responsible for linking them so that they can be used. The executable can be formed using **make bms**.

To make sure that a system is set up to compile the code, a few commands in the makefile can be run. The command: **make** checks the python
version installed on the system. **PYTHON 3.8 or ABOVE** is required to compile the software. If the system does not have python3.8, python3.8
needs to be installed first before continuing. Secondly, the command: **make install** can be run. This command checks to see if the protoc
compiler is installed on the system as well as pkg-config and python3-config. If these are not already on the system, three different shell scripts
are invoked installing the commands. For pkg-config and python3-config, the commands are installed using apt-get (for linux) and brew (for mac).
The protoc compiler source code is downloaded from GitHub and installed from source from instructions from protobuf. **The protoc version that is used 
to compile this program is 3.21.6.** Some of these shell scripts use the sudo command and require root access, so the root password will need to be
provided by the user. Protoc and the other packages can be installed manually as well, but the **make install** command should install the tools
necessary to build the system. Finally, once that is complete the command: **make run** can be called to compile the software. To run any of the tests in
this file reference the text above.   

[merge]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/tests/testAggregate.cpp
[aggregate]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/tests/testAggregate.cpp
[design]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/doc/Task%202.2%20BAL%20Document.pdf
[partition]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/tests/testPartition.cpp
[directory]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/tests/testDirectory.cpp
[manager]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/tests/testDirectoryManager.cpp
[jbd]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/tests/testJBDBMS.cpp
[bos]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/tests/fifo.cpp
[doc]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/tree/bos_rewrite/bos_rewrite/doc
[fifo]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/tests/testFifo.cpp
[pseudo]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/tests/testPseudo.cpp
[topology]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/doc/test_topology.png
[socket]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/tests/socket.cpp
[socketTest]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/tests/testSocket.cpp
[dynamic]: https://github.com/obinnoromjr/BOS/blob/main/tests/testDynamic.cpp
