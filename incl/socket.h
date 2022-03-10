/**
* @file socket.h
* @brief POSIX-compatible socket library supporting TCP protocol on IPv4.
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "tcp.h"
#include <bits/stdc++.h>
using namespace std;

#ifndef _SOCKET_H_
#define _SOCKET_H_

#define ROLE_CLIENT     1
#define ROLE_SEVER      0

#define STATE_CLOSED        0
#define STATE_LISTEN        1
#define STATE_SYNRCVD       2
#define STATE_SYNSENT       3
#define STATE_ESTABLISHED   4
#define STATE_FINWAIT1      5
#define STATE_FINWAIT2      6
#define STATE_TIMEWAIT      7
#define STATE_CLOSING       8
#define STATE_CLOSEWAIT     9
#define STATE_LASTACK       10

#define SOCK_TYPE_STREAM    1

#define SOCK_BUFFER_SIZE (1 << 18)

struct tryConn_t {
    int synReceive;
    int ackReceive;
    int connState;
    uint32_t srcIP;
    uint16_t srcPort;
    uint32_t dstIP;
    uint16_t dstPort;
    tryConn_t () {
        synReceive = false;
        ackReceive = false;
        connState = STATE_LISTEN;
    }
};


struct socket_t {
    unsigned char* buffer;  // for receiving
    int readp;              //next read position
    int szep;               //pos of the last received byte

    int writep;             //next write position 

    int domain;
    int type; 
    int protocol;
    
    Info_t info; //port是主机序的

    int index;              //corresponding fd number
    int role; // CLIENT or SEVER
    int state; 
    int backlog;

    bool connectReady;
    //std :: mutex connect_mutex;
    //std :: condition_variable connect_cv; 

    set <tryConn_t*> connTrying_set;

    socket_t() {
        buffer = new unsigned char [SOCK_BUFFER_SIZE];
        readp = writep = szep = 0;
        domain = type = protocol = 0;
        index = -1, role = state = -1, backlog = 1;
        connectReady = false; 
        connTrying_set.clear();
    }
    socket_t(const socket_t* x) { 
        buffer = new unsigned char [SOCK_BUFFER_SIZE]; memcpy(buffer, x -> buffer, SOCK_BUFFER_SIZE);
        readp = x -> readp, writep = x -> writep, szep = x -> szep;
        domain = x -> domain, type = x -> type, protocol = x -> protocol;
        info = x -> info;
        index = x -> index, role = x -> role, state = x -> state, backlog = x -> state;
        connectReady = x -> connectReady;
        connTrying_set.clear();
    }
    ~socket_t() {

    }
};

map <int, socket_t*> *getSocketPool();


/**
* @see [POSIX.1-2017:socket](http://pubs.opengroup.org/onlinepubs/9699919799/functions/socket.html )
*/
extern "C" {
    int __wrap_socket(int domain, int type, int protocol);
}
int __my_socket(int domain, int type, int protocol);

/**
* @see [POSIX.1-2017:bind](http://pubs.opengroup.org/onlinepubs/9699919799/functions/bind.html)
*/
extern "C" {
    int __wrap_bind(int socket, const struct sockaddr* address, socklen_t address_len);
}
int __my_bind(int socket, const struct sockaddr* address, socklen_t address_len);

/**
* @see [POSIX.1-2017:listen](http://pubs.opengroup.org/onlinepubs/9699919799/functions/listen.html)
*/
extern "C" {
    int __wrap_listen(int socket, int backlog);
}
int __my_listen(int socket, int backlog);

/**
* @see [POSIX.1-2017:connect](http://pubs.opengroup.org/onlinepubs/9699919799/functions/connect.html)
*/
extern "C" {
    int __wrap_connect(int socket, const struct sockaddr* address, socklen_t address_len);
}
int __my_connect(int socket, const struct sockaddr* address, socklen_t address_len);


/**
* @see [POSIX.1-2017:accept](http://pubs.opengroup.org/onlinepubs/9699919799/functions/accept.html)
*/
extern "C" {
    int __wrap_accept(int socket, struct sockaddr* address , socklen_t * address_len );
}
int __my_accept(int socket, struct sockaddr* address , socklen_t * address_len );


/**
* @see [POSIX.1-2017:read](http://pubs.opengroup.org/onlinepubs/9699919799/functions/read.html)
*/
extern "C"{
    ssize_t __wrap_read(int fildes, void* buf, size_t nbyte);
}
ssize_t __my_read(int fildes, void* buf, size_t nbyte);

/**
* @see [POSIX.1-2017:write](http://pubs.opengroup.org/onlinepubs/9699919799/functions/write.html)
*/
extern "C"{
ssize_t __wrap_write(int fildes, const void* buf, size_t nbyte);
}
ssize_t __my_write(int fildes, const void* buf, size_t nbyte);

/**
* @see [POSIX.1-2017:close](http://pubs.opengroup.org/onlinepubs/9699919799/functions/close.html)
*/
extern "C"{
int __wrap_close(int fildes);
}
int __my_close(int fildes);

/**
* @see [POSIX.1-2017:getaddrinfo](http://pubs.opengroup.org/onlinepubs/9699919799/functions/getaddrinfo.html)
*/
extern "C"{
int __wrap_getaddrinfo(const char* node, const char* service, const struct addrinfo* hints, struct addrinfo** res);
}
int __my_getaddrinfo(const char* node, const char* service, const struct addrinfo* hints, struct addrinfo** res);

#endif