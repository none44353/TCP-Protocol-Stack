#include "packetio.h"
#include "ip.h"
#include "routing.h"
#include <unistd.h>
#include <sys/time.h>

/**
* @file packetio.h
* @brief Library supporting sending/receiving Ethernet II frames.
*/

extern IPPacketReceiveCallback my_recvPacket;
extern int recvPacket_SF;
extern bool ResentFlag;

/**
* @brief Encapsulate some data into an Ethernet II frame and send it.
*
* @param buf Pointer to the payload.
* @param len Length of the payload.
* @param ethtype EtherType field value of this frame.
* @param destmac MAC address of the destination.
* @param id ID of the device(returned by `addDevice`) to send on. @return 0 on success, -1 on error. @see addDevice
*/

uint16_t changeEndian16(uint16_t x) {
    uint16_t mask = 0xff;
    return ((x & mask) << 8) | ((x >> 8) & mask);
}

int sendFrame(const void* buf, int len, int ethtype, const void* destmac, int id) {
    //pcap_sendpacket(pcap_t *p, const u_char *buf, int size);
    Device* sender = getDevice(id);
    if (sender == NULL) return -1; // can not find this device
     
    u_char* frame = new u_char[len + 14 + 4];
    
    //build ethHeader
    ethHdr_t header;
    memcpy(header.dstMAC, destmac, 6);
    memcpy(header.srcMAC, sender -> MAC, 6);
    header.ethtype = changeEndian16(ethtype);
    
    //construct the frame
    memcpy(frame, &header, sizeof(ethHdr_t));
    memcpy(frame + sizeof(ethHdr_t), buf, len);

 /*  if (!ResentFlag) {
        struct timeval t;
        gettimeofday(&t, NULL);
        fprintf(stderr, "at (%ld.%ld), LINK LAYER send\n", t.tv_sec, t.tv_usec);
        ipHdr_t ipHdr;
        memcpy(&ipHdr, buf, sizeof(ipHdr_t));
        uint8_t* ip = (uint8_t*)&ipHdr.src; 
        for (int i = 0; i < 4; ++i) printf("%02x%c", ip[i], ": "[i == 3]);

        ip = (uint8_t*)&ipHdr.dst; 
        for (int i = 0; i < 4; ++i) printf("%02x%c", ip[i], ": "[i == 3]);
        
        printf("%02x %d\n", ipHdr.protocol, (int)ipHdr.ttl);
        fprintf(stderr, "-----------------------\n");fflush(stderr);
    }*/
    //setChecksum
    //getChecksum(fram, len + 14); 
    int ret = pcap_sendpacket(sender -> handle, (const u_char*)frame, len + sizeof(ethHdr_t) + 4);
    
    delete [] frame;
    if (ret != 0) return -1;
    return 0;
}

/**
* @brief Process a frame upon receiving it.
* @param buf Pointer to the frame.
* @param len Length of the frame.
* @param id ID of the device (returned by `addDevice`) receiving current frame. @return 0 on success, -1 on error. @see addDevice
*/
//typedef int (*frameReceiveCallback)(const void*, int, int);

int recvFrame(const void* buf, int len, int id) {
    ethHdr_t ethHdr; 
    memcpy(&ethHdr, buf, sizeof(ethHdr_t));
    
    ipHdr_t ipHdr; 
    memcpy(&ipHdr, buf + sizeof(ethHdr_t), sizeof(ipHdr_t));

   /* if (!ResentFlag) {
        struct timeval t;
        gettimeofday(&t, NULL);
        fprintf(stderr, "at (%ld.%ld), LINK LAYER receive\n", t.tv_sec, t.tv_usec);
        uint8_t* ip = (uint8_t*)&ipHdr.src; 
        for (int i = 0; i < 4; ++i) printf("%02x%c", ip[i], ": "[i == 3]);

        ip = (uint8_t*)&ipHdr.dst; 
        for (int i = 0; i < 4; ++i) printf("%02x%c", ip[i], ": "[i == 3]);
        
        printf("%02x %d\n", ipHdr.protocol, (int)ipHdr.ttl);
        fprintf(stderr, "-----------------------\n");fflush(stderr);
    }*/

    Device* device = getDevice(id); 
    if (ipHdr.src == device -> IP || ipHdr.protocol == 0x80) {
       // fprintf(stderr, "Ignoring this frame.\n\n");
       // fflush(stderr);
        return -1; //Ignoring this frame.
    }

    if (ipHdr.protocol == IPV4_MYARP) { //These packets are used for routing.
       // fprintf(stderr, "MYARP packet is received\n\n");
       // fflush(stderr);

        recvDVR(buf + sizeof(ethHdr_t), len - sizeof(ethHdr_t) - 4, id, ethHdr.srcMAC);
        return 0;
    }

    bool ismyPacket = false;
    for (DeviceList *ptr = getDevlist(); ptr != NULL; ptr = ptr -> nxt) {
		if (ipHdr.dst == (ptr -> cur) -> IP) ismyPacket = true;
	}

    int ret;
    if (ismyPacket || ipHdr.dst == 0xffffffff) { //receive a Data packet
          //  fprintf(stderr, "I think it's mine\n");
           // fflush(stderr);

        if (!recvPacket_SF) {
            fflush(stderr);
            fprintf(stderr, "[Error]: recvPacket havn't be set!\n\n");
            fflush(stderr);
            ret = -1;
        }  
        else 
            ret = my_recvPacket(buf + sizeof(ethHdr_t), len - sizeof(ethHdr_t) - 4); //4 is the size of CRC code
    }
    
    if (!ismyPacket) {
       // fprintf(stderr, "Forwarding\n\n");
       // fflush(stderr);

        ret = forward(buf + sizeof(ethHdr_t), len - sizeof(ethHdr_t) - 4);
    }
    return ret;
}

void *handlerThread(void *vargp) {
    pthread_detach(pthread_self()); 
	
    void* argp = vargp;
    Device* device = *((Device**)argp); argp = (Device**)argp + 1;
    frameReceiveCallback callback = *((frameReceiveCallback*)argp); argp = (frameReceiveCallback*)argp + 1;
    const u_char* packet = *((const u_char**)argp); argp = (const u_char**)argp + 1;
    struct pcap_pkthdr header = *((struct pcap_pkthdr*)argp);  argp = NULL;
    free(vargp);

    //int recvFrame(const void* buf, int len, int id);
    callback(packet, header.len, device -> id);
}

pthread_t headerid;
void *deviceThread(void *vargp) {
    //Separate threads so that the system can reclaim memory resources and avoid leaks.
    pthread_detach(pthread_self()); 
	
    void* argp = vargp;
    Device* device = *((Device**)argp); argp = (Device**)argp + 1;
    frameReceiveCallback callback = *((frameReceiveCallback*)argp); argp = NULL;
    free(vargp);

    const u_char* packet;
    struct pcap_pkthdr header;

    while (true) { // now the device can listen
        packet = pcap_next(device -> handle, &header);

        //receiverOutputAsking();
        /*void* harg = malloc(sizeof(Device*) + sizeof(frameReceiveCallback) + sizeof(const u_char*) + sizeof(struct pcap_pkthdr));
        void* hargp = harg;
        *((Device**)hargp) = device; hargp = (Device**)hargp + 1;
        *((frameReceiveCallback*)hargp) = callback; hargp = (frameReceiveCallback*)hargp + 1;
        *((const u_char**)hargp) = packet; hargp = (const u_char**)hargp + 1;
        *((struct pcap_pkthdr*)hargp) = header;  hargp = NULL;
        
        pthread_create(&headerid, NULL, handlerThread, harg);*/
        callback(packet, header.len, device -> id);
        //receiverOutputFinished();
    }

    return NULL;
}

/**
* @brief Register a callback function to be called each time an Ethernet II frame was received.
* @param id the device id
* @param callback the callback function.
* @return 0 on success, -1 on error.
* @see frameReceiveCallback
*/
pthread_t tid;
int setFrameReceiveCallback(int id, frameReceiveCallback callback) {
    Device* device = getDevice(id);
    if (device == NULL) return -1; // can not find this device

    void* arg = malloc(sizeof(Device*) + sizeof(frameReceiveCallback));
    void* argp = arg;
    *((Device**)argp) = device; argp = (Device**)argp + 1;
    *(frameReceiveCallback*)argp = callback; argp = NULL;
    
    pthread_create(&tid, NULL, deviceThread, arg);

    return 0;
}