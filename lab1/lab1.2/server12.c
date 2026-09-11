/*
** server12.c -- TCP calculator server
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdint.h>

#define PORT "10020"  // the port users will be connecting to

#define BACKLOG 10   // how many pending connections queue will hold

void sigchld_handler(int s)
{
	(void)s; // quiet unused variable warning

	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int recv_all(int sockfd, void *buffer, int length)
{
    int total = 0;
    int bytes_received;

    while (total < length) {
        bytes_received = recv(
            sockfd,
            (char *)buffer + total,
            length - total,
            0
        );

        if (bytes_received <= 0) {
            return bytes_received;
        }

        total += bytes_received;
    }

    return total;
}


int send_all(int sockfd, const void *buffer, int length)
{
    int total = 0;
    int bytes_sent;

    while (total < length) {
        bytes_sent = send(
            sockfd,
            (const char *)buffer + total,
            length - total,
            0
        );

        if (bytes_sent <= 0) {
            return bytes_sent;
        }

        total += bytes_sent;
    }

    return total;
}

int main(void)
{
	// listen on sock_fd, new connection on new_fd
	int sockfd, new_fd;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address info
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server12: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server12: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr,
				&sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

        if (!fork()) { // this is the child process

            close(sockfd); // child doesn't need the listener

            unsigned char request[9];
            unsigned char response[14];

            uint8_t operation;

            uint32_t operand_a_net;
            uint32_t operand_b_net;

            uint32_t operand_a;
            uint32_t operand_b;

            uint32_t answer = 0;
            uint8_t validity = 1;

            int bytes_received = recv_all(new_fd, request, 9);

            if (bytes_received != 9) {
                fprintf(stderr,
                        "server12: failed to receive complete request\n");
                close(new_fd);
                exit(1);
            }

            operation = request[0];

            memcpy(&operand_a_net, request + 1, 4);
            memcpy(&operand_b_net, request + 5, 4);

            operand_a = ntohl(operand_a_net);
            operand_b = ntohl(operand_b_net);

            switch (operation) {

                case '+':
                    answer = operand_a + operand_b;
                    break;

                case '-':
                    answer = operand_a - operand_b;
                    break;

                case 'x':
                    answer = operand_a * operand_b;
                    break;

                case '/':
                    if (operand_b == 0) {
                        answer = 0;
                        validity = 2;
                    }
                    else {
                        answer = operand_a / operand_b;
                    }
                    break;

                default:
                    answer = 0;
                    validity = 2;
                    break;
            }

            uint32_t answer_net = htonl(answer);

            response[0] = operation;

            memcpy(response + 1, &operand_a_net, 4);
            memcpy(response + 5, &operand_b_net, 4);
            memcpy(response + 9, &answer_net, 4);

            response[13] = validity;

            if (send_all(new_fd, response, 14) != 14) {
                perror("server12: send");
            }

            close(new_fd);
            exit(0);
        }

        close(new_fd);

    } // end while(1)

    return 0;
}
