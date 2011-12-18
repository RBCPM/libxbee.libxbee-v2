#include <stdio.h>
#include <stdlib.h>
#include <xbee.h>

int main(int argc, char *argv[]) {
	int i;
	char **modes;

	modes = xbee_modeGetList();

	for (i = 0; modes[i]; i++) {
		printf("modes[%d] = '%s'\n",i, modes[i]);
	}

	free(modes);
}
