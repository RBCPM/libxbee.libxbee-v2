all: main

run: main
	./$^

main: main.c /usr/lib/libxbee.so /usr/include/xbee.h
	gcc $(filter %.c,$^) -g -lxbee -lpthread -lrt -ldl -o $@
