#include "mylock.h"
#include "device.h"
#include "packetio.h"
#include "ip.h"
#include "routing.h"
#include "tcp.h"
#include "socket.h"
#include <unistd.h>
#include <arpa/inet.h>

#ifndef _SEVER_
#define _SEVER_

int main() {
	addAlldevices();
	
	//Check Point1 show opened device
	showDevices();
	fflush(stderr);

	initOutputConsole();
	initRoutingTable();
	
	pthread_t routingtid;
	void* arg = NULL;
	pthread_create(&routingtid, NULL, routingThread, arg);

	for (int i = 0, n = getDevNum(); i < n; ++i) {
		Device* ndevice = getDevice(i);
		if (memcmp(ndevice -> name, "any", 3) == 0 || ndevice -> IP == 0) continue;

		setFrameReceiveCallback(i, recvFrame);
	}
	setIPPacketReceiveCallback(recvPacket);

	sleep(9);
 	
    uint8_t srcip[4] = {0x0a, 0x64, 0x03, 0x02};
	uint16_t srcport = 80;
    //int sendTCPPacket(void* buf, int len, Info_t* info, uint32_t seq, uint32_t ack, int16_t flags);
    //sendTCPPacket(payload, 1, &info, 0, 0, setSYN(0));

//Check Point 
    int listenfd = __wrap_socket(AF_INET, SOCK_STREAM, 0);
    //__wrap_bind(int socket, const struct sockaddr *address, socklen_t address_len)
	struct sockaddr_in* addr = new sockaddr_in();
	addr -> sin_family = AF_INET;
	addr -> sin_port = changeEndian16(srcport); //网络字节序
	addr -> sin_addr.s_addr = ptr2ip(srcip);
	//senderOutputAsking();
    __wrap_bind(listenfd, (struct sockaddr *)addr, sizeof(sockaddr_in));
    __wrap_listen(listenfd, 2);

    struct sockaddr* dstaddr = new sockaddr();
    socklen_t addrlen;
    int connfd = __wrap_accept(listenfd, dstaddr, &addrlen);
	uint32_t dstip = ((struct sockaddr_in*)dstaddr) -> sin_addr.s_addr; 
	uint8_t *dip = (uint8_t *)&dstip;
	
	for (int i = 0; i < 4; ++i) 
		printf("%02x%c", dip[i], ":\n"[i == 3]);
	fflush(stderr);

	char* text = new char [100];
	//ssize_t __wrap_read(int fildes, void *buf, size_t nbyte)
	__wrap_read(connfd, text, 10);
	for (int i = 0; i < 10; ++i) printf("%c", text[i]); 
	printf("\n");
	fflush(stderr);

	//senderOutputFinished();
	sleep(1000);
}

#endif