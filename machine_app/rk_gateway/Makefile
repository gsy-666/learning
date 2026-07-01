CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c11

TARGETS = gm6220_tcp_gateway sensor_daemon
SRCS = gm6220_tcp_gateway.c sht31-d.c ../liangcang/GM6220/dm6220_linux.c \
       ../liangcang/project/HX711_scale/HX711_scale.c
SENSOR_SRCS = sensor_daemon.c sht31-d.c

CFLAGS += -D_DEFAULT_SOURCE
CFLAGS += -I../liangcang/GM6220 -I../liangcang/project/HX711_scale

all: $(TARGETS)

gm6220_tcp_gateway: $(SRCS)
	$(CC) $(CFLAGS) -pthread -o $@ $(SRCS) -lm

sensor_daemon: $(SENSOR_SRCS)
	$(CC) $(CFLAGS) -pthread -o $@ $(SENSOR_SRCS)

clean:
	rm -f $(TARGETS)
