#include <bits/stdc++.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
using namespace std;

#ifndef _ROUTING_H_
#define _ROUTING_H_

struct rtValue {
    unsigned char dstMAC[6];
    int32_t deviceID;
    int32_t distance;
    int32_t timestamp;
    rtValue() {deviceID = -1; timestamp = 0; distance = 1e9; };
};

struct nxtHop {
    unsigned char dstMAC[6];
    int deviceID;
};

//rtKey: pair(dst, mask)
typedef pair <in_addr_t, in_addr_t> rtKey;

void initRoutingTable();

//find corresponding entry according to Longest Prefix Matching
vector <nxtHop> lookForRouting(const struct in_addr dest, const struct in_addr mask);

//Broadcast your RoutingTable to neighbours
int sendDVR();

//Change your RoutingTable according to your neighbour's RoutingTable
int recvDVR(const void* buf, int len, int id, const void* MAC);

//Discard the expired entry of RoutingTable
int updateDVR();


void *routingThread(void *vargp);

#endif
