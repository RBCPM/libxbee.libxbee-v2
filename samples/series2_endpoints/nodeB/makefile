all: main

run: main
	LD_LIBRARY_PATH=../../../lib ./$^

main: main.c ../../../lib/libxbee.so
	gcc $(filter %.c,$^) -I../../../ -L../../../lib -lxbee -o $@
