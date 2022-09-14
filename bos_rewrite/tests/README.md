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
properly.

- [testAggregate][aggregate]: This test shows how physical batteries can be aggregated together. In this 
test, two physical batteries with varying power outputs and capacities are aggregated together to form
a single aggregate battery. A discharge command is sent to the aggregate battery and each physical battery 
prints out its output current as a result. The status of the battery is also printed to make sure the status 
is consistent with the defintion of an aggregate battery as found in the [BAL Design Document][design]. 


[merge]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/tests/testAggregate.cpp
[aggregate]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/tests/testAggregate.cpp
[design]: https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/bos_rewrite/bos_rewrite/doc/Task%202.2%20BAL%20Document.pdf
