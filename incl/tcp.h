#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#ifndef _TCP_H_
#define _TCP_H_

#define MAX_PORT_NUMBER (1 << 16)

struct Info_t{
    uint32_t srcIP;
    uint32_t dstIP;
    uint16_t srcPort;
    uint16_t dstPort;
};

struct tcpHdr_t{
    uint16_t srcPort;
    uint16_t dstPort;
    uint32_t seq;
    uint32_t ack;
    uint16_t flags;
    uint16_t window;
    uint32_t others;
};

#define FLAG_SYN (1 << 9)
#define FLAG_ACK (1 << 12)
#define FLAG_FIN (1 << 8)

bool isSYN(uint16_t flag);
bool isACK(uint16_t flag);
bool isFIN(uint16_t flag);
uint16_t setSYN(uint16_t flag);
uint16_t setACK(uint16_t flag);
uint16_t setFIN(uint16_t flag);

/**
* @brief Send an TCP packet to specified host.
*
* @param buf pointer to payload
* @param len Length of payload
* @param info Information about Source IP address, Source Port, Destination IP address, Destination Port.
* @param seq SEQ number
* @param ack ACK number
* @param flags Value of flags in header
* @return 0 on success, -1 on error.
*/
int sendTCPPacket(const void* buf, int len, Info_t* info, uint32_t seq, uint32_t ack, int16_t flags);

int recvTCPPacket(const void* buf, int len, Info_t info);

#endif