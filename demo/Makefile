CC ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -pedantic

TARGET = gateway_redundancy_demo
SRCS = main.c state_machine.c transport_sim.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)
