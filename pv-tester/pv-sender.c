#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char const* argv[])
{
	int sock = 0, valread, client_fd;
	struct sockaddr_in serv_addr;
	char buffer[1024] = { 0 };
	if (argc < 4) {
		printf("Usage ./pv-server <IP> <PORT> <MSG>\n");
		return 0;
	}

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("\n Socket creation error \n");
		return -1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[2]));

	// Convert IPv4 and IPv6 addresses from text to binary
	// form
	if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr)
		<= 0) {
		printf(
			   "\nInvalid address/ Address not supported \n");
		return -1;
	}
	printf("addr = %ld\n", serv_addr.sin_addr.s_addr);
	if ((client_fd
		 = connect(sock, (struct sockaddr*)&serv_addr,
				   sizeof(serv_addr)))
		< 0) {
		printf("\nConnection Failed \n");
		return -1;
	}
	
	send(sock, argv[3], strlen(argv[3]), 0);
	printf("Message sent %s\n", argv[3]);
//	valread = read(sock, buffer, 1024);
//	printf("Response: %s\n", buffer);

	// closing the connected socket
	close(client_fd);
	return 0;
}
