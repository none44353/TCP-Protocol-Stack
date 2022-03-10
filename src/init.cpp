#include "init.h"

void InitNS() {
    addAlldevices();

	//Check Point1 show opened device
	showDevices();
	fflush(stderr);

	initMyLock();
	initRoutingTable();
	
	pthread_t routingtid;
	void* arg = NULL;
	pthread_create(&routingtid, NULL, routingThread, arg);

	pthread_t Timer_tid;
	void* Timer_arg = NULL;
	pthread_create(&Timer_tid, NULL, TimerThread, Timer_arg);

	for (int i = 0, n = getDevNum(); i < n; ++i) {
		Device* ndevice = getDevice(i);
		if (memcmp(ndevice -> name, "any", 3) == 0 || ndevice -> IP == 0) continue;
		printf("%s\n", ndevice -> name);
		setFrameReceiveCallback(i, recvFrame);
	}
	setIPPacketReceiveCallback(recvPacket);

    return;
}
