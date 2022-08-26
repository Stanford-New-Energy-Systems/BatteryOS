GPP = g++

all: bos

INC_DIR = ../src/
CPP_FILES = $(wildcard ../src/*.cpp)

OBJS = $(patsubst %.cpp,%.o,$(CPP_FILES))
OBJS += ../protobuf/battery.pb.o
OBJS += ../protobuf/battery_manager.pb.o

CFLAGS = -std=c++2a -g -I$(INC_DIR) -I../
LFLAGS = -g -pthread -lprotobuf

define remove_file
    $(if $(wildcard $(1)), \rm $(1))
endef

bos: $(OBJS) fifo.o
	$(GPP) $(LFLAGS) -o $@ $^

socket: $(OBJS) socket.o 
	$(GPP) $(LFLAGS) -o $@ $^

fifoTest: $(OBJS) testFifo.o
	$(GPP) $(LFLAGS) -o $@ $^

socketTest: $(OBJS) testSocket.o 
	$(GPP) $(LFLAGS) -o $@ $^

%.o: %.cpp
	$(GPP) $(CFLAGS) -c $< -o $@

../protobuf/%.o: ../protobuf/%.cc
	$(GPP) $(CFLAGS) -c $< -o $@

clean:
	$(call remove_file,*.o)
	$(call remove_file,../src/*.o)
	$(call remove_file,../protobuf/*.o)

	$(call remove_file,bos)
	$(call remove_file,socket)
	$(call remove_file,fifoTest)
	$(call remove_file,socketTest)
