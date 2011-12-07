all: main

run: main
	LD_LIBRARY_PATH=../../../lib ./$^

main: main.c ../../../lib/libxbee.so
	gcc -g $(filter %.c,$^) -I../../../ -L../../../lib -lxbee -o $@
