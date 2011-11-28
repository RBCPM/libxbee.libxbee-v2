all: main

run: main
	LD_LIBRARY_PATH=../../lib ./$^

main: main.c ../../lib/libxbee.so
	gcc $(filter %.c,$^) --static -I../../ -L../../lib -lxbee -lpthread -lrt -o $@
