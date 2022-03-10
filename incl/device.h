#include <bits/stdc++.h>
#include <pcap/pcap.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include "mylock.h"
using namespace std;

/**
* @file device.h
* @brief Library supporting network device management.
*/

#ifndef _DEVICE_H_
#define _DEVICE_H_

#define MAX_NAME_LENGTH       50
#define MAX_Device_NUM		   500		// MAX_Device_NUM
#define WAITING_TIME          10    //wating time per packets(ms)
#define MY_BUF_SIZE        65536

class Device {
  //private:
  public:  
    char* name;
    int32_t id;
    unsigned char MAC[8];
    uint32_t IP;
    pcap_t* handle;

	Device();
    Device(const char* _name, const int32_t& _id, bool& flag);
    ~Device();
};

class DeviceList {
  public:
    DeviceList* nxt;
    Device* cur;
    DeviceList();
};

//Detect all devices on the host and return their names.
vector <string> detectAllDevice(); 

//use addDevice to add all available devices to the library
void addAlldevices();

/**
* Add a device to the library for sending/receiving packets.
*
* @param it information of network device to send/receive packet on.
* @return A non-negative _device-ID_ on success, -1 on error.
*/
int addDevice(const pcap_if_t* it);

/**
* Find a device added by `addDevice`.
*
* @param device Name of the network device.
* @return A non-negative _device-ID_ on success, -1 if no such device was found.
*/
int findDevice(const char* device);

//show the devices added to the library(name and id)
void showDevices();

Device* getDevice(int id);
Device* getDevicefromMAC(const char* mac);
int getDevNum();
DeviceList* getDevlist();
#endif
