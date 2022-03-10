#include "socket.h"
#include "tcp.h"
#include "routing.h"
#include "device.h"
#include "init.h"
#include <fcntl.h>
#include <unistd.h>
#include <condition_variable>
#include <sys/time.h>

const double rGAP = 0.07;

bool ResentFlag = false;
const int MAGIC_NUM = 100;

map <int, socket_t*> socket_pool;
set <int> port_pool; // all used port number are here

//每个host上有一个独立的kernel锁，表示host上不会出现socket操作的并行
std :: mutex kernel_mutex;
std :: condition_variable kernel_cv; 

map <int, socket_t*> *getSocketPool() {
    return &socket_pool;
}

int validSocket(int fildes) {
    for (auto it = socket_pool.begin(); it != socket_pool.end(); it++){
        if (it -> first == fildes){
            return true;
        }
    }
    return false;
}

extern "C"{
    int __real_close(int fildes);
    ssize_t __real_read(int fildes, void *buf, size_t nbyte);
    ssize_t __real_write(int fildes, const void *buf, size_t nbyte);

    int __wrap_socket(int domain, int type, int protocol){
        return __my_socket(domain, type, protocol);
    }
    int __wrap_bind(int socket, const struct sockaddr* address, socklen_t address_len){
        return __my_bind(socket, address, address_len);
    }
    int __wrap_listen(int socket, int backlog){
        return __my_listen(socket, backlog);
    }
    int __wrap_connect(int socket, const struct sockaddr* address, socklen_t address_len){
        return __my_connect(socket, address, address_len);
    }
    int __wrap_accept(int socket, struct sockaddr* address , socklen_t * address_len ){
        return __my_accept(socket, address, address_len);
    }
    int __wrap_close(int fildes){
        if (validSocket(fildes)) return __my_close(fildes);
        else return __real_close(fildes);
    }
    ssize_t __wrap_read(int fildes, void *buf, size_t nbyte){
        if (validSocket(fildes)) return __my_read(fildes, buf, nbyte);
        else return __real_read(fildes, buf, nbyte);
    }
    ssize_t __wrap_write(int fildes, const void *buf, size_t nbyte){
        if (validSocket(fildes)) return __my_write(fildes, buf, nbyte);
        else return __real_write(fildes, buf, nbyte);
    }
    int __wrap_getaddrinfo(const char* node, const char* service, const struct addrinfo* hints, struct addrinfo** res){
        return __my_getaddrinfo(node, service, hints, res);
    }
}

double getTimeval(timeval* endT, timeval* startT) {
    double second = (double)endT -> tv_sec - (double)startT -> tv_sec;
    second += ((double)endT -> tv_usec - (double)startT -> tv_usec) / 1000000.0;
    return second;
}

int __my_socket(int domain, int type, int protocol) {
    std :: unique_lock <std :: mutex> kernelLock(kernel_mutex); 
    ResentFlag = true;  
    //printf("[Info]: CALL MY SOCKET\n"); fflush(stderr);
    static bool visFlag = false;
    if (!visFlag) {
        InitNS();
        visFlag = true;
    }

    int fd = 0;
    for (; fd < 256; ++fd) {
        if (socket_pool.find(fd) != socket_pool.end()) continue;
        //find a free fd and create a new socket
        break;
    }

    socket_t* mySocket = new socket_t();
    mySocket -> domain = domain;
    mySocket -> type = type;
    mySocket -> protocol = protocol;
    mySocket -> index = fd;

    //put this socket and file descriptor to the pool
    socket_pool.insert(make_pair(fd, mySocket));

    //printf("[Info]: FINISH MY SOCKET\n"); fflush(stderr);
    return fd;
}

bool bindOK(const struct sockaddr *address, socklen_t address_len) {
    if (address -> sa_family == AF_INET) {
        struct sockaddr_in* addr = (struct sockaddr_in*)address;
        if (port_pool.find(addr -> sin_port) != port_pool.end()) {
            return false;
        }
    }

    return true;
}

int __my_bind(int socket, const struct sockaddr *address, socklen_t address_len) {
    std :: unique_lock <std :: mutex> kernelLock(kernel_mutex);   
    //printf("[Info]: CALL MY BIND\n"); fflush(stderr);
    if (socket_pool.find(socket) == socket_pool.end()) {
        fprintf(stderr, "[Error]: can not find this socket!\n");
        return -1;
    }

    if (!bindOK(address, address_len)) {
        return -1;
    }

    socket_t* mysocket = socket_pool[socket];
    if (mysocket -> role != -1) {
        fprintf(stderr, "[Error]: can not bind this socket to address\n");
        return -1;
    }

    mysocket -> role = ROLE_SEVER;
    mysocket -> state = STATE_CLOSED;
    
    struct sockaddr_in* addr = (struct sockaddr_in*)address;
    mysocket -> info.srcIP = addr -> sin_addr.s_addr;
    mysocket -> info.srcPort = addr -> sin_port;
    port_pool.insert(((sockaddr_in*)address) -> sin_port);

   // printf("[Info]: FINISH MY BIND\n"); fflush(stderr);
    return 0;
}

int __my_listen(int socket, int backlog) {
    std :: unique_lock <std :: mutex> kernelLock(kernel_mutex);   
    
   // printf("[Info]: CALL MY LISTEN\n"); fflush(stderr);
    if (socket_pool.find(socket) == socket_pool.end()) {
        fprintf(stderr, "[Error]: can not find this socket!\n");
        return -1;
    }
    socket_t* mysocket = socket_pool[socket];
    
    if (backlog > 5) backlog = 5;
    if (backlog <= 0) backlog = 1;
    mysocket -> backlog = backlog; 
    mysocket -> state = STATE_LISTEN;
    //printf("[Info]: FINISH MY LISTEN\n"); fflush(stderr);
    return 0;
}

int __my_connect(int socket, const struct sockaddr *address, socklen_t address_len) {
    std :: unique_lock <std :: mutex> kernelLock(kernel_mutex);   

    //printf("[Info]: CALL MY CONNECT\n"); fflush(stderr);
    if (socket_pool.find(socket) == socket_pool.end()) {
        fprintf(stderr, "[Error]: can not find this socket!\n");
        return -1;
    }
    socket_t* mysocket = socket_pool[socket];

    Info_t info;
    struct sockaddr_in* dstaddr = (struct sockaddr_in*)address;
    struct in_addr dest; dest = dstaddr -> sin_addr;
    struct in_addr mask; mask.s_addr = 0xffffffff;
    
    ReaderIn();
    vector <nxtHop> nxtHops = lookForRouting(dest, mask);
    ReaderOut();
    
    if (nxtHops.size() == 0) {
        fprintf(stderr, "[Error]: can not connect to this address!\n");
        return -1;
    }
    Device* srcDevice = NULL;  
    for (int i = 0, sze = nxtHops.size(); i < sze; ++i) {
        int deviceID = nxtHops[i].deviceID;
        Device* device = getDevice(deviceID);		
        if (memcmp(device -> name, "any", 3) == 0 || device -> IP == 0) continue;
        srcDevice = device; 
        break;
    }
    
//set Destination
    info.dstIP = dest.s_addr;
    info.dstPort = dstaddr -> sin_port;
//set Source
    if (srcDevice == NULL) return -1;
    int portNo = -1;
    while (portNo < 0 || port_pool.find(portNo) != port_pool.end()) {
        portNo = rand() % MAX_PORT_NUMBER;
    }
    info.srcIP = srcDevice -> IP;
    info.srcPort =  portNo;
    port_pool.insert(portNo);

    mysocket -> role = ROLE_CLIENT;
    mysocket -> state = STATE_SYNSENT;
    memcpy(&(mysocket -> info), &info, sizeof(Info_t));

    unsigned char* payload = new unsigned char[1];
    payload[0] = 'a';
    //int sendTCPPacket(void* buf, int len, Info_t* info, uint32_t seq, uint32_t ack, int16_t flags);
    
    struct timeval lst_ts, cur_ts;
    gettimeofday(&lst_ts, NULL);
    sendTCPPacket(payload, 1, &info, 0, 0, setSYN(0));
    
    //wait until the connection is set up
    while (!(mysocket -> connectReady)) {
        gettimeofday(&cur_ts, NULL);
        double delta = getTimeval(&cur_ts, &lst_ts);
        if (delta > rGAP) {
            sendTCPPacket(payload, 1, &info, 0, 0, setSYN(0));
            lst_ts = cur_ts;
        }
        kernel_cv.wait(kernelLock);
    }
    delete [] payload;
    mysocket -> readp = 1;
    mysocket -> writep = 1;
    mysocket -> szep = 1;
    //fprintf(stderr, "[INFO]: Connection established!");

    //printf("[Info]: FINISH MY CONNECT\n"); fflush(stderr);
    return 0;
}

bool getAddr(socket_t* socket, struct sockaddr *address, socklen_t *address_len, Info_t* info) {
    set <tryConn_t*> :: iterator it = (socket -> connTrying_set).begin();
    for (; it != (socket -> connTrying_set).end(); ++it) {
        tryConn_t* myConn = *it;
        
        if (myConn -> connState == STATE_ESTABLISHED) {
            struct sockaddr_in* addr = new struct sockaddr_in();
            addr -> sin_family = AF_INET;
            addr -> sin_port = myConn -> dstPort;          
            addr -> sin_addr.s_addr = myConn -> dstIP;

            info -> srcIP = myConn -> srcIP;
            info -> srcPort = myConn -> srcPort;
            info -> dstIP = myConn -> dstIP;
            info -> dstPort = myConn -> dstPort;

            (*address_len) = (socklen_t)sizeof(struct sockaddr_in);
            memcpy(address, addr, sizeof(struct sockaddr_in));

            socket -> connTrying_set.erase(it);
            delete addr;
            return true;
        }
    }

    return false;
}

int __my_accept(int socket, struct sockaddr *address, socklen_t *address_len) {
    std :: unique_lock <std :: mutex> kernelLock(kernel_mutex);   
    //printf("[Info]: CALL MY ACCEPT\n"); fflush(stderr);
    if (socket_pool.find(socket) == socket_pool.end()) {
        fprintf(stderr, "[Error]: can not find this socket!\n");
        return -1;
    }
    socket_t* mysocket = socket_pool[socket];
    Info_t info;

    while (!getAddr(mysocket, address, address_len, &info)) kernel_cv.wait(kernelLock);
    
    
    //build a new socket
    int fd = 0;
    for (; fd < 256; ++fd) {
        if (socket_pool.find(fd) != socket_pool.end()) continue;
        //find a free fd and create a new socket
        break;
    }
    socket_t* newsocket = new socket_t(mysocket);
    newsocket -> info = info;
    /*newsocket -> info.srcIP = info.srcIP;
    newsocket -> info.srcPort = info.srcPort;
    newsocket -> info.dstIP = info.dstIP;
    newsocket -> info.dstPort = info.dstPort;*/
    newsocket -> role = ROLE_CLIENT;
    newsocket -> state = STATE_ESTABLISHED;
    newsocket -> connTrying_set.clear();
    newsocket -> readp = 1;
    newsocket -> writep = 1;
    newsocket -> szep = 1;
    //put this socket and file descriptor to the pool
    socket_pool.insert(make_pair(fd, newsocket));
   /* 
    uint8_t* _ip = (uint8_t*)&(newsocket -> info.srcIP); 
    for (int i = 0; i < 4; ++i) printf("%02x%c", _ip[i], ": "[i == 3]);
    uint8_t* _port = (uint8_t*)&(newsocket -> info.srcPort); 
    for (int i = 0; i < 2; ++i) printf("%02x%c", _port[i], ":\n"[i == 1]);

    _ip = (uint8_t*)&(newsocket -> info.dstIP); 
    for (int i = 0; i < 4; ++i) printf("%02x%c", _ip[i], ": "[i == 3]);
    _port = (uint8_t*)&(newsocket -> info.dstPort); 
    for (int i = 0; i < 2; ++i) printf("%02x%c", _port[i], ":\n"[i == 1]);*/

    //printf("#### socketAddr=%08x\n", newsocket);
    //fprintf(stderr, "[INFO]: Accept a connection request!");
    //printf("[Info]: FINISH MY ACCEPT\n"); fflush(stderr);
    return fd;
}

ssize_t __my_read(int fildes, void *buf, size_t nbyte) {
    std :: unique_lock <std :: mutex> kernelLock(kernel_mutex); 
   // printf("CALL MYREAD(), want to receive [%d] bytes.\n", nbyte);
    if (socket_pool.find(fildes) == socket_pool.end()) {
        fprintf(stderr, "[Error]: can not find this socket.\n");
        return -1;
    }

    socket_t* mysocket = socket_pool[fildes];

      /* if (mysocket -> szep > 15000 || mysocket -> writep > 15000) {
            printf("CALL MYREAD(), STATE = %d\n", mysocket -> state);
            printf("[READP]=%d [SZEP]=%d\n", mysocket -> readp, mysocket -> szep);
            fflush(stderr);
        }*/

    int counter = 0;
    while (mysocket -> readp == mysocket -> szep && mysocket -> state == STATE_ESTABLISHED) {
      /*  if (mysocket -> szep > 15000 || mysocket -> writep > 15000) {
            printf("CALL MYREAD(), STATE = %d\n", mysocket -> state);
            printf("[READP]=%d [SZEP]=%d\n", mysocket -> readp, mysocket -> szep);
            fflush(stderr);
        }*/
        if (++counter >= MAGIC_NUM) break;
        kernel_cv.wait(kernelLock);
    }

    if (counter >= MAGIC_NUM) {
        fprintf(stderr, "[Error]: Connection Error\n");
        return -1;
    }

    /*if (mysocket -> szep > 15000 || mysocket -> writep > 15000) {
        printf("MYREAD() FINISH WAITING, STATE = %d\n", mysocket -> state);
        printf("[READP]=%d [SZEP]=%d\n", mysocket -> readp, mysocket -> szep);
        fflush(stderr);
    }*/
    if (mysocket -> state != STATE_ESTABLISHED && mysocket -> readp == mysocket -> szep) {
        sleep(1);
        if (mysocket -> readp == mysocket -> szep) return 0;
    }

    size_t res = mysocket -> szep - mysocket -> readp;
    size_t len = min(nbyte, res);
    
    size_t posR = mysocket -> readp % SOCK_BUFFER_SIZE;
    if (posR + len <= SOCK_BUFFER_SIZE) 
        memcpy(buf, mysocket -> buffer + posR, len);
    else {
        size_t l1 = SOCK_BUFFER_SIZE - posR;
        memcpy(buf, mysocket -> buffer + posR, l1);
        size_t l2 = len - l1;
        memcpy(buf + l1, mysocket -> buffer, l2);
    }

    mysocket -> readp += len;

    //printf("[Info]: FINISH MYREAD(), [%u] bytes have been read\n", len);
    return len;
}

ssize_t __my_write(int fildes, const void *buf, size_t nbyte) {
    std :: unique_lock <std :: mutex> kernelLock(kernel_mutex); 
    //printf("CALL MYWRITE(), want to send [%d] bytes.\n", nbyte);
    //fflush(stderr);
    ResentFlag = true;
    struct timeval lst_ts, cur_ts;

    if (socket_pool.find(fildes) == socket_pool.end()) {
        fprintf(stderr, "[Error]: can not find this socket.\n");
        return -1;
    }

    socket_t* mysocket = socket_pool[fildes];
    if (mysocket -> state != STATE_ESTABLISHED) {
        fprintf(stderr, "[Error]: Connection Error.\n");
        return -1;
    }

    size_t start_writep = mysocket -> writep;
    for (size_t i = 0, len; i < nbyte; i += len) {
        len = min((size_t)1400, (int)nbyte - i);
        //sendTCPPacket(void* buf, int len, Info_t* info, uint32_t seq, uint32_t ack, int16_t flags);
        int nxtWrtp = mysocket -> writep + len;
        
        gettimeofday(&lst_ts,NULL);
        //printf("(%ld.%ld) [SZEP]=%d, [WRTP]=%d [NWRTP]=%d\n",lst_ts.tv_sec, lst_ts.tv_usec, mysocket -> szep, mysocket -> writep, nxtWrtp); fflush(stderr);
        int res = sendTCPPacket(buf + i, len, &(mysocket -> info), mysocket -> writep, mysocket -> szep, (uint16_t)0);
        /*if (res < 0) {
            fprintf(stderr, "[Error]: fail to send packets.\n");
            return -1;
        }*/
        int counter = 0;
        while (mysocket -> writep != nxtWrtp) {
            //printf("Wake up to check!\n"); fflush(stderr);
            gettimeofday(&cur_ts,NULL);
            double delta = getTimeval(&cur_ts, &lst_ts);
            if (delta > rGAP || res < 0) {
                //printf("resent!\n"); fflush(stderr);
                //printf("(%ld.%ld) delta=%.6lf res=%d\n", cur_ts.tv_sec, cur_ts.tv_usec, delta, res);
                //printf("[SZEP]=%d, [WRTP]=%d\n", mysocket -> szep, mysocket -> writep); fflush(stderr);
                res = sendTCPPacket(buf + i, len, &(mysocket -> info), mysocket -> writep, mysocket -> szep, (uint16_t)0);
                lst_ts = cur_ts;
                ++counter;
            }
            if (counter >= MAGIC_NUM) break;
            kernel_cv.wait(kernelLock);
            //printf("Wake up to check if I can write\n");
            //fflush(stderr);
        }
        if (counter >= MAGIC_NUM) {
            size_t ret = mysocket -> writep - start_writep; 
            fprintf(stderr, "[Error]: Connection Error.\n");
            return ret;
        }
    }

    //printf("[Info]: FINISH MYWRITE(), [%u] bytes have been writen\n", nbyte);
    return nbyte;
}

int __my_close(int fildes) {
    std :: unique_lock <std :: mutex> kernelLock(kernel_mutex);    
    if (socket_pool.find(fildes) == socket_pool.end()) {
        fprintf(stderr, "[Error]: can not find this socket.\n");
        return -1;
    }

    socket_t* mysocket = socket_pool[fildes];
    //printf("[Info]: CALL MY CLOSE, [SEQ]=%d [ACK]=%d\n", mysocket -> writep, mysocket -> szep);(stderr); 
    unsigned char* buffer = new unsigned char[1];
    
    
    struct timeval lst_ts, cur_ts;
    int res;
    gettimeofday(&lst_ts, NULL);
    
    if (mysocket -> state == STATE_CLOSEWAIT) {//Passive close 
        int counter = 1;
        res = sendTCPPacket(buffer, 1, &(mysocket -> info), mysocket -> writep, mysocket -> szep + 1, setFIN(0));
        mysocket -> state = STATE_LASTACK;
        while (mysocket -> state != STATE_CLOSED) {
            gettimeofday(&cur_ts, NULL);
            double delta = getTimeval(&cur_ts, &lst_ts);
            if (delta > rGAP || res < 0) {
                //printf("my close [SEQ]=%d [ACK]=%d\n", mysocket -> writep, mysocket -> szep + 1); fflush(stderr);
                res = sendTCPPacket(buffer, 1, &(mysocket -> info), mysocket -> writep, mysocket -> szep + 1, setFIN(0));
                cur_ts = lst_ts;
                ++counter;
            }
            if (counter < MAGIC_NUM) kernel_cv.wait(kernelLock);
            else break;
        }
    }
    else 
        if (mysocket -> state == STATE_ESTABLISHED) {
            int counter = 1;
            res = sendTCPPacket(buffer, 1, &(mysocket -> info), mysocket -> writep, mysocket -> szep, setFIN(0));        
            mysocket -> state = STATE_FINWAIT1; 
            while (mysocket -> state != STATE_TIMEWAIT && mysocket -> state != STATE_CLOSED) {
                gettimeofday(&cur_ts, NULL);
                double delta = getTimeval(&cur_ts, &lst_ts);
                if (delta > rGAP || res < 0) {
                   // printf("my close [SEQ]=%d [ACK]=%d\n", mysocket -> writep, mysocket -> szep); fflush(stderr);
                    res = sendTCPPacket(buffer, 1, &(mysocket -> info), mysocket -> writep, mysocket -> szep, setFIN(0));
                    cur_ts = lst_ts;
                    ++counter;
                }
                if (counter < MAGIC_NUM) kernel_cv.wait(kernelLock);
                else {
                    mysocket -> state = STATE_CLOSED;
                    break;
                }
            }

            if (mysocket -> state == STATE_TIMEWAIT) usleep(500);
            
        }

    delete [] buffer;
    port_pool.erase(mysocket->info.srcPort);
    socket_pool.erase(fildes);
    delete [] mysocket -> buffer;
    delete mysocket;

    //printf("[Info]: FINISH MY CLOSE\n"); fflush(stderr); 
    return 0;
}

int __my_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints
    , struct addrinfo **res) {
    std :: unique_lock <std :: mutex> kernelLock(kernel_mutex);     
    
    int cnt = 0;
    std::map<int, socket_t*> :: iterator it = socket_pool.begin();
    for (; it != socket_pool.end(); ++it) {
        socket_t* mysocket = it -> second;
        if (memcmp(&(mysocket -> info.srcPort), service, sizeof(uint16_t)) != 0
         || memcmp(&(mysocket -> info.srcIP), node, sizeof(uint32_t)) != 0) continue;

        struct sockaddr_in* curAddr = new struct sockaddr_in();
        curAddr -> sin_family = AF_INET;
        curAddr -> sin_port = mysocket -> info.dstPort;      
        curAddr -> sin_addr.s_addr = mysocket -> info.dstIP;

        struct addrinfo* addrinfo = new struct addrinfo();
        addrinfo -> ai_family = mysocket -> domain;
        addrinfo -> ai_socktype = mysocket -> type;
        addrinfo -> ai_protocol = mysocket -> protocol;
        addrinfo -> ai_addrlen = sizeof(struct sockaddr_in);
        memcpy(addrinfo -> ai_addr, curAddr, sizeof(struct sockaddr_in));

        res[cnt] = addrinfo, ++cnt;

        delete curAddr;
        delete addrinfo;
    }

    struct addrinfo* lstp = NULL; 
    for (int i = cnt - 1; i >= 0; lstp = res[i], --i) res[i] -> ai_next = lstp;
    
    return 0;
}