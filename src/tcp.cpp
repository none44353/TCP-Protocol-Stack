#include "tcp.h"
#include "ip.h"
#include "socket.h"
#include "packetio.h"
#include <sys/time.h>
#include <condition_variable>

//每个host上有一个独立的kernel锁，表示host上不会出现socket操作的并行
extern std :: mutex kernel_mutex;
extern std :: condition_variable kernel_cv; 
extern bool ResentFlag;

bool ARQflag = true;

bool isSYN(uint16_t flag) {
    return (flag & FLAG_SYN) ? true : false;
}

bool isACK(uint16_t flag) {
    return (flag & FLAG_ACK) ? true : false;
}

bool isFIN(uint16_t flag) {
    return (flag & FLAG_FIN) ? true : false;
}

uint16_t setSYN(uint16_t flag) {
    return flag | FLAG_SYN;
}

uint16_t setACK(uint16_t flag) {
    return flag | FLAG_ACK;
}

uint16_t setFIN(uint16_t flag) {
    return flag | FLAG_FIN;
}

uint16_t getchecksum(const void *buf, size_t nbyte) {
    uint16_t xorsum = 0;
    const uint16_t* ptr = (const uint16_t*)buf;
    int i = 0;
    for (; i + 2 <= nbyte; i += 2)
        xorsum ^= (*ptr), ptr++;
    
    if (i != nbyte) {
        uint8_t y = *(const uint8_t*)ptr;
        xorsum ^= (uint16_t)y;
    }

    return xorsum;
}

int sendTCPPacket(const void* buf, int len, Info_t* info, uint32_t seq, uint32_t ack, int16_t flags) {
   /* static int isfirst = 1;
    if (isfirst) {
        isfirst = 0;
        struct timeval t;
        gettimeofday(&t, NULL);
        printf("Send! (%ld.%ld)\n", t.tv_sec, t.tv_usec); fflush(stderr);
    }*/
   
   
    /*if (ResentFlag && seq > 15000 && ack > 15000) {
        fprintf(stderr, "** TCP send PACKET\n");
        fprintf(stderr, "seq=%d ack=%d flag=%04x\n", seq, ack, flags);
        fflush(stderr);

        uint8_t* ip = (uint8_t*)&(info -> srcIP); 
        for (int i = 0; i < 4; ++i) printf("%02x%c", ip[i], ": "[i == 3]);
        uint8_t* port = (uint8_t*)&(info -> srcPort); 
        for (int i = 0; i < 2; ++i) printf("%02x%c", port[i], ":\n"[i == 1]);

        ip = (uint8_t*)&(info -> dstIP); 
        for (int i = 0; i < 4; ++i) printf("%02x%c", ip[i], ": "[i == 3]);
        port = (uint8_t*)&(info -> dstPort); 
        for (int i = 0; i < 2; ++i) printf("%02x%c", port[i], ":\n"[i == 1]);
        fprintf(stderr, "--------------\n");
    }*/
    //fprintf(stderr, "--------------\n");

    tcpHdr_t header;
    header.srcPort = info -> srcPort;
    header.dstPort = info -> dstPort;
    header.seq = changeEndian32(seq);
    header.ack = changeEndian32(ack);
    header.flags = 0x50 | flags;
    header.window = changeEndian16(4096);
    header.others = 0;

    u_char* packet = new u_char[len + sizeof(tcpHdr_t)];
    memcpy(packet, &header, sizeof(tcpHdr_t));
    memcpy(packet + sizeof(tcpHdr_t), buf, len);
    
    *((uint16_t*)&header.others) = getchecksum(packet, len + sizeof(tcpHdr_t));
    memcpy(packet, &header, sizeof(tcpHdr_t));

//sendIPPacket(const struct in_addr src, const struct in_addr dest, 
//              int proto, const void *buf, int len, int ttl);
    struct in_addr src; src.s_addr = info -> srcIP;
    struct in_addr dst; dst.s_addr = info -> dstIP;
    int ret = sendIPPacket(src, dst, IPV4_TCP, packet, len + sizeof(tcpHdr_t), 32);
    
    if (ret < 0) {
        printf("[PANIC]: Fail to send IP Packet!\n");
    }

    delete [] packet;

    
   // if (seq != lastSEQ || ack != lastACK) fprintf(stderr, "TCP send PACKET finished\n");

    return 0;
}

const uint32_t specialIP = 0;

int recvTCPPacket(const void* buf, int len, Info_t Info) {
    std :: unique_lock <std :: mutex> kernelLock(kernel_mutex); 

    /*static int isfirst = 1;
    if (isfirst) {
        isfirst = 0;
        struct timeval t;
        gettimeofday(&t, NULL);
        printf("Receive (%ld.%ld)\n", t.tv_sec, t.tv_usec); fflush(stderr);
    }*/
   
    uint16_t checkflag = getchecksum(buf, len);
    if (checkflag != 0) return -1;

    tcpHdr_t header;
    memcpy(&header, buf, sizeof(tcpHdr_t));
    header.seq = changeEndian32(header.seq);
    header.ack = changeEndian32(header.ack);
    header.window = changeEndian16(header.window);
    
    static uint32_t lastSEQ = 0;
    static uint32_t lastACK = 0;

    
    /*if (ResentFlag && header.seq > 15000 && header.ack > 15000) {
        fprintf(stderr, "TCP recv PACKET\n");
        fprintf(stderr, "seq=%d ack=%d flag=%04x\n", header.seq, header.ack, header.flags);
                uint8_t* ip = (uint8_t*)&Info.srcIP; 
        for (int i = 0; i < 4; ++i) printf("%02x%c", ip[i], ": "[i == 3]);
        uint8_t* port = (uint8_t*)&Info.srcPort; 
        for (int i = 0; i < 2; ++i) printf("%02x%c", port[i], ":\n"[i == 1]);

        ip = (uint8_t*)&Info.dstIP; 
        for (int i = 0; i < 4; ++i) printf("%02x%c", ip[i], ": "[i == 3]);
        port = (uint8_t*)&Info.dstPort; 
        for (int i = 0; i < 2; ++i) printf("%02x%c", port[i], ":\n"[i == 1]);
        fprintf(stderr, "--------------\n");
    }*/
      //  printf("receive an packet\n");
      //  fflush(stderr);

    
    lastSEQ = max(header.seq, lastSEQ), lastACK = max(header.ack, lastACK);

    //Find the corresponding connfd according to Info
    int connfd = -1;
    map <int, socket_t*> *pool = getSocketPool();
    std :: map <int, socket_t*> :: iterator it;

    for (it = pool -> begin(); it != pool -> end(); ++it) {
        socket_t* socket = it -> second;

        if (socket -> info.dstIP != Info.srcIP) continue;
        if (socket -> info.dstPort != Info.srcPort) continue;
        if (socket -> info.srcIP != Info.dstIP) continue;
        if (socket -> info.srcPort != Info.dstPort) continue;
        connfd = it -> first; 
        break;
    } //Have seen the other side before

    if (connfd < 0) { //The first time to see the other side
        for (it = pool -> begin(); it != pool -> end(); ++it) {
            socket_t* socket = it -> second;
            if (socket -> state != STATE_LISTEN) continue;
            if ((socket -> info).srcIP != specialIP && (socket -> info).srcIP != Info.dstIP) continue;
            if ((socket -> info).srcPort != Info.dstPort) continue;
            
            connfd = it -> first; 
        }
    }

    if (connfd < 0) {
        return -1;
    }
    
   // printf("# connfd = %d\n", connfd);

    socket_t* mysocket = (pool -> find(connfd)) -> second;
    if (mysocket -> state == STATE_LISTEN) { //you are a sever
       // printf("For Server.....................\n");
       // fflush(stderr);

        swap(Info.srcIP, Info.dstIP);
        swap(Info.srcPort, Info.dstPort);

        set <tryConn_t*> :: iterator it = mysocket -> connTrying_set.begin();
        tryConn_t* my_tryConn = NULL;
        for (; it != mysocket -> connTrying_set.end(); ++it) {
            tryConn_t* tryConn = *it;
            if (tryConn -> dstIP != Info.dstIP || tryConn -> dstPort != Info.dstPort) continue;
            if (tryConn -> srcIP != Info.srcIP || tryConn -> srcPort != Info.srcPort) continue;
            my_tryConn = tryConn;
            //printf("# find tryConn\n");
            break;    
        }

        if (my_tryConn == NULL) { 
            if (mysocket -> connTrying_set.size() < mysocket -> backlog) {
                my_tryConn = new tryConn_t();
    
                my_tryConn -> srcIP = Info.srcIP;
                my_tryConn -> srcPort = Info.srcPort;
                my_tryConn -> dstIP = Info.dstIP;
                my_tryConn -> dstPort = Info.dstPort;
                my_tryConn -> connState = STATE_LISTEN; 
                mysocket -> connTrying_set.insert(my_tryConn);
            }
            else { // Drop this packet
                return -1;
            }
        }
    
        if (isSYN(header.flags) && !isACK(header.flags)) {
            if (my_tryConn -> connState == STATE_LISTEN || my_tryConn -> connState == STATE_SYNRCVD) { 
                unsigned char* buf = new unsigned char [1];
                sendTCPPacket(buf, 1, &Info, 0, 1, setSYN(setACK(0)));
                delete [] buf;

                my_tryConn -> synReceive = true;
                my_tryConn -> connState = STATE_SYNRCVD;
            }
        }
        if (isACK(header.flags) || header.seq == 1 && header.ack == 1) {
           // printf("----------------------------Come to here. Hear an ACK!--------------------------------");
           // printf("%d\n", my_tryConn -> connState);
           // fflush(stderr);
            if (my_tryConn -> connState == STATE_SYNRCVD) {
                my_tryConn -> ackReceive = true;
                my_tryConn -> connState = STATE_ESTABLISHED;
                kernel_cv.notify_all();
            }
        }
            
        return 0;
    }
    

    //printf("FOR CLIENT...................\n");
    //for CLIENT
    Info_t sendInfo = Info;
    swap(sendInfo.dstIP, sendInfo.srcIP);
    swap(sendInfo.dstPort, sendInfo.srcPort);
    if (isSYN(header.flags) && isACK(header.flags) && mysocket -> state == STATE_SYNSENT) {
        //sendTCPPacket(void* buf, int len, Info_t* info, uint32_t seq, uint32_t ack, int16_t flags);
        //printf("??????11111111111????????????????\n");
        //fflush(stderr);
        unsigned char* buf = new unsigned char [1];
        sendTCPPacket(buf, 1, &sendInfo, header.ack, header.seq + 1, setACK(0));
        delete [] buf;

        mysocket -> state = STATE_ESTABLISHED;

        mysocket -> connectReady = true;
        kernel_cv.notify_all();
        
        return 0;
    } 
/*
    if (isSYN(header.flags) && !isACK(header.flags) && mysocket -> state == STATE_SYNSENT) {
        //printf("??????22222222222????????????????\n");
        //fflush(stderr);
        unsigned char* buf = new unsigned char [1];
        sendTCPPacket(buf, 1, &sendInfo, header.ack, header.seq + 1, setSYN(setACK(0)));
        delete [] buf;

        mysocket -> state = STATE_SYNRCVD;
        return 0;
    }

    if (isACK(header.flags) && mysocket -> state == STATE_SYNRCVD) {
        //printf("??????333333333333????????????????\n");
        //fflush(stderr);
        mysocket -> state = STATE_ESTABLISHED;
        //Notify()
        mysocket -> connectReady = true;
        kernel_cv.notify_all();
        return 0;
    }*/
    //printf("QWQ\n");
    //fflush(stderr);

    if (isFIN(header.flags) && (mysocket -> state == STATE_ESTABLISHED || mysocket -> state == STATE_CLOSEWAIT)) {
        if (header.seq != mysocket -> szep) return 0;
        if (header.ack != mysocket -> writep) return 0;
        //printf("1111111111111111111111111111111111111111\n");
        //fflush(stderr);
        unsigned char* buf = new unsigned char [1];
        sendTCPPacket(buf, 1, &sendInfo, header.ack, header.seq + 1, setACK(0));
        delete [] buf;

        mysocket -> state = STATE_CLOSEWAIT;
        kernel_cv.notify_all();
        return 0;
    }
     
    if (isFIN(header.flags) && (mysocket -> state == STATE_FINWAIT1 || mysocket -> state == STATE_CLOSING)) {
        if (header.seq != mysocket -> szep) return 0;
        if (header.ack != mysocket -> writep) return 0;
        //printf("22222222222222222222222222222222222222\n");
        //fflush(stderr);
        unsigned char* buf = new unsigned char [1];
        sendTCPPacket(buf, 1, &sendInfo, header.ack, header.seq + 1, setACK(0));
        delete [] buf;

        mysocket -> state = STATE_CLOSING;
        kernel_cv.notify_all();
        return 0;
    }

    if (isFIN(header.flags) && (mysocket -> state == STATE_FINWAIT2 || mysocket -> state == STATE_TIMEWAIT)) {
        if (header.seq != mysocket -> szep) return 0;
        if (header.ack != mysocket -> writep + 1) return 0;
        //printf("33333333333333333333333333333333333333333\n");
        //fflush(stderr);
        unsigned char* buf = new unsigned char [1];
        sendTCPPacket(buf, 1, &sendInfo, header.ack, header.seq + 1, setACK(0));
        delete [] buf;

        mysocket -> state = STATE_TIMEWAIT;
        kernel_cv.notify_all();
        return 0;
    }


    if (isACK(header.flags) && mysocket -> state == STATE_FINWAIT1) {
        if (header.seq != mysocket -> szep) return 0;
        if (header.ack != mysocket -> writep + 1) return 0;
        //printf("44444444444444444444444444444444444444\n");
        //fflush(stderr);
        mysocket -> state = STATE_FINWAIT2;
        kernel_cv.notify_all();
        return 0;
    }

    if (isACK(header.flags) && mysocket -> state == STATE_CLOSING) {
        if (header.seq != mysocket -> szep) return 0;
        if (header.ack != mysocket -> writep + 1) return 0;
        //printf("55555555555555555555555555555555555555555\n");
        //fflush(stderr);
        mysocket -> state = STATE_TIMEWAIT;
        kernel_cv.notify_all();
        return 0;
    }

    if (isACK(header.flags) && mysocket -> state == STATE_LASTACK) {
        if (header.seq != mysocket -> szep + 1) return 0;
        if (header.ack != mysocket -> writep + 1) return 0;
        //printf("6666666666666666666666666666666666666666\n");
        //fflush(stderr);
        mysocket -> state = STATE_CLOSED;
        kernel_cv.notify_all();
        return 0;
    }
    //printf("MYACK=[%d], MYSEQ=[%d]\n", mysocket -> szep, mysocket -> writep);

    if (mysocket -> state == STATE_ESTABLISHED) { //For non-special case...
        //update writep
        if (mysocket -> writep < header.ack) {
            mysocket -> writep = header.ack;
            kernel_cv.notify_all();
        }

        if (isACK(header.flags)) return 0;
        //automatically send ACK
        bool update = false;
        if (mysocket -> szep == header.seq && mysocket -> writep == header.ack) {
            size_t length = len - sizeof(tcpHdr_t);
            if (mysocket -> szep - mysocket -> readp + length < SOCK_BUFFER_SIZE) {
                update = true;
                int posS = mysocket -> szep % SOCK_BUFFER_SIZE;
                if (posS + length <= SOCK_BUFFER_SIZE) 
                    memcpy(mysocket -> buffer + posS, buf + sizeof(tcpHdr_t), length);
                else {
                    size_t l1 = SOCK_BUFFER_SIZE - posS;
                    memcpy(mysocket -> buffer + posS, buf + sizeof(tcpHdr_t), l1);
                    size_t l2 = length - l1;
                    memcpy(mysocket -> buffer, buf + sizeof(tcpHdr_t) + l1, l2);
                }
                mysocket -> szep += len - sizeof(tcpHdr_t);
            }
        }

        /*int curszep = header.seq + len - sizeof(tcpHdr_t);
        int curwritep = header.ack;
        if (curszep == mysocket -> szep && curwritep == mysocket -> writep) {
            unsigned char* sendMessage = new unsigned char [1];
            sendTCPPacket(sendMessage, 1, &sendInfo, mysocket -> writep, mysocket -> szep, setACK(0));
        }*/

        if (update || ARQflag) {
            ARQflag = false;
            unsigned char* sendMessage = new unsigned char [1];
            sendTCPPacket(sendMessage, 1, &sendInfo, mysocket -> writep, mysocket -> szep, setACK(0));
        }

        if (update) kernel_cv.notify_all();
        return 0;
    }


/* Print out Template
 /* fprintf(stderr, "TCP recv PACKET\n");
        fprintf(stderr, "seq=%d ack=%d flag=%04x\n", header.seq, header.ack, header.flags);
                uint8_t* ip = (uint8_t*)&Info.srcIP; 
        for (int i = 0; i < 4; ++i) printf("%02x%c", ip[i], ": "[i == 3]);
        uint8_t* port = (uint8_t*)&Info.srcPort; 
        for (int i = 0; i < 2; ++i) printf("%02x%c", port[i], ":\n"[i == 1]);

        ip = (uint8_t*)&Info.dstIP; 
        for (int i = 0; i < 4; ++i) printf("%02x%c", ip[i], ": "[i == 3]);
        port = (uint8_t*)&Info.dstPort; 
        for (int i = 0; i < 2; ++i) printf("%02x%c", port[i], ":\n"[i == 1]);
        fprintf(stderr, "--------------\n");*/
      //  printf("receive an packet\n");
      //  fflush(stderr);

    return 0;

}