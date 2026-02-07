/*
 * protocol.h
 *
 *  Created on: Jan 5, 2026
 *      Author: umby
 */

#ifndef INC_PROTOCOL_H_
#define INC_PROTOCOL_H_

#include "manipulator_types.h"
#include "usart.h"

/* UART Circular Buffer Definitions */
#define UART_RX_BUFFER_SIZE 512
#define RX_BUFFER_SIZE 64 

extern uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
extern uint8_t rx_data[RX_BUFFER_SIZE]; 
extern uint8_t tx_data[22];

uint32_t crc32(const uint8_t *data, size_t length);
int pb_push(manipulator_t *manipulator, Packet_t packet);
int pb_pop(manipulator_t *manipulator, Packet_t *packet);
void manipulator_uart_process(manipulator_t *manipulator, UART_HandleTypeDef *huart);
void manipulator_handle_telemetry(manipulator_t *manipulator, UART_HandleTypeDef *huart);

#endif /* INC_PROTOCOL_H_ */
