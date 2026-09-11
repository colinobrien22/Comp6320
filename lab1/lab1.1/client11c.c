/*
** client11c.c -- UDP PING test client
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <stdint.h>


#define SERVERPORT "10010"
#define MAX_PACKET_SIZE 1038
#define NUM_PACKETS 10000
#define HEADER_SIZE 14



uint64_t htonll(uint64_t value)
{
    static const int one = 1;


    if (*(const char *)&one == 1) {
        uint32_t high = htonl((uint32_t)(value >> 32));
        uint32_t low = htonl((uint32_t)(value & 0xFFFFFFFF));

        return ((uint64_t)low << 32) | high;
    }


    return value;
}


uint64_t ntohll(uint64_t value)
{
    static const int one = 1;

    if (*(const char *)&one == 1) {
        uint32_t high = ntohl((uint32_t)(value >> 32));
        uint32_t low = ntohl((uint32_t)(value & 0xFFFFFFFF));

        return ((uint64_t)low << 32) | high;
    }

    return value;
}


int main(int argc, char *argv[])
{
    int sockfd;
    struct addrinfo hints;
    struct addrinfo *servinfo;
    struct addrinfo *p;

    int rv;
    int sentbytes;
    int recvbytes;

    char message[1025];

    unsigned char packet[MAX_PACKET_SIZE];
    unsigned char buf[MAX_PACKET_SIZE];

    struct sockaddr_storage their_addr;
    socklen_t addr_len;

    uint16_t total_length;
    uint32_t sequence_number;
    uint64_t timestamp_ms;

    uint16_t received_length_net;
    uint32_t received_sequence_net;
    uint64_t received_timestamp_net;

    pid_t pid;


    int received[NUM_PACKETS + 1] = {0};

    int received_count = 0;

    double min_rtt = 0.0;
    double max_rtt = 0.0;
    double total_rtt = 0.0;



    if (argc != 2) {
        fprintf(stderr, "usage: client11c hostname\n");
        exit(1);
    }


    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;



    rv = getaddrinfo(argv[1], SERVERPORT, &hints, &servinfo);

    if (rv != 0) {
        fprintf(stderr,
                "getaddrinfo: %s\n",
                gai_strerror(rv));

        return 1;
    }


    for (p = servinfo; p != NULL; p = p->ai_next) {

        sockfd = socket(p->ai_family,
                        p->ai_socktype,
                        p->ai_protocol);

        if (sockfd == -1) {
            perror("client11c: socket");
            continue;
        }

        break;
    }


    if (p == NULL) {
        fprintf(stderr,
                "client11c: failed to create socket\n");

        freeaddrinfo(servinfo);
        return 2;
    }


    pid = fork();

    if (pid < 0) {
        perror("client11c: fork");

        freeaddrinfo(servinfo);
        close(sockfd);

        exit(1);
    }


    if (pid == 0) {


        struct timeval timeout;

        timeout.tv_sec = 2;
        timeout.tv_usec = 0;

        if (setsockopt(sockfd,
                       SOL_SOCKET,
                       SO_RCVTIMEO,
                       &timeout,
                       sizeof timeout) == -1) {

            perror("client11c: setsockopt");

            freeaddrinfo(servinfo);
            close(sockfd);

            exit(1);
        }


        while (1) {

            addr_len = sizeof their_addr;

 
            recvbytes = recvfrom(
                sockfd,
                buf,
                sizeof(buf),
                0,
                (struct sockaddr *)&their_addr,
                &addr_len
            );


      
            if (recvbytes == -1) {

                if (errno == EAGAIN ||
                    errno == EWOULDBLOCK) {

                    break;
                }

                perror("client11c: recvfrom");

                freeaddrinfo(servinfo);
                close(sockfd);

                exit(1);
            }


            if (recvbytes < HEADER_SIZE) {
                continue;
            }


            memcpy(&received_length_net,
                   buf,
                   2);

            memcpy(&received_sequence_net,
                   buf + 2,
                   4);

            memcpy(&received_timestamp_net,
                   buf + 6,
                   8);

            uint16_t received_length =
                ntohs(received_length_net);

            uint32_t received_sequence =
                ntohl(received_sequence_net);

            uint64_t received_timestamp =
                ntohll(received_timestamp_net);


            if (received_length < HEADER_SIZE ||
                received_length > MAX_PACKET_SIZE ||
                received_length > recvbytes) {

                continue;
            }


            struct timeval receive_time;

            gettimeofday(&receive_time, NULL);

            uint64_t receive_timestamp_ms;

            receive_timestamp_ms =
                (uint64_t)receive_time.tv_sec * 1000ULL;

            receive_timestamp_ms +=
                receive_time.tv_usec / 1000;

            if (receive_timestamp_ms <
                received_timestamp) {

                continue;
            }


            double packet_rtt =
                (double)(receive_timestamp_ms -
                         received_timestamp);


            if (received_sequence >= 1 &&
                received_sequence <= NUM_PACKETS) {

                if (received[received_sequence] == 0) {

                    received[received_sequence] = 1;

                    received_count++;

                    total_rtt += packet_rtt;


                    if (received_count == 1) {

                        min_rtt = packet_rtt;
                        max_rtt = packet_rtt;
                    }

                    else {

                        if (packet_rtt < min_rtt) {
                            min_rtt = packet_rtt;
                        }

                        if (packet_rtt > max_rtt) {
                            max_rtt = packet_rtt;
                        }
                    }
                }
            }


            if (received_count == NUM_PACKETS) {
                break;
            }
        }


        printf("\n--- UDP Echo Test Summary ---\n");

        printf("Packets sent:     %d\n",
               NUM_PACKETS);

        printf("Packets received: %d\n",
               received_count);


        int missing_count = 0;

        for (int i = 1;
             i <= NUM_PACKETS;
             i++) {

            if (received[i] == 0) {

                printf("Missing echo: %d\n", i);

                missing_count++;
            }
        }


        if (missing_count == 0) {

            printf("No echo responses were missing.\n");
        }

        else {

            printf("Missing echo responses: %d\n",
                   missing_count);
        }


        if (received_count > 0) {

            double average_rtt =
                total_rtt / received_count;

            printf("Minimum RTT: %.3f ms\n",
                   min_rtt);

            printf("Maximum RTT: %.3f ms\n",
                   max_rtt);

            printf("Average RTT: %.3f ms\n",
                   average_rtt);
        }

        else {

            printf("Minimum RTT: N/A\n");
            printf("Maximum RTT: N/A\n");
            printf("Average RTT: N/A\n");
        }


        freeaddrinfo(servinfo);
        close(sockfd);

        return 0;
    }


    else {

        for (sequence_number = 1;
             sequence_number <= NUM_PACKETS;
             sequence_number++) {


            snprintf(message,
                     sizeof(message),
                     "%u",
                     sequence_number);


            total_length =
                HEADER_SIZE + strlen(message);


          
            struct timeval now;

            gettimeofday(&now, NULL);

            timestamp_ms =
                (uint64_t)now.tv_sec * 1000ULL;

            timestamp_ms +=
                now.tv_usec / 1000;

            uint16_t network_length =
                htons(total_length);

            uint32_t network_sequence =
                htonl(sequence_number);

            uint64_t network_timestamp =
                htonll(timestamp_ms);


            memcpy(packet,
                   &network_length,
                   2);

            memcpy(packet + 2,
                   &network_sequence,
                   4);

            memcpy(packet + 6,
                   &network_timestamp,
                   8);

            memcpy(packet + HEADER_SIZE,
                   message,
                   strlen(message));


            sentbytes = sendto(
                sockfd,
                packet,
                total_length,
                0,
                p->ai_addr,
                p->ai_addrlen
            );


            if (sentbytes == -1) {

                perror("client11c: sendto");

                freeaddrinfo(servinfo);
                close(sockfd);

                exit(1);
            }
        }


        if (waitpid(pid, NULL, 0) == -1) {
            perror("client11c: waitpid");
        }
    }


    freeaddrinfo(servinfo);

    close(sockfd);

    return 0;
}