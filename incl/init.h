#include "mylock.h"
#include "device.h"
#include "packetio.h"
#include "ip.h"
#include "routing.h"
#include "tcp.h"
#include "socket.h"
#include "timer.h"
#include <unistd.h>
#include <arpa/inet.h>

#ifndef _INIT_H_
#define _INIT_H_

void InitNS();

#endif