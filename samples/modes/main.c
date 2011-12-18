#include <stdio.h>
#include <stdlib.h>
#include <xbee.h>

int main(int argc, char *argv[]) {
	int i;
	char **modes;
	char **types;
	struct xbee *xbee;

	modes = xbee_modeGetList();
	for (i = 0; modes[i]; i++) {
		printf("modes[%d] = '%s'\n",i, modes[i]);
	}
	free(modes);

	if (xbee_setup("/dev/ttyS0", 57600, &xbee)) {
		printf("xbee_setup(): failed\n");
		return 1;
	}

	if (xbee_modeSet(xbee, "series1")) {
		printf("xbee_modeSet(): failed\n");
		return 1;
	}

	if (xbee_conGetTypeList(xbee, &types)) {
		printf("xbee_conGetTypeList(): failed\n");
		return 1;
	}
	for (i = 0; types[i]; i++) {
		printf("type[%d] = '%s'\n",i, types[i]);
	}
	free(types);

	return 0;
}
