#include "mylock.h"
#include "device.h"
#include "packetio.h"
#include "ip.h"
#include "routing.h"
#include "tcp.h"
#include "socket.h"
#include "init.h"
#include <unistd.h>
#include <arpa/inet.h>

#ifndef _MAIN_
#define _MAIN_

int main() {
	InitNS();
	sleep(8000000);
}

#endif