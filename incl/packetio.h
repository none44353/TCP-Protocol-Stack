#include "device.h"

/**
* @file packetio.h
* @brief Library supporting sending/receiving Ethernet II frames.
*/

#ifndef _PACKETIO_H_
#define _PACKETIO_H_

struct ethHdr_t {
    u_char dstMAC[6];
    u_char srcMAC[6];
    uint16_t ethtype;
};

uint16_t changeEndian16(uint16_t x);

/**
* @brief Encapsulate some data into an Ethernet II frame and send it.
*
* @param buf Pointer to the payload.
* @param len Length of the payload.
* @param ethtype EtherType field value of this frame.
* @param destmac MAC address of the destination.
* @param id ID of the device(returned by `addDevice`) to send on. @return 0 on success, -1 on error. @see addDevice
*/
int sendFrame(const void* buf, int len, int ethtype, const void* destmac, int id);

/**
* @brief Process a frame upon receiving it.
* @param buf Pointer to the frame.
* @param len Length of the frame.
* @param id ID of the device (returned by `addDevice`) receiving current frame. @return 0 on success, -1 on error. @see addDevice
*/
typedef int (*frameReceiveCallback)(const void*, int, int);
//The concrete implementation of callback function
int recvFrame(const void* buf, int len, int id);

/**
* @brief Register a callback function to be called each time an Ethernet II frame was received.
* @param id the device id
* @param callback the callback function.
* @return 0 on success, -1 on error.
* @see frameReceiveCallback
*/
int setFrameReceiveCallback(int id, frameReceiveCallback callback);

#endif