PY_CFLAGS = $(shell python3-config --cflags)
PY_LDFLAGS = $(shell python3-config --ldflags)

PY_CFLAGS_EMBED = $(shell python3-config --cflags --embed)
PY_LDFLAGS_EMBED = $(shell python3-config --ldflags --embed)

CURL_LDFLAGS = $(shell curl-config --libs)

CPP_FILES = main.cpp 
CPP_FILES += BatteryStatus.cpp 
CPP_FILES += util.cpp
CPP_FILES += JBDBMS.cpp
CPP_FILES += BatteryInterface.cpp
CPP_FILES += RPC.cpp
CPP_FILES += AggregatorBattery.cpp
CPP_FILES += BALSplitter.cpp

C_FILES = include/wiringSerial.c


.PHONY: mac
mac:
	clang++ -std=c++17 $(PY_CFLAGS_EMBED) $(PY_LDFLAGS_EMBED) $(CPP_FILES) $(C_FILES) -g $(CURL_LDFLAGS)

.PHONY: rpi
rpi:
	g++ -std=c++17 $(PY_CFLAGS) $(PY_LDFLAGS) $(CPP_FILES) $(C_FILES) -g $(CURL_LDFLAGS) -lpthread -fPIE
	
.PHONY: wsl
wsl:
	g++ -std=c++17 $(PY_CFLAGS_EMBED) $(CPP_FILES) $(C_FILES) -g $(CURL_LDFLAGS) -lpthread $(PY_LDFLAGS_EMBED) -fPIE

