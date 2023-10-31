GPP = g++
GCC = gcc

.SILENT: check_python_version

OS := $(shell uname -s)
ifeq ($(OS),Darwin)
    target = mac
else 
    target = linux
endif

# check if python exists and then ensure
# python version is at least 3.8

PROTOC   := $(shell which protoc)
PYTHON   := $(shell which python3)
VERSION  := $(shell python3 --version)

PYCONFIG  := $(shell which python3-config)
PKGCONFIG := $(shell which pkg-config)

PY3 = $(word 3, $(subst ., ,$(VERSION)))

INC_DIR = ../include/
CPP_FILES = $(wildcard ../src/*.cpp)

BATTERY_DRIVERS = $(wildcard ../src/device_drivers/*.cpp)
BATTERY_OBJECTS = $(patsubst %.cpp,%.o,$(BATTERY_DRIVERS))

OBJS = $(patsubst %.cpp,%.o,$(CPP_FILES))
OBJS += ../protobuf/battery.pb.o
OBJS += ../protobuf/battery_manager.pb.o
OBJS += ../src/wiringSerial.o
OBJS += $(BATTERY_OBJECTS)

LFLAGS = -g -L. -lbprivacy -L../../libiec61850/.install/lib -pthread -liec61850 -lssl -lcrypto
CXXFLAGS = -MMD -MD -I$(INC_DIR) -I../ -I../../libiec61850/.install/include
LFLAGS += `pkg-config --libs protobuf`
CXXFLAGS += `pkg-config --cflags protobuf`
CXXFLAGS += `python3-config --cflags --embed`
LFLAGS += `python3-config --ldflags --embed`

CFLAGS = $(CXXFLAGS)

define remove_file
    $(if $(wildcard $(1)), \rm $(1))
endef

all: check_python_version 

run: protobuf $(OBJS) $(target)

check_python_version:
	$(if $(PYTHON), ,$(error "python3 not installed or added to path. Install python3 to continue."))
	if [ ${PY3} -lt 8 ]; then \
		echo "python3 version must be at least 3.8" ;\
		echo "upgrade to python3.8 or above to continue" ;\
	fi;

install:
	$(if $(PROTOC), ,$(shell bash install_protobuf.sh))
	$(if $(PYCONFIG), ,$(shell bash install_python_config.sh)) 
	$(if $(PKGCONFIG), ,$(shell bash install_pkg_config.sh))

protobuf:
	protoc -I ../protobuf --cpp_out ../protobuf ../protobuf/battery.proto ../protobuf/battery_manager.proto 

mac: $(BATTERY_OBJECTS) ../src/BatteryStatus.o ../src/wiringSerial.o
	$(GPP) -dynamiclib -o libbatterydrivers.dylib $^

linux: $(BATTERY_OBJECTS) ../src/BatteryStatus.o ../src/wiringSerial.o
	$(GPP) -shared -o libbatterydrivers.so $^
	
bos: $(OBJS) fifo.o
	$(GPP) -o $@ $^ $(LFLAGS)

socket: $(OBJS) socket.o 
	$(GPP) -o $@ $^ $(LFLAGS)

aggregator: $(OBJS) aggregator.o 
	$(GPP) -o $@ $^ $(LFLAGS)

fifoTest: $(OBJS) testFifo.o
	$(GPP) -o $@ $^ $(LFLAGS)

socketTest: $(OBJS) testSocket.o 
	$(GPP) -o $@ $^ $(LFLAGS)

manager: $(OBJS) testDirectoryManager.o
	$(GPP) -o $@ $^ $(LFLAGS)

aggregate: $(OBJS) testAggregate.o
	$(GPP) -o $@ $^ $(LFLAGS)

partition: $(OBJS) testPartition.o
	$(GPP) -o $@ $^ $(LFLAGS)

pseudo: $(OBJS) testPseudo.o
	$(GPP) -o $@ $^ $(LFLAGS)

dynamic: $(OBJS) testDynamic.o
	$(GPP) -o $@ $^ $(LFLAGS) -L ./ -lbatterydrivers

../src/device_drivers/%.o: ../src/device_drivers/%.cpp
	$(GPP) -std=c++17 -fPIC -c $< -o $@	$(CFLAGS)

../src/BatteryStatus.o: ../src/BatteryStatus.cpp
	$(GPP) -std=c++17 -fPIC -c $< -o $@ $(CFLAGS)	

../src/wiringSerial.o: ../src/wiringSerial.c
	$(GCC) -fPIC -c $< -o $@ $(CFLAGS)

../protobuf/%.o: ../protobuf/%.cc
	$(GPP) -std=c++17 -c $< -o $@ $(CFLAGS)

clean:
	$(call remove_file,*.o)
	$(call remove_file,../src/*.o)
	$(call remove_file,../protobuf/*.o)
	$(call remove_file,../protobuf/*.h)
	$(call remove_file,../protobuf/*.cc)
	$(call remove_file,../src/device_drivers/*.o)

	$(call remove_file,bos)
	$(call remove_file,socket)
	$(call remove_file,pseudo)
	$(call remove_file,manager)
	$(call remove_file,dynamic)
	$(call remove_file,fifoTest)
	$(call remove_file,aggregate)
	$(call remove_file,partition)
	$(call remove_file,socketTest)
	
	$(call remove_file,libbatterydrivers.so)
	$(call remove_file,libbatterydrivers.dylib)

-include *.d
