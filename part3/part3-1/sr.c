// part3-1
#include <stdint.h>
#include "./crc32.h"
#include "./SRHeader.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define MAX_PACKET_SIZE (sizeof(struct SRHeader) + MAX_PAYLOAD_SIZE)
const char* flag_str[] = {"SYN", "FIN", "DATA", "ACK"};
struct buf {
    unsigned int seq;    // seq of the packet in it
    char* packet;
};
/* Adding an unsigned int as a flag of checking whether or not this buf is occupied
 * instead of checking window of zero memory?
 */

int input_log(const char* log_file, struct SRHeader header){
	// function to input header information to the required log_file
    FILE *fp_log = fopen(log_file, "a+");
    if (fp_log == NULL){
        return -1;
    }

    fputs(flag_str[header.flag], fp_log);
    fputs(" ", fp_log);

    char *str = malloc(sizeof(int) + 1);

    memset(str, 0 , sizeof(int) + 1);
    sprintf(str, "%d", header.seq);
    fputs(str, fp_log);
    fputs(" ", fp_log);

    memset(str, 0 , sizeof(int) + 1);
    sprintf(str, "%d", header.len);
    fputs(str, fp_log);
    fputs(" ", fp_log);

    memset(str, 0 , sizeof(int) + 1);
    sprintf(str, "%d", header.crc);
    fputs(str, fp_log);
    fputs("\n", fp_log);

    free(str);
    fclose(fp_log);
    return 0;
}

int sender(const char *receiver_ip, int receiver_port, int myport, int window_size, const char *filename, const char *log_file) {
    // create UDP socket.
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);  // use IPPROTO_UDP
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(1);
    }

    // below is the same as TCP socket.
    struct sockaddr_in myaddr;
    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = INADDR_ANY;
    // bind to a specific port
    myaddr.sin_port = htons(myport);
    bind(sockfd, (struct sockaddr *) & myaddr, sizeof(myaddr));

    struct sockaddr_in receiver_addr;
    struct hostent *host = gethostbyname(receiver_ip);
    memcpy(&(receiver_addr.sin_addr), host->h_addr, host->h_length);
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(receiver_port);

    struct buf window[window_size];   // create sender window
    for(int i = 0; i < window_size; i++) {
        window[i].seq = i;
        window[i].packet = malloc(MAX_PACKET_SIZE);
        memset(window[i].packet, 0, MAX_PACKET_SIZE);
    }

    struct timeval start, stop;
    socklen_t slen = sizeof(struct sockaddr_in);

    // deal with data from file
    FILE *fp = fopen(filename, "rb"); // to read files like jpg
    if (fp == NULL) {
        printf("Open file_to_send failed!\n");
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    unsigned int length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char** chunks = malloc(sizeof(char*)*(length/MAX_PAYLOAD_SIZE+1));
    for (int i = 0; i <= length/MAX_PAYLOAD_SIZE; i++) {
        chunks[i] = (char*)malloc(MAX_PACKET_SIZE);
        memset(chunks[i], 0, MAX_PACKET_SIZE);
    }

    unsigned int seq = 0;
    char* payload = malloc(MAX_PAYLOAD_SIZE);

	// read data from file in chunks
    for (int i = 0; i <= length/MAX_PAYLOAD_SIZE; i++) {
        unsigned int len = MAX_PAYLOAD_SIZE;
        if (i == length/MAX_PAYLOAD_SIZE)
            len = length - i*MAX_PAYLOAD_SIZE;
        memset(payload, 0, MAX_PAYLOAD_SIZE);
        fread(payload, len, 1, fp);
        struct SRHeader header = {2, seq++, len, crc32(payload, len)};
        memcpy(chunks[i], &header, sizeof(struct SRHeader));
        memcpy(chunks[i] + sizeof(struct SRHeader), payload, len);
    }
    free(payload);
    fclose(fp);

    // SYN
    srand(time(NULL));
    int random_seq = rand() + (length/MAX_PAYLOAD_SIZE + 2);  // ensure the seq would not collide with the seq in the transmitting process
    struct SRHeader syn = {0, random_seq, 0, 0};
    sendto(sockfd, (const struct SRHeader *) &syn, sizeof(struct SRHeader), 0, (const struct sockaddr *) &receiver_addr,
           sizeof(struct sockaddr_in));
    if (input_log(log_file, syn) == -1) {
        printf("Open send_logfile failed!\n");
        return -1;
    }

    gettimeofday(&start, NULL);
    gettimeofday(&stop, NULL);
    while(1) {
        struct SRHeader recv_syn;
        int n = recvfrom(sockfd, (struct SRHeader *) &recv_syn, sizeof(struct SRHeader), MSG_DONTWAIT, (struct sockaddr *) &receiver_addr, &slen);
        if (n > 0) {
            if (input_log(log_file, recv_syn) == -1) {
                printf("Open send_logfile failed!\n");
                return -1;
            }
            if (recv_syn.flag == 3 && recv_syn.seq == syn.seq) { // receive the ACK sent by receiver for SYN 
                break;
            }
        }
        gettimeofday(&stop, NULL);
        if ((double) (stop.tv_sec - start.tv_sec) * 1000 + (double) (stop.tv_usec - start.tv_usec) / 1000 > 400) {
            // printf("Send SYN again.\n" );
            sendto(sockfd, (const struct SRHeader *) &syn, sizeof(struct SRHeader), 0, (const struct sockaddr *) &receiver_addr,
                   sizeof(struct sockaddr_in));
            if (input_log(log_file, syn) == -1) {
                printf("Open send_logfile failed!\n");
                return -1;
            }
            gettimeofday(&start, NULL);
            gettimeofday(&stop, NULL);
        }
    }

    // DATA
    // initialize the window
    for (int i = 0; i <= length/MAX_PAYLOAD_SIZE && i < window_size; i++){
        struct SRHeader header;
        memcpy(&header, chunks[i], sizeof(struct SRHeader));
        sendto(sockfd, chunks[i], MAX_PACKET_SIZE, 0, (const struct sockaddr *) &receiver_addr, sizeof(struct sockaddr_in));
        if (input_log(log_file, header) == -1) {
            printf("Open send_logfile failed!\n");
            return -1;
        }
        window[i].seq = header.seq;
        memcpy(window[i].packet, chunks[i], MAX_PACKET_SIZE);
    }

    // advance the window
    gettimeofday(&start, NULL);
    gettimeofday(&stop, NULL);
    while(1){
        struct SRHeader ack;
        int n = recvfrom(sockfd, (struct SRHeader *) &ack, sizeof(struct SRHeader), MSG_DONTWAIT, (struct sockaddr *) &receiver_addr, &slen);
        if (n > 0) {
            if (input_log(log_file, ack) == -1) {
                printf("Open send_logfile failed!\n");
                return -1;
            }
            if (ack.seq == (length / MAX_PAYLOAD_SIZE+1))
                break;
            if (ack.seq <= window[0].seq)
                continue;
            if (ack.seq <= (window[0].seq + window_size)){
                int diff = (ack.seq - window[0].seq);
                for (int i = 0; i < diff; i++){
                    window[i].seq += diff;
                    memset(window[i].packet, 0, MAX_PACKET_SIZE);
                }
                for (int i = diff; i < window_size; i++){
                    window[i].seq += diff;
                    memcpy(window[i-diff].packet, window[i].packet, MAX_PACKET_SIZE);
                }
                for (int i = window_size-diff; i < window_size; i++){
                    if (window[i].seq <= length/MAX_PAYLOAD_SIZE) {
                        struct SRHeader header;
                        memcpy(&header, chunks[window[i].seq], sizeof(struct SRHeader));
                        sendto(sockfd, chunks[window[i].seq], MAX_PACKET_SIZE, 0, (const struct sockaddr *) &receiver_addr,
                               sizeof(struct sockaddr_in));
                        if (input_log(log_file, header) == -1) {
                            printf("Open send_logfile failed!\n");
                            return -1;
                        }
                        memcpy(window[i].packet, chunks[window[i].seq], MAX_PACKET_SIZE);
                    }
                }
                gettimeofday(&start, NULL);
                gettimeofday(&stop, NULL);
            }
        }

        gettimeofday(&stop, NULL);
        if ((double) (stop.tv_sec - start.tv_sec) * 1000 + (double) (stop.tv_usec - start.tv_usec) / 1000 > 400) {
            // printf("Send all the data in the window again. \n" );
            for (int i = 0; i < window_size; i++) {
                struct SRHeader header;
                memcpy(&header, window[i].packet, sizeof(struct SRHeader));
                sendto(sockfd, window[i].packet, MAX_PACKET_SIZE, 0, (const struct sockaddr *) &receiver_addr, sizeof(struct sockaddr_in));
                if (input_log(log_file, header) == -1) {
                    printf("Open send_logfile failed!\n");
                    return -1;
                }
            }
            gettimeofday(&start, NULL);
            gettimeofday(&stop, NULL);
        }
    }

    for (int i = 0; i <= length/MAX_PAYLOAD_SIZE; i++)
        free(chunks[i]);
    free(chunks);

    // FIN
    struct SRHeader fin = {1, random_seq, 0, 0};
    sendto(sockfd, (const struct SRHeader *) &fin, sizeof(struct SRHeader), 0, (const struct sockaddr *) &receiver_addr, sizeof(struct sockaddr_in));
    if (input_log(log_file, fin) == -1) {
        printf("Open send_logfile failed!\n");
        return -1;
    }

    gettimeofday(&start, NULL);
    gettimeofday(&stop, NULL);
    while(1) {
        struct SRHeader recv_fin;
        int n = recvfrom(sockfd, (struct SRHeader *) &recv_fin, sizeof(struct SRHeader), MSG_DONTWAIT, (struct sockaddr *) &receiver_addr, &slen);
        if (n > 0) {
            if (input_log(log_file, recv_fin) == -1) {
                printf("Open send_logfile failed!\n");
                return -1;
            }
            if (recv_fin.flag == 3 && recv_fin.seq == fin.seq)
                break;
        }
        gettimeofday(&stop, NULL);
        if ((double) (stop.tv_sec - start.tv_sec) * 1000 + (double) (stop.tv_usec - start.tv_usec) / 1000 > 400) {
            // printf("Send Fin again. \n" );
            sendto(sockfd, (const struct SRHeader *) &fin, sizeof(struct SRHeader), 0, (const struct sockaddr *) &receiver_addr,
                   sizeof(struct sockaddr_in));
            if (input_log(log_file, fin) == -1) {
                printf("Open send_logfile failed!\n");
                return -1;
            }
            gettimeofday(&start, NULL);
            gettimeofday(&stop, NULL);
        }
    }

    for(int i = 0; i < window_size; i++)
        free(window[i].packet);
    return 0;
}

int receiver(int port, int window_size, const char *recv_dir, const char *log_file) {
    struct sockaddr_in si_me;
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == -1) {
        fputs("socket creation failed!", stderr);
        exit(2);
    }
    memset((char *) &si_me, 0, sizeof(struct sockaddr_in));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(port);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(sockfd , (struct sockaddr*) &si_me, sizeof(struct sockaddr_in));

    char* packet = malloc(MAX_PACKET_SIZE);
    struct SRHeader header;
    char* payload = malloc(MAX_PAYLOAD_SIZE);
    struct buf window[window_size];
    for(int i = 0; i < window_size; i++) {
        window[i].seq = i;
        window[i].packet = malloc(MAX_PACKET_SIZE);
        memset(window[i].packet, 0, MAX_PACKET_SIZE);
    }

    int file_i = 0;  // count the output file number
    unsigned int is_new_file = 1;
    int this_seq;    // seq used by SYN/FIN in the current sending process

    socklen_t slen = sizeof(struct sockaddr_in);
    struct sockaddr_in si_other;

    while (1) {
        memset(packet, 0, MAX_PACKET_SIZE);
        memset(&header, 0, sizeof(struct SRHeader));
        memset(payload, 0, MAX_PAYLOAD_SIZE);

        int n = recvfrom(sockfd, packet, MAX_PACKET_SIZE, MSG_DONTWAIT, (struct sockaddr *) &si_other, &slen);
        if (n > 0) {
            memcpy(&header, packet, sizeof(struct SRHeader));
            memcpy(payload, packet + sizeof(struct SRHeader) , header.len);
            if (input_log(log_file, header) == -1) {
                printf("Open recv_logfile failed!\n");
                break;
            }

            if (header.flag == 0) {
                // SYN
                is_new_file = 1;
                this_seq = header.seq;
                for(int i = 0; i < window_size; i++) {
                    window[i].seq = i;
                    memset(window[i].packet, 0, MAX_PACKET_SIZE);
                }
                struct SRHeader ack = {3, header.seq, 0, 0};
                sendto(sockfd, (const struct SRHeader *) &ack, sizeof(struct SRHeader), 0, (const struct sockaddr *) &si_other, sizeof(struct sockaddr_in));
                if (input_log(log_file, ack) == -1) {
                    printf("Open recv_logfile failed!\n");
                    break;
                }
            }

            if (header.flag == 1) {
                // FIN
                if (is_new_file && this_seq == header.seq) {
                    file_i++;
                }
                is_new_file = 0;
                // printf("FIN: file %d", file_i);
                struct SRHeader ack = {3, header.seq, 0, 0};
                sendto(sockfd, (const struct SRHeader *) &ack, sizeof(struct SRHeader), 0, (const struct sockaddr *) &si_other,
                       sizeof(struct sockaddr_in));
                if (input_log(log_file, ack) == -1) {
                    printf("Open recv_logfile failed!\n");
                    break;
                }
            }

            if (header.flag == 2) {
                // DATA
                unsigned int crc = crc32(payload, header.len);
                if (crc != header.crc) {
                    // printf("crc incorrect %d\n", header.seq);
                    continue;
                }
                if (header.seq >= window_size + window[0].seq)
                    continue;
                if (header.seq < window[0].seq) {
                    struct SRHeader ack = {3, window[0].seq, 0, 0};
                    sendto(sockfd, (const struct SRHeader *) &ack, sizeof(struct SRHeader), 0, (const struct sockaddr *) &si_other,
                           sizeof(struct sockaddr_in));
                    if (input_log(log_file, ack) == -1) {
                        printf("Open recv_logfile failed!\n");
                        break;
                    }
                }
                if (header.seq > window[0].seq) {
                    struct SRHeader ack = {3, window[0].seq, 0, 0};
                    sendto(sockfd, (const struct SRHeader *) &ack, sizeof(struct SRHeader), 0, (const struct sockaddr *) &si_other,
                           sizeof(struct sockaddr_in));
                    if (input_log(log_file, ack) == -1) {
                        printf("Open recv_logfile failed!\n");
                        break;
                    }
                    memcpy(window[header.seq-window[0].seq].packet, packet, MAX_PACKET_SIZE);
                }
                if (header.seq == window[0].seq) {
                    memcpy(window[0].packet, packet, MAX_PACKET_SIZE);
                    int i = 0;  // window[0].seq + i == z+1
                    char* zero_mem = malloc(MAX_PACKET_SIZE);
                    memset(zero_mem, 0, MAX_PACKET_SIZE);
                    for (i = 0; i < window_size; i++){
                        if (memcmp(zero_mem, window[i].packet, MAX_PACKET_SIZE) == 0)
                            break;
                    }
                    free(zero_mem);

                    struct SRHeader ack = {3, (window[0].seq+i), 0, 0};
                    sendto(sockfd, (const struct SRHeader *) &ack, sizeof(struct SRHeader), 0, (const struct sockaddr *) &si_other,
                           sizeof(struct sockaddr_in));
                    if (input_log(log_file, ack) == -1) {
                        printf("Open recv_logfile failed!\n");
                        break;
                    }
					
		    // deal with output filename
                    char *file_i_str = malloc(sizeof(int) + 1);
                    memset(file_i_str, 0, sizeof(int) + 1);
                    sprintf(file_i_str, "%d", file_i);
                    char *filename = malloc(strlen(recv_dir) + strlen(file_i_str) + 10);
                    memset(filename, 0, strlen(recv_dir) + strlen(file_i_str) + 10);

                    memcpy(filename, recv_dir, strlen(recv_dir));
                    memcpy(filename + strlen(recv_dir), "file_", 5);
                    memcpy(filename + strlen(recv_dir) + 5, file_i_str, strlen(file_i_str));
                    memcpy(filename + strlen(recv_dir) + 5 + strlen(file_i_str), ".txt", 4);
                    // printf("%s\n", filename);

                    FILE *fp = fopen(filename, "ab");
                    if (fp == NULL) {
                        printf("Open file_i.txt failed!\n");
                        break;
                    }

                    char* window_payload = malloc(MAX_PAYLOAD_SIZE);
                    struct SRHeader window_header;
                    for (int j = 0; j < i; j++) {
                        memset(&window_header, 0, sizeof(struct SRHeader));
                        memset(window_payload, 0, MAX_PAYLOAD_SIZE);
                        memcpy(&window_header, window[j].packet, sizeof(struct SRHeader));
                        memcpy(window_payload, window[j].packet + sizeof(struct SRHeader) , window_header.len);
                        fwrite(window_payload, sizeof(char), window_header.len, fp);
                        memset(window[j].packet, 0, MAX_PACKET_SIZE);
                        window[j].seq += i;
                    }
                    free(window_payload);
                    for (int j = i; j < window_size; j++){
                        memcpy(window[j-i].packet, window[j].packet, MAX_PACKET_SIZE);
                        memset(window[j].packet, 0, MAX_PACKET_SIZE);
                        window[j].seq += i;
                    }

                    fclose(fp);
                    free(file_i_str);
                    free(filename);
                }
            }
        }
    }

    free(payload);
    free(packet);
    for(int i = 0; i < window_size; i++)
        free(window[i].packet);
    return -1;
    // since the receiver should keep running, if it breaks the while loop,
    // there should be some error
}

int main(int argc, char **argv){
    // Parse command line arguments, assuming all the arguments are exactly correct
    if (strcmp(argv[1], "-r") == 0){
        // receiver mode
        int port = atoi(argv[2]);
        int window_size = atoi(argv[3]);
        const char *recv_dir = argv[4];
        const char *log_file = argv[5];
        // printf("Running in receiver mode\n");
        if (receiver(port, window_size, recv_dir, log_file) == -1) {
            return 1;
        }
    }

    if (strcmp(argv[1], "-s") == 0){
        // sender mode
        const char *receiver_ip = argv[2];
        int receiver_port = atoi(argv[3]);
        int myport = atoi(argv[4]);
        int window_size = atoi(argv[5]);
        const char *filename = argv[6];
        const char *log_file = argv[7];
        // printf("Running in sender mode\n");
        if (sender(receiver_ip, receiver_port, myport, window_size, filename, log_file) == -1) {
            return 1;
        }
    }
    return 0;
}
