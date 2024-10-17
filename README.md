# BatteryOS

BatteryOS is a software package for software virtualization of batteries, particularly 
grid-attached batteries in homes and home battery/solar installations. BatteryOS allows
you to aggregate and partition batteries and supports a simple driver interface for connecting
it to physical batteries. The core ideas in BatteryOS are described in

  [Software defined grid energy storage.](https://dl.acm.org/doi/10.1145/3563357.3564082) Sonia Martin, Nicholas Mosier, Obi Nnorom, Yancheng Ou, Liana Patel, Oskar Triebe, Gustavo Cezar, Philip Levis, Ram Rajagopal. In BuildSys '22: Proceedings of the 9th ACM International Conference on Systems for Energy-Efficient Buildings, Cities, and Transportation, 2022.

This [design document report](https://github.com/Stanford-New-Energy-Systems/BatteryOS/blob/main/BatteryOS-V3/doc/Task%202.2%20BAL%20Document.pdf) contains a more complete description of the software system.

BatteryOS has gone through three implementations:
  - V1: The first prototype, written in Python.
  - V2: The first full version, written in C++.
  - V3: A clean rewrite in C++, including support for secure aggregation.

Secure aggregation is an advanced feature that allows a battery owner to present
a private partition interface to clients . In this model, clients control partitions of
the battery. The owner knows the sum of all the client requests (i.e., whether the battery
is charging or discharging), but individual client requests remain private. The techniques
to acheive this are described in

   [Privacy-Preserving Control of Partitioned Energy Resources.](https://dl.acm.org/doi/10.1145/3632775.3661988) Evan Laufer, Philip Levis, and Ram Rajagopal. Published in e-Energy '24: Proceedings of the 15th ACM International Conference on Future and Sustainable Energy Systems, June 2024.
