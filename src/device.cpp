#include "device.h"
#include <netinet/ip.h>

char errorInfo[PCAP_ERRBUF_SIZE] = {0};

//Begin: the inner functions of class Device
Device::Device() {
    name = (char*)malloc(MAX_NAME_LENGTH);
    handle = NULL;
}

Device::Device(const char* _name, const int32_t& _id, bool& flag) {
    if (strlen(_name) >= MAX_NAME_LENGTH) {
        strcpy(errorInfo, "The device name is too long!");
        flag = false;
        return;
    }
    name = (char*)malloc(MAX_NAME_LENGTH); strcpy(name, _name);
    id =_id;
    
    handle = pcap_open_live(name, MY_BUF_SIZE, 1, WAITING_TIME, errorInfo);
    //promise = 1, mix mode
    //if WAITING_TIME==0: wait until the packet come
    if(handle == NULL) {
        flag = false;
        return;
    }

    flag = true;
    return;
}

Device::~Device() {
    if (name) free(name);
}
//End: the inner functions of class Device


//Begin: the inner functions of class DeviceList
DeviceList::DeviceList() {
    cur = NULL; nxt = NULL;
}
//End: the inner functions of class DeviceList

int32_t device_num = 0;

DeviceList* devlist_ptr = NULL;

vector <string> detectAllDevice() {
    pcap_if_t *it;
    int ret;

    ret = pcap_findalldevs(&it, errorInfo);
    if(ret == -1) fprintf(stderr, "Error: %s\n",errorInfo); //but we still return;
    
    vector <string> device_names;
    device_names.clear();

    for (; it; it = it -> next) 
        device_names.push_back((string)(it -> name));
    
    return device_names;
}

void addAlldevices() {//add all nearby devices to the devlist
    pcap_if_t *it;
    int ret;

    ret = pcap_findalldevs(&it, errorInfo);
    if(ret == -1) fprintf(stderr, "Error: %s\n",errorInfo); 

    for (; it; it = it -> next) {
        if (it -> name[0] == 'n') continue;
        if (it -> name[0] == 'a' && it -> name[1] == 'n' && it -> name[2] == 'y') continue;
        addDevice(it);
    }
    return;
}

/**
* Add a device to the library for sending/receiving packets.
*
* @param it information of network device to send/receive packet on.
* @return A non-negative _device-ID_ on success, -1 on error.
*/
int addDevice(const pcap_if_t* it) {
    if (device_num == MAX_Device_NUM) {
        fprintf(stderr, "Error: Too many device!\n");
        return -1;
    }

    //check for name duplication
    for (DeviceList *ptr = devlist_ptr; ptr != NULL; ptr = ptr -> nxt) 
        if (strcmp((ptr -> cur) -> name, it -> name) == 0) {
            fprintf(stderr, "Error: Duplication of name!\n");
            return -1;
        }

    bool flag;
    int device_id = device_num;
    Device *ndevice= new Device(it -> name, device_id, flag);

    if (!flag) { // new device error
        fprintf(stderr, "Error: %s\n", errorInfo);
        delete ndevice;
        return -1;
    }   

    ++device_num;

    //set MAC for this device
    for (pcap_addr* addrlist = it -> addresses; addrlist; addrlist = addrlist -> next) {
        sockaddr* dr = addrlist -> addr;
        if (dr -> sa_family != AF_PACKET) continue;
        unsigned char* s = ((struct sockaddr_ll*)dr) -> sll_addr;
        memcpy(ndevice -> MAC, s, 6);
    }

    //set IP for this device, mask might be needed!
    for (pcap_addr* addrlist = it -> addresses; addrlist; addrlist = addrlist -> next) {
        sockaddr* dr = addrlist -> addr;
        //sockaddr* mask = addrlist -> netmask;
        if (dr -> sa_family != AF_INET) continue;
        struct in_addr ipv4 = ((struct sockaddr_in*)dr) -> sin_addr;
        ndevice -> IP = ipv4.s_addr;
    }

    DeviceList *ptr = new DeviceList();
    ptr -> cur = ndevice, ptr -> nxt = devlist_ptr;
    devlist_ptr = ptr;

    return device_id;
}

/**
* Find a device added by `addDevice`.
*
* @param device Name of the network device.
* @return A non-negative _device-ID_ on success, -1 if no such device was found.
*/
//The device-ID is maintained by devlist_ptr. Be sure that addAll() is called before this procedure.
int findDevice(const char* device_name) { 
    for (DeviceList *ptr = devlist_ptr; ptr != NULL; ptr = ptr -> nxt) 
        if (strcmp((ptr -> cur) -> name, device_name) == 0) return (ptr -> cur) -> id;

    return -1;
}

void showDevices() {
    for (DeviceList *ptr = devlist_ptr; ptr != NULL; ptr = ptr -> nxt) {
		fprintf(stderr, "#device_name=%s device_id:%d ", ptr -> cur -> name, ptr -> cur -> id);
        fprintf(stderr, "mac=");
        for (int i = 0; i < 6; ++i)
            fprintf(stderr, "%02x%c", ptr -> cur -> MAC[i], ": "[i == 5]);
        fprintf(stderr, "IP=");
        for (int i = 0, x = ptr -> cur -> IP; i < 4; ++i, x >>= 8)
            fprintf(stderr, "%02x%c", x & 0xff, ":\n"[i == 3]);
	}
}

Device* getDevice(int id) {
    for (DeviceList *ptr = devlist_ptr; ptr != NULL; ptr = ptr -> nxt) 
        if (ptr -> cur -> id == id) return ptr -> cur;
    
    return NULL;
}

Device* getDevicefromMAC(const char* mac) {
    for (DeviceList *ptr = devlist_ptr; ptr != NULL; ptr = ptr -> nxt) 
        if (memcmp(ptr -> cur -> MAC, mac, 6) == 0) return ptr -> cur;

    return NULL;
}

int getDevNum() {
    return device_num;
}

DeviceList* getDevlist() {
    return devlist_ptr;
}