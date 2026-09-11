/*
** listener.c -- a datagram sockets "server" demo
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

#define MYPORT "10010"
#define MAXBUFLEN 1038

int main(void)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;

    struct sockaddr_storage their_addr;
    unsigned char buf[MAXBUFLEN];
    socklen_t addr_len;

    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo);

    if (rv != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {

        sockfd = socket(p->ai_family,
                        p->ai_socktype,
                        p->ai_protocol);

        if (sockfd == -1) {
            perror("server11: socket");
            continue;
        }

        if (bind(sockfd,
                 p->ai_addr,
                 p->ai_addrlen) == -1) {

            close(sockfd);
            perror("server11: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "server11: failed to bind socket\n");
        freeaddrinfo(servinfo);
        return 2;
    }

    freeaddrinfo(servinfo);

    printf("server11: waiting for UDP packets on port %s...\n",
           MYPORT);

    while (1) {

        addr_len = sizeof their_addr;

        numbytes = recvfrom(
            sockfd,
            buf,
            sizeof(buf),
            0,
            (struct sockaddr *)&their_addr,
            &addr_len
        );

        if (numbytes == -1) {
            perror("server11: recvfrom");
            continue;
        }

        if (sendto(
                sockfd,
                buf,
                numbytes,
                0,
                (struct sockaddr *)&their_addr,
                addr_len) == -1) {

            perror("server11: sendto");
        }
    }

    close(sockfd);

    return 0;
}
