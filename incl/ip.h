/**
* @file ip.h
* @brief Library supporting sending/receiving IP packets encapsulated in an Ethernet II frame.
*/
#include <netinet/ip.h>
#include <arpa/inet.h>

#ifndef _IP_H_
#define _IP_H_

#define ETHTYPE_IPV4 0x0800
#define IPV4_MYARP 0xf0
#define IPV4_TCP 0x06

struct ipHdr_t {
    uint32_t p1;
    uint32_t p2;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t hdrCRC;
    uint32_t src;
    uint32_t dst;
};

uint32_t changeEndian32(uint32_t x);

//Merge c0, c1, c2, c3 together, so they can in-order in memory
//This mechina is little Endian one
uint32_t merge32(uint8_t* c);

/**
* @brief Send an IP packet to specified host.
*
* @param src Source IP address.
* @param dest Destination IP address.
* @param proto Value of `protocol` field in IP header.
* @param buf pointer to IP payload
* @param len Length of IP payload
* @return 0 on success, -1 on error.
*/
int sendIPPacket(const struct in_addr src, const struct in_addr dest, int proto, const void *buf, int len, int ttl = 32);

/**
* @brief Process an IP packet upon receiving it.
*s
* @param buf Pointer to the packet.
* @param len Length of the packet.
* @return 0 on success, -1 on error.
* @see addDevice
*/
typedef int (*IPPacketReceiveCallback)(const void* buf, int len);

int recvPacket(const void* buf, int len);

uint32_t ptr2ip(const uint8_t* ptr);
/**
* @brief Register a callback function to be called each time an IP packet was received.
*
* @param callback The callback function.
* @return 0 on success, -1 on error.
* @see IPPacketReceiveCallback
*/
int setIPPacketReceiveCallback(IPPacketReceiveCallback callback);

/**
* @brief forward a packet which is not sended to current host
*
* @param buf Pointer to the packet.
* @param len Length of the packet.
* @return 0 on success, -1 on error.
*/
int forward(const void* buf, int len);

#endif
