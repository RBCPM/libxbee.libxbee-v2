#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

int connectMe(char *host, int port) {
	char rport[7];
	struct addrinfo *rinfo;
	struct addrinfo hints;
	int sock;
	int ret;

	ret = -1;

	snprintf(rport, sizeof(rport), "%d", port);

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if ((ret = getaddrinfo(host, rport, &hints, &rinfo)) != 0) {
		fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(ret));
		goto die1;
	}

	if ((sock = socket(rinfo->ai_family, rinfo->ai_socktype, rinfo->ai_protocol)) == -1) {
		perror("socket()");
		goto die2;
	} else if (sock < 0) {
		fprintf(stderr, "socket(): returned invalid file handle\n");
		goto die2;
	}

	if (connect(sock, rinfo->ai_addr, rinfo->ai_addrlen) == -1) {
		perror("connect()");
		goto die2;
	}

	ret = sock;

die2:
	freeaddrinfo(rinfo);
die1:
done:
	return ret;
}

int main(int argc, char *argv[]) {
	char *host;
	int port;
	char *file;

	int c;
	int i;
	int ret;

	int fd;
	unsigned char fbuf[1024];

	int sock;

	if (argc < 4) {
		printf("usage: %s <host> <port> <file>...<fileN>\n", argv[0]);
		return 1;
	}

	host = argv[1];
	port = atoi(argv[2]);

	if (port <= 0 || port >= 65535) {
		printf("please specify a port in the range 1-65535\n");
		return 1;
	}

	printf("Connecting to %s:%d, and sending %d file(s)\n", host, port, argc - 3);

	if ((sock = connectMe(host, port)) < 0) {
		printf("failed to connect\n");
		return 1;
	}

	for (i = 3; i < argc; i++) {
		file = argv[i];

		if (access(file, R_OK)) {
			printf("can't open '%s' for reading\n", file);
			return 1;
		}

		if ((fd = open(file, O_RDONLY)) < 0) {
			printf("failed to open file\n");
			return 1;
		}

		while ((c = read(fd, fbuf, sizeof(fbuf))) > 0) {
			if (send(sock, fbuf, c, 0) != c) {
				printf("sent wrong number of bytes!\n");
				return 1;
			}
		}

		close(fd);

		sleep(1);
	}

	c = 0;
	do {
		if ((ret = recv(sock, fbuf, sizeof(fbuf), 0)) > 0) {
			int i;
			for (i = 0; i < ret; i++, c++) {
				printf("%3d:%3d  0x%02X '%c'\n", c, i, fbuf[i], ((fbuf[i] >= ' ' && fbuf[i] <= '~')?fbuf[i]:'.'));
			}
		}
	} while (ret > 0);
	sleep(5);

	shutdown(sock, SHUT_RDWR);
	close(sock);

	return 0;
}
