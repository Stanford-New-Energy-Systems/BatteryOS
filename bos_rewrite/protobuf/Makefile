GPP = g++

all: main

OBJS = battery_manager.pb.o battery.pb.o main.o

CFLAGS = `pkg-config --cflags --libs protobuf`
LFLAGS = `pkg-config --libs protobuf`

main: $(OBJS)
	$(GPP) -o $@ $^ $(LFLAGS)

%.o: %.cc
	$(GPP) -std=c++14 -c $< -o $@ $(CFLAGS)

main.o: main.cpp
	$(GPP) -std=c++14 -c $< -o $@ $(CFLAGS)

clean:
	\rm *.o
	\rm main
