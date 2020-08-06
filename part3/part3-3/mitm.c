/*
Jiangchen Zhu
zhujiangchen@sjtu.edu.cn

A man-in-the-middle (MITM) that can trigger events like packet loss, delay, corruption, and reordering.
Used to test the 'selective repeat' sender/receiver for part 3 in VE489 project.

Assumptions for this testing:
The sender is at 10.0.0.1:8001
The receiver is at 10.0.0.2:8002
The MITM is at 10.0.0.3:8003

Usage:
Receiver works as usual.
Sender transfers file to MITM at 10.0.0.3:8003, instead of receiver at 10.0.0.1:8001.
The MITM will forward every packet from 10.0.0.1:8001 to 10.0.0.2:8002,
and forward any pacekt coming from 10.0.0.2:8002 to 10.0.0.1:8001

Note:
Since this is the first time that the project is launched, if you spot any bugs, please let me know.

*/

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "./crc32.h"
#include "./SRHeader.h"


struct Packet {
    struct SRHeader ph;
    char payload[MAX_PAYLOAD_SIZE];
};



int create_socket(struct sockaddr_in *si_me, int port_num) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == -1) {
        fputs("Sender: create socket failed!", stderr);
        exit(2);
    }
    memset((char *) si_me, 0, sizeof(struct sockaddr_in));
    si_me->sin_family = AF_INET;
    si_me->sin_port = htons(port_num);
    si_me->sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(sockfd , (struct sockaddr*) si_me, sizeof(struct sockaddr_in) ) == -1) {
        fputs("Sender: bind socket failed!", stderr);
        exit(3);
    }
    return sockfd;
}


// arguments for function handle_packet
struct Targ {
    int sockfd;
    struct sockaddr_in* si_other;
    struct Packet* buf;
    int packet_len;  // length of header plus data (if any)
    int delay;
    int loss;
    int error;
    char *receiver_ip;
    int receiver_port;
    char *sender_ip;
    int sender_port;
};


/* decide if an event should happen with the given probability, prob is a percentage 0-100 */
int event_happen(int prob) {
    int n = rand() % 100;
    if (n < prob) {
        return 1;
    }
    return 0;
}

void *handle_packet(void *arg) {
    struct Targ* args = (struct Targ*) arg;
    struct Packet* buf = args->buf;

    // check crc 51 381467760 1456
    if (crc32(buf->payload, buf->ph.len) != buf->ph.crc) {
        printf("Packet errors\n");
    }

    if (args->packet_len < 0) {
        return NULL;
    }

    // printf("drop rate %d\n", args->loss);
    // if we want to drop the packet, then do not forward the packet.
    if (event_happen(args->loss)) {
        printf("Drop packet\n");
        return NULL;
    }

    // if we want to delay the packet to cause re-ordering, then sleep a while.
    int d = args->delay;
    if (d > 10) {
        d = 10;
    }
    if (event_happen(d)) {
        printf("Dealy packet\n");
        usleep(20000); // sleep 20 milliseconds, which is 20000 microseconds.
    }

    // if we want to insert errors into packet
    if (event_happen(args->error)) {
        // check if this is a DATA packet
        // we only want to add errors into data.
        if (buf->ph.flag == DATA) {
            printf("Make error in packet\n");

            // just simply change the first byte in payload, but not re-compute crc
            // this should cause receiver to drop the packet.
            buf->payload[0] += 1;
        }
    }

    // printf("%d\n", args->delay);
    // printf("%d\n", args->buf->ph.flag);
    // printf("%d\n", args->si_other);
    // (struct sockaddr_in*) si_other = args->si_other;
    char *ip = inet_ntoa(args->si_other->sin_addr);
    int port = htons(args->si_other->sin_port);

    // printf("Receive a packet from %s:%d\n", ip, port);

    char *receiver_ip = args->receiver_ip;
    char *sender_ip = args->sender_ip;
    int receiver_port = args->receiver_port;
    int sender_port = args->sender_port;


    // simple forward here...
    // if it's from 10.0.0.1:8001, forward to 10.0.0.2:8002
    // if it's from 10.0.0.2:8002, forward to 10.0.0.1:8001

    // determine if this packet is from sender or receiver
    if (port == sender_port && strcmp(ip, sender_ip) == 0) {
        // forward to receiver
        struct sockaddr_in address;  // create receiver's address
        struct hostent *host = gethostbyname(receiver_ip);
        memcpy(&(address.sin_addr), host->h_addr, host->h_length);
        address.sin_family = AF_INET;
        address.sin_port = htons(receiver_port);

        sendto(args->sockfd, (void*) args->buf, buf->ph.len + sizeof(struct SRHeader), 0, (const struct sockaddr *) &address, sizeof(struct sockaddr_in));
    } else if (port == receiver_port && strcmp(ip, receiver_ip) == 0) {
        // forward to sender
        struct sockaddr_in address;  // create sender's address
        struct hostent *host = gethostbyname(sender_ip);
        memcpy(&(address.sin_addr), host->h_addr, host->h_length);
        address.sin_family = AF_INET;
        address.sin_port = htons(sender_port);

        sendto(args->sockfd, (void*) args->buf, buf->ph.len + sizeof(struct SRHeader), 0, (const struct sockaddr *) &address, sizeof(struct sockaddr_in));
    } else {
        printf("MITM does not know who sent this packet!\n");
    }

    free(args->si_other);
    free(args->buf);
    free(args);

    return NULL;
}


// add flag -pthread when compiling
int main(int argc, char **argv) {

    // accept command line arguments
    if (argc != 8) {
        printf("Usage: ./mitm <shuffle> <loss> <error> <receiver's ip> <receiver's port> <sender's ip> <sender's port>\nExample: ./mitm 1 10 0 10.0.0.2 8002 10.0.0.1 8001\n");
        exit(1);
    }

    int delay = atoi(argv[1]);  // bool, whether mitm delay/shuffle packets
    int loss = atoi(argv[2]);  // prob. of dropping any packet
    int error = atoi(argv[3]);  // prob. of inserting an error to a data packet.

    char *receiver_ip = argv[4];
    int receiver_port = atoi(argv[5]);
    char *sender_ip = argv[6];
    int sender_port = atoi(argv[7]);


    struct sockaddr_in si_me;
    int port_num = 8003;  // mitm listens on port 8003


    int sockfd = create_socket(&si_me, port_num);

    srand(time(0));

    while(1) {
        struct sockaddr_in* si_other = malloc(sizeof(struct sockaddr_in));
        socklen_t slen = sizeof(struct sockaddr_in);
        struct Packet *buf = malloc(sizeof(struct Packet));
        int n = recvfrom(sockfd, buf, sizeof(struct Packet), 0, (struct sockaddr *) si_other, &slen);

        // cannot pass local variable to the thread, it may be freed in next loop
        // malloc some memory for args, free it inside the thread.
        struct Targ *args = malloc(sizeof(struct Targ));
        args->sockfd = sockfd;
        args->si_other = si_other;
        args->buf = buf;
        args->packet_len = n;
        args->delay = delay;
        args->loss = loss;
        args->error = error;
        args->receiver_ip = receiver_ip;
        args->receiver_port = receiver_port;
        args->sender_ip = sender_ip;
        args->sender_port = sender_port;

        // spawn a thread to handle this packet.
        // having multi threads causes random ordering of packets
        // for in-order but error packets, do not use multi-threading
        if (delay > 0) {
            pthread_t tid;
            pthread_create(&tid, NULL, handle_packet, (void*) args);
            pthread_detach(tid);
        } else {
            // still preserve the order of packets, without multi-threading
            handle_packet(args);
        }
    }

    return 0;
}
