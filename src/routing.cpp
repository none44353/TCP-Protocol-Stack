#include "routing.h"
#include "device.h" 
#include "ip.h"
#include "packetio.h"
#include <semaphore.h>
#include <arpa/inet.h>
#include <netinet/ip.h>

int clocker;
map <rtKey, rtValue> RoutingTable;

void initRoutingTable() {
    clocker = 0;
}


extern bool ResentFlag;

/**
* @brief Add an item to routing table. 
*
* @param dest The destination IP prefix.
* @param mask The subnet mask of the destination IP prefix.
* @param nextHopMAC MAC address of the next hop.
* @param deviceID id of device to send packets on.
* @return 0 on success, -1 on error
*/
int setRoutingTable(const struct in_addr dest, const struct in_addr mask, 
    const void* nxtHopMAC, const int& deviceID, const int& distance) {

    rtKey key = make_pair(dest.s_addr, mask.s_addr);
    map <rtKey, rtValue> :: iterator it = RoutingTable.find(key);

    rtValue value;
    memcpy(value.dstMAC, nxtHopMAC, 6);
    value.deviceID = deviceID, value.distance = distance, value.timestamp = clocker;

    //Check for special case
    for (DeviceList *ptr = getDevlist(); ptr != NULL; ptr = ptr -> nxt) 
        if (ptr -> cur -> IP == key.first) { // want to send to same-host device
            memcpy(value.dstMAC, ptr -> cur -> MAC, 6);
            value.distance = 0; // special case
        }
    
    if (it == RoutingTable.end()) RoutingTable.insert(make_pair(key, value));
    else 
        if ((*it).second.timestamp == ((clocker + 1) & 3) 
        || (*it).second.distance >= value.distance) (*it).second = value;
    
    return 0;
}

vector <nxtHop> lookForRouting(const struct in_addr dest, const struct in_addr mask) {
    vector <nxtHop> ret;
    ret.clear();
 
    if (dest.s_addr == 0xffffffff) { // No need to view RoutingTable when broadcast
        //printf("Routing but Broadcast\n"); fflush(stderr);
        DeviceList* devlist_ptr = getDevlist();
        for (DeviceList *ptr = devlist_ptr; ptr != NULL; ptr = ptr -> nxt) {
            nxtHop nhop;
            for (int i = 0; i < 6; ++i) nhop.dstMAC[i] = 0xff;
            nhop.deviceID = ptr -> cur -> id;
            ret.push_back(nhop);
        }
        
        return ret;
    }

    /*if (!ResentFlag) {
        printf("LookRoutingTable\n");
        uint32_t ip = dest.s_addr; uint8_t* c = (uint8_t*)&ip;
        printf("dest=%u ", dest.s_addr);
        for (int j = 0; j < 4; ++j) printf("%02x%c", c[j], ": "[j == 3]);
        ip = mask.s_addr; c = (uint8_t*)&ip;
        printf("mask=%u ", mask.s_addr);
        for (int j = 0; j < 4; ++j) printf("%02x%c", c[j], ":\n"[j == 3]); fflush(stderr);
    }*/

    map <rtKey, rtValue> :: iterator it = RoutingTable.begin();
    
    in_addr_t preLst = 0; 
    bool flag = false;
    for (it; it != RoutingTable.end(); ++it) {
        rtKey key = (*it).first;
       // printf("-key (%u,%u)\n", key.first, key.second); fflush(stderr);
        if ((key.first & key.second) != (dest.s_addr & key.second) 
        || (key.second & mask.s_addr) != key.second) continue;
        
        /*uint32_t ip = key.first; uint8_t* c = (uint8_t*)&ip;
        printf("**d=%u ", key.first);
        for (int j = 0; j < 4; ++j) printf("%02x%c", c[j], ": "[j == 3]);
        ip = key.second; c = (uint8_t*)&ip;
        printf("**m=%u ", key.second);
        for (int j = 0; j < 4; ++j) printf("%02x%c", c[j], ": "[j == 3]); fflush(stderr);
        rtValue value = (*it).second;        

        printf("id=%d ", value.deviceID);
        printf("dis=%d ", value.distance);
        c = value.dstMAC;
        for (int j = 0; j < 6; ++j) printf("%02x%c", c[j], ":\n"[j == 5]);*/

        preLst = max(preLst, key.second);
        flag = true;
    }
    
    if (flag) {
        it = RoutingTable.begin();
        for (it; it != RoutingTable.end(); ++it) {
            rtKey key = (*it).first;
            if ((key.first & key.second) != (dest.s_addr & key.second) 
            || (key.second & mask.s_addr) != key.second) continue;

            if (key.second != preLst) continue;

            rtValue value = (*it).second;        
        /*  printf("dis=[%d] ", value.distance);
            uint8_t* c = value.dstMAC;
            for (int j = 0; j < 6; ++j) printf("%02x%c", c[j], ":\n"[j == 5]);
            printf("\n\n"); fflush(stderr);*/

            nxtHop nhop; 
            nhop.deviceID = value.deviceID;
            memcpy(nhop.dstMAC, value.dstMAC, 6);

            ret.push_back(nhop);
            preLst = 0x10; // In this case, only 1 nxthop is needed.
        }
    }

    if (!ret.size()) { //nothing in routing table
        DeviceList* devlist_ptr = getDevlist();
        for (DeviceList *ptr = devlist_ptr; ptr != NULL; ptr = ptr -> nxt) {
            nxtHop nhop;
            for (int i = 0; i < 6; ++i) nhop.dstMAC[i] = 0xff;
            nhop.deviceID = ptr -> cur -> id;
            ret.push_back(nhop);
        }
    }
    
    return ret;
}

void PrintDVR(char* buf, int len) {
    if (false) {
        printf("PRINT DVR\n"); fflush(stderr);
        char* ptr = (char*)buf + sizeof(ipHdr_t);
        for (int i = 0, pos = sizeof(ipHdr_t); pos < len; ++i) {
            rtKey key;
            rtValue value;   

            key.first = *((in_addr_t *)ptr); ptr += sizeof(in_addr_t);
            key.second = *((in_addr_t *)ptr); ptr += sizeof(in_addr_t);
            struct in_addr dest; dest.s_addr = key.first;
            struct in_addr mask; mask.s_addr = key.second;
            memcpy(&value, ptr, sizeof(rtValue)); ptr += sizeof(rtValue);

            uint32_t ip = dest.s_addr; uint8_t* c = (uint8_t*)&ip;
            printf("dest=");
            for (int j = 0; j < 4; ++j) printf("%02x%c", c[j], ": "[j == 3]);
            ip = mask.s_addr; c = (uint8_t*)&ip;
            printf("mask=");
            for (int j = 0; j < 4; ++j) printf("%02x%c", c[j], ": "[j == 3]);
            printf("dis=%d ", value.distance);
            c = value.dstMAC;
            for (int j = 0; j < 6; ++j) printf("%02x%c", c[j], ": "[j == 5]);

            printf("deviceID=%d\n", value.deviceID);
            pos += (sizeof(in_addr_t) * 2 + sizeof(rtValue));
        }
        printf("---------------------------------------\n"); fflush(stderr);
    }
}

//Distance Vector Routing Strategy is used.
int sendDVR() {
    ReaderIn();    
    
    //need to record our routing table.
    int n = RoutingTable.size() + 1;
    int length = n * (sizeof(in_addr_t) * 2 + sizeof(rtValue));
    char* buf = new char[length + sizeof(ipHdr_t)];
    char* ptr = buf + sizeof(ipHdr_t);
    
    map <rtKey, rtValue> :: iterator it = RoutingTable.begin();
    for (; it != RoutingTable.end(); ++it) {
        rtKey key = (*it).first;
        rtValue value = (*it).second;

        *((in_addr_t *)ptr) = key.first; ptr += sizeof(in_addr_t);
        *((in_addr_t *)ptr) = key.second; ptr += sizeof(in_addr_t);
        memcpy(ptr, &value, sizeof(rtValue)); ptr += sizeof(rtValue);
    }

    //you need to choose the srcIP, which depends on divice
    struct in_addr dest; dest.s_addr = 0xffffffff;
    struct in_addr mask; mask.s_addr = 0xffffffff;
    vector <nxtHop> nxtHops = lookForRouting(dest, mask);

    ipHdr_t header;
    //build IP Header
    uint8_t c[4];
    uint32_t total_len = (uint32_t)length + sizeof(ipHdr_t);
    header.p1 = 0; 
    c[0] = 0x45, c[1] = 0, c[2] = (uint8_t)((total_len >> 8) & 0xff), c[3] = (uint8_t)(total_len & 0xff);
    header.p1 = merge32(c);
    header.p2 = 0;
    header.ttl = 1; //you don't neet to forward this kind of packets
    header.protocol = IPV4_MYARP; 
    header.hdrCRC = 0;
    //header.src = src.s_addr; Depends on the device.
    header.dst = dest.s_addr;
    
    for (int i = 0, sze = nxtHops.size(); i < sze; ++i) {
        nxtHop nhop = nxtHops[i];
        Device* ndevice = getDevice(nhop.deviceID);

        if (ndevice == NULL) continue;
        header.src = ndevice -> IP; 

        //fill the packet header
        memcpy(buf, &header, sizeof(ipHdr_t));

        //fill the buffer(send the information of yourself)
        rtKey key = make_pair(ndevice -> IP, 0xffffffff); 
        rtValue value; value.distance = 0;
        
        char* mptr = ptr;
        *((in_addr_t *)mptr) = key.first; mptr += sizeof(in_addr_t);
        *((in_addr_t *)mptr) = key.second; mptr += sizeof(in_addr_t);
        memcpy(mptr, &value, sizeof(rtValue)); mptr += sizeof(rtValue);

        PrintDVR(buf, total_len);//Print Out

        int ret = sendFrame(buf, total_len, ETHTYPE_IPV4, nhop.dstMAC, nhop.deviceID);
    }

    delete [] buf;
    ReaderOut();
    
    return 0;
}

int recvDVR(const void* buf, int len, int id, const void* MAC) {
    ipHdr_t header;
    memcpy(&header, buf, sizeof(ipHdr_t));

    char* ptr = (char*)buf + sizeof(ipHdr_t);
    for (int i = 0, pos = sizeof(ipHdr_t); pos < len; ++i) {
        rtKey key;
        rtValue value;   

        key.first = *((in_addr_t *)ptr); ptr += sizeof(in_addr_t);
        key.second = *((in_addr_t *)ptr); ptr += sizeof(in_addr_t);
        struct in_addr dest; dest.s_addr = key.first;
        struct in_addr mask; mask.s_addr = key.second;
        memcpy(&value, ptr, sizeof(rtValue)); ptr += sizeof(rtValue);

        //use these information to update our Table.
        WriterIn();    
        setRoutingTable(dest, mask, MAC, id, value.distance + 1); 
        WriterOut();

        pos += (sizeof(in_addr_t) * 2 + sizeof(rtValue));
    }
    
    return 0;
}

int updateDVR() {
    WriterIn();    

    map <rtKey, rtValue> :: iterator it = RoutingTable.begin();
    for (; it != RoutingTable.end();) {
        if ((*it).second.timestamp == ((clocker + 1) & 3)) 
            it = RoutingTable.erase(it);
        else 
            ++it;
    }

    clocker = (clocker + 1) & 3;
    WriterOut();

    return 0;
}

void *routingThread(void *vargp) {
    //Separate threads so that the system can reclaim memory resources and avoid leaks.
    pthread_detach(pthread_self());
        
    while (true) {
        updateDVR();
        sendDVR();
        sleep(5);


        //Device* ndevice = getDevice(0);
        //Testing send a packet
        /*if (memcmp(ndevice -> name, "veth1", 5) == 0) {
		    senderOutputAsking();
            fprintf(stderr, "Sending my packet.\n");
            struct in_addr src; src.s_addr = changeEndian32(0x0a640101);
            struct in_addr dest; dest.s_addr = changeEndian32(0x0a640402);
            string text = "Hello, I'm Here!";
            sendIPPacket(src, dest, 0xe0, text.c_str(), text.length(), 3);
		    senderOutputFinished();
        }*/
        //
    }

    return NULL;
}