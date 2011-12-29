#include <stdio.h>
#include <stdlib.h>
#include <xbee.h>

int main(int argc, char *argv[]) {
	printf("Libxbee revision: %s\n", libxbee_revision);
	printf("Libxbee commit: %s\n", libxbee_commit);
	return 0;
}
