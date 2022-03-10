#include "ip.h"
#include "packetio.h"
#include "routing.h"
#include "tcp.h"

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

extern bool ResentFlag;

IPPacketReceiveCallback my_recvPacket;
int recvPacket_SF = 0;

uint32_t changeEndian32(uint32_t x) {
    uint32_t mask = 0xff, y = x;
    u_char c[4];
    for (int i = 3; i >= 0; --i, y >>= 8) c[i] = (y & mask);
    for (int i = 3; i >= 0; --i) y = (y << 8) | c[i];
    return y;
}

uint32_t ptr2ip(const uint8_t* ptr){
    uint32_t ret = *((const uint32_t*) ptr);
    return ret;
}

//Merge c0, c1, c2, c3 together, so they can in-order in memory
//This mechina is little Endian one
uint32_t merge32(uint8_t* c) { 
    uint32_t x = 0;
    for (int i = 3; i >= 0; --i) x = (x << 8) | c[i];
    return x;
}

int sendIPPacket(const struct in_addr src, const struct in_addr dest, int proto, const void *buf, int len, int ttl) {   
    //int sendFrame(const void* buf, int len, int ethtype, const void* destmac, int id);
    u_char* packet = new u_char[len + 20];

    ipHdr_t header;
    //build IP Header
    uint8_t c[4];
    uint32_t total_len = (uint32_t)len + sizeof(ipHdr_t);
    header.p1 = 0; 
    c[0] = 0x45, c[1] = 0, c[2] = (uint8_t)((total_len >> 8) & 0xff), c[3] = (uint8_t)(total_len & 0xff);
    header.p1 = merge32(c);
    header.p2 = 0;
    header.ttl = ttl;
    header.protocol = proto; 
    header.hdrCRC = 0;
    header.src = src.s_addr;
    header.dst = dest.s_addr;
    //construct the packet
    memcpy(packet, &header, sizeof(ipHdr_t));
    memcpy(packet + sizeof(ipHdr_t), buf, len);

    //know where to send
    struct in_addr mask; mask.s_addr = 0xffffffff;
    
    ReaderIn(); 
    vector <nxtHop> nxtHops = lookForRouting(dest, mask);
    ReaderOut();
/*
    if (!ResentFlag) {
        for (int i = 0, sze = nxtHops.size(); i < sze; ++i) {
            nxtHop nhop = nxtHops[i];
            if (true) {
                printf("(");
                uint8_t* _ip = (uint8_t *)&header.src;
                for (int j = 0; j < 4; ++j) printf("%02x%c", _ip[j], ":,"[j == 3]);
                _ip = (uint8_t *)&header.dst;
                for (int j = 0; j < 4; ++j) printf("%02x%c", _ip[j], ":)"[j == 3]);
                printf("  ");
                for (int j = 0; j < 6; ++j) printf("%02x%c", nhop.dstMAC[j], ":\n"[j == 5]);
                printf("-------------------------\n"); fflush(stderr);
            }

        }
    }*/

    bool flag = false;
    for (int i = 0, sze = nxtHops.size(); i < sze; ++i) {
        nxtHop nhop = nxtHops[i];
        int ret = sendFrame(packet, len + sizeof(ipHdr_t), ETHTYPE_IPV4, nhop.dstMAC, nhop.deviceID);
        if (!ret) flag = true;
    }
    
    delete packet;
    if (flag) return 0;

    return -1;
}

int recvPacket(const void* buf, int len) {
    ipHdr_t ipHdr;
    memcpy(&ipHdr, buf, sizeof(ipHdr_t));

    tcpHdr_t tcpHdr;
    memcpy(&tcpHdr, buf + sizeof(ipHdr_t), sizeof(tcpHdr_t));

    Info_t Info; 
    Info.srcIP = ipHdr.src;
    Info.dstIP = ipHdr.dst;
    Info.srcPort = tcpHdr.srcPort;
    Info.dstPort = tcpHdr.dstPort;

    //int recvTCPPacket(const void* buf, int len, Info_t* info);
    recvTCPPacket(buf + sizeof(ipHdr_t), len - sizeof(ipHdr_t), Info); 
    return 0;
}
/**
* @brief Register a callback function to be called each time an IP packet was received.
*
* @param callback The callback function.
* @return 0 on success, -1 on error.
* @see IPPacketReceiveCallback
*/
int setIPPacketReceiveCallback(IPPacketReceiveCallback callback) {
    my_recvPacket = callback;
    recvPacket_SF = 1; //set the flag after the procedure is setted

    return 0;
}

/**
* @brief forward a packet which is not sended to current host
*
* @param buf Pointer to the packet.
* @param len Length of the packet.
* @return 0 on success, -1 on error.
*/
int forward(const void* buf, int len) {
    if (sizeof(ipHdr_t) > len) return -1;

    ipHdr_t header;
    memcpy(&header, buf, sizeof(ipHdr_t));

    struct in_addr src; src.s_addr = header.src;
    struct in_addr dst; dst.s_addr = header.dst;

    if (header.ttl == 0) return -1;

    tcpHdr_t tcpheader;
    memcpy(&tcpheader, buf + sizeof(ipHdr_t), sizeof(tcpHdr_t));
    tcpheader.seq = changeEndian32(tcpheader.seq);
    tcpheader.ack = changeEndian32(tcpheader.ack);
    tcpheader.window = changeEndian16(tcpheader.window);

/*    
    if (!ResentFlag) {
        fprintf(stderr, "TCP forwarding PACKET\n");
        fprintf(stderr, "seq=%d ack=%d flag=%04x\n", tcpheader.seq, tcpheader.ack, tcpheader.flags);
        uint8_t* ip = (uint8_t*)&src.s_addr; 
        for (int i = 0; i < 4; ++i) printf("%02x%c", ip[i], ": "[i == 3]);
        uint8_t* port = (uint8_t*)&tcpheader.srcPort; 
        for (int i = 0; i < 2; ++i) printf("%02x%c", port[i], ":\n"[i == 1]);

        ip = (uint8_t*)&dst.s_addr; 
        for (int i = 0; i < 4; ++i) printf("%02x%c", ip[i], ": "[i == 3]);
        port = (uint8_t*)&tcpheader.dstPort; 
        for (int i = 0; i < 2; ++i) printf("%02x%c", port[i], ":\n"[i == 1]);
        fprintf(stderr, "-----------------------\n");
    }*/

    int ret = sendIPPacket(src, dst, header.protocol, buf + sizeof(ipHdr_t), len - sizeof(ipHdr_t), header.ttl - 1);
    return ret;
}
