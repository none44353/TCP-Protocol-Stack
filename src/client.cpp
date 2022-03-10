#include "mylock.h"
#include "device.h"
#include "packetio.h"
#include "ip.h"
#include "routing.h"
#include "tcp.h"
#include "socket.h"
#include <unistd.h>
#include <arpa/inet.h>

#ifndef _CLIENT_
#define _CLIENT_

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

	//fprintf(stderr, "!!!!!!!!!!!!!!!!!!!\n");
	//fflush(stderr);
	sleep(9);
	//fprintf(stderr, "....................\n");
	//fflush(stderr);
	//Check Point 
    uint8_t dstip[4] = {0x0a, 0x64, 0x03, 0x02};
	uint16_t dstport = 80;
    int fd = __wrap_socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in* addr = new sockaddr_in();
	addr -> sin_family = AF_INET;
	addr -> sin_port = changeEndian16(dstport);
	addr -> sin_addr.s_addr = ptr2ip(dstip);
	
	//fprintf(stderr, "????????????????????\n");
	//fflush(stderr);

	//senderOutputAsking();
	fprintf(stderr, "ask for connection\n");
	fflush(stderr);
    __wrap_connect(fd, (struct sockaddr *)addr, sizeof(sockaddr_in));
	fflush(stderr);
	fprintf(stderr, "build connection\n");
	fflush(stderr);
	//senderOutputFinished();
	sleep(18);

	string text = "Hello, My TCP!";
	fprintf(stderr, "Say Hello to the other side\n");
	fflush(stderr);
	__wrap_write(fd, text.c_str(), text.size());

	sleep(1000);
}

#endif