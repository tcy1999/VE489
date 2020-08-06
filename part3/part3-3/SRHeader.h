#ifndef __SR_HEADER_H__
#define __SR_HEADER_H__

struct SRHeader
{
	unsigned int flag;     // 0: START; 1: END; 2: DATA; 3: ACK
    unsigned int seq;   // Described below
    unsigned int len;   // Length of data; 0 for ACK packets
    unsigned int crc; // 32-bit CRC
};

#define NACK 4
#define ACK 3
#define DATA 2
#define FIN 1
#define SYN 0

#define MAX_PAYLOAD_SIZE 1456

#endif
