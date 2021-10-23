PY_CFLAGS = $(shell python3-config --cflags)
PY_LDFLAGS = $(shell python3-config --ldflags)

PY_CFLAGS_EMBED = $(shell python3-config --cflags --embed)
PY_LDFLAGS_EMBED = $(shell python3-config --ldflags --embed)

CPP_FILES = main.cpp 
CPP_FILES += BatteryStatus.cpp 

C_FILES = include/wiringSerial.c


.PHONY: mac
mac:
	clang++ -std=c++17 $(PY_CFLAGS_EMBED) $(PY_LDFLAGS_EMBED) $(CPP_FILES) $(C_FILES) -g

.PHONY: rpi
rpi:
	g++ -std=c++17 $(PY_CFLAGS) $(PY_LDFLAGS) $(CPP_FILES) $(C_FILES) -g 

