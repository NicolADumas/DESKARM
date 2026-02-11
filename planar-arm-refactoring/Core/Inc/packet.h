#include<stdint.h>

#ifndef PACKET_H
#define PACKET_H

#define START_BYTE_1 0xA5
#define START_BYTE_2 0x5A

#define CMD_TRAJECTORY 0x01
#define CMD_HOMING     0x02
#define CMD_STOP       0x03
#define CMD_POS        0x04
#define CMD_MELODY     0x05

#define RESP_STATUS    0x01
#define RESP_POS       0x02

#define PACKET_SIZE 32
#define MOTION_BUFFER_SIZE 100 

#define PARSE_STATE_HEADER1 0
#define PARSE_STATE_HEADER2 1
#define PARSE_STATE_BODY    2


typedef struct {
    uint8_t header[2]; // 0xA5 0x5A
    uint8_t cmd;       // Command ID
    // Payload (24 bytes)
    float q0;
    float q1;
    float dq0;
    float dq1;
    float ddq0;
    float ddq1;
    uint8_t pen_up;
    // Checksum (4 bytes)
    uint32_t checksum;
} __attribute__((packed)) Packet_t;

typedef struct {
    uint8_t header[2]; // 0xA5 0x5A
    uint8_t type;      // RESP_POS
    float q0_actual;
    float q1_actual;
    uint32_t checksum;
} __attribute__((packed)) Feedback_POS_t;



#endif // PACKET_H