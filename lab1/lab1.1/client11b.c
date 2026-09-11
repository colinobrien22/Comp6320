/*
** talker.c -- a datagram "client" demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <stdint.h>

#define SERVERPORT "10010"   // the port users will be connecting to

uint64_t htonll(uint64_t value)
{
    uint32_t high = htonl((uint32_t)(value >> 32));
    uint32_t low = htonl((uint32_t)(value & 0xFFFFFFFF));

    return ((uint64_t)low << 32) | high;
}

uint64_t ntohll(uint64_t value)
{
    uint32_t high = ntohl((uint32_t)(value >> 32));
    uint32_t low = ntohl((uint32_t)(value & 0xFFFFFFFF));

    return ((uint64_t)low << 32) | high;
}

int main(int argc, char *argv[])
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int sentbytes;
	int recvbytes;
	char message[1025];
	unsigned char buf[1038];
	struct sockaddr_storage their_addr;
	socklen_t addr_len;
	struct timeval start, end;
	double rtt;
	unsigned char packet[1038];
	uint16_t total_length;
	uint32_t sequence_number;
	uint64_t timestamp_ms;
	uint16_t received_length_net;
	uint32_t received_sequence_net;
	uint64_t received_timestamp_net;
	char received_message[1025];
	

	if (argc != 2) {
		fprintf(stderr,"usage: client11b hostname\n");
		exit(1);
	}

printf("Enter a string: ");

if (fgets(message, sizeof message, stdin) == NULL) {
    fprintf(stderr, "Error reading input\n");
    exit(1);
}

message[strcspn(message, "\n")] = '\0';

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
	hints.ai_socktype = SOCK_DGRAM;

	rv = getaddrinfo(argv[1], SERVERPORT, &hints, &servinfo);
	if (rv != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	

	// loop through all the results and make a socket
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("talker: socket");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client11b: failed to create socket\n");
		return 2;
	}

	sequence_number = 1;

	total_length = 2 + 4 + 8 + strlen(message);

	struct timeval now;
	gettimeofday(&now, NULL);

	timestamp_ms = (uint64_t)now.tv_sec * 1000ULL;
	timestamp_ms += now.tv_usec / 1000;

	uint16_t network_length = htons(total_length);
	uint32_t network_sequence = htonl(sequence_number);
	uint64_t network_timestamp = htonll(timestamp_ms);

	memcpy(packet, &network_length, 2);
	memcpy(packet + 2, &network_sequence, 4);
	memcpy(packet + 6, &network_timestamp, 8);
	memcpy(packet + 14, message, strlen(message));

	gettimeofday(&start, NULL);

	if ((sentbytes = sendto(sockfd, packet, total_length, 0,
			 p->ai_addr, p->ai_addrlen)) == -1) {
		perror("client11b: sendto");
		exit(1);
	}

	addr_len = sizeof their_addr;

	if ((recvbytes = recvfrom(sockfd, buf, sizeof(buf), 0,
    		(struct sockaddr *)&their_addr, &addr_len)) == -1) {
    	perror("client11b: recvfrom");
    	exit(1);
	
	}

	gettimeofday(&end, NULL);

	memcpy(&received_length_net, buf, 2);
	memcpy(&received_sequence_net, buf + 2, 4);
	memcpy(&received_timestamp_net, buf + 6, 8);

	uint16_t received_length = ntohs(received_length_net);
	uint32_t received_sequence = ntohl(received_sequence_net);
	uint64_t received_timestamp = ntohll(received_timestamp_net);

	if (received_length < 14 || received_length > 1038) {
    fprintf(stderr, "Invalid received message length\n");
    exit(1);
	}

	if (received_length > recvbytes) {
    fprintf(stderr, "Incomplete packet received\n");
    exit(1);
	}

	size_t message_length = received_length - 14;

	memcpy(received_message, buf + 14, message_length);
	received_message[message_length] = '\0';

	freeaddrinfo(servinfo);

	rtt = (end.tv_sec - start.tv_sec) * 1000.0;
	rtt += (end.tv_usec - start.tv_usec) / 1000.0;

	printf("Round trip time: %.3f ms\n", rtt);
	printf("Received echo: %s\n", received_message);
	printf("client11b: sent %d bytes to %s\n", sentbytes, argv[1]);
	close(sockfd);

	return 0;
}