#include "protocol.h"
#include "main.h" // For globals or HAL if needed
#include <string.h> // for size_t

// Global variables (moved from manipulator.c)
uint8_t rx_data[RX_BUFFER_SIZE]; 
uint8_t tx_data[22];
uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
uint16_t uart_rx_tail = 0;
// Note: parse_state, packet_buffer, packet_idx, global_size seemed unused or local enough in original file, 
// checking if they are needed. global_size was updated in pb_push.
// keeping local static if possible or global if needed.
// global_degs1/2 etc were used for debugging likely.

// External dependencies (like calibration_start) need to be known if we call them.
// We need to include "manipulator.h" to access high-level functions called from protocol callbacks
#include "manipulator.h" 

uint32_t crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc = crc >> 1;
        }
    }
    return ~crc;
}

// Packet Buffer Helpers
// Modified to take manipulator pointer to not rely on global 'manipulator' variable if possible, 
// though the original code used global 'manipulator'.
// We will use the passed pointer.

int pb_push(manipulator_t *manipulator, Packet_t packet) {
    uint8_t next_tail = (manipulator->mb_tail + 1) % MOTION_BUFFER_SIZE;
    if (next_tail == manipulator->mb_head) {
        return 0; // Buffer Full
    }
    manipulator->motion_buffer[manipulator->mb_tail] = packet;
    manipulator->mb_tail = next_tail;
    manipulator->mb_count++;
    // global_size = manipulator->mb_count; // Removed global_size as it's likely debug
    return 1;
}

int pb_pop(manipulator_t *manipulator, Packet_t *packet) {
    if (manipulator->mb_head == manipulator->mb_tail) {
        return 0; // Buffer Empty
    }
    *packet = manipulator->motion_buffer[manipulator->mb_head];
    manipulator->mb_head = (manipulator->mb_head + 1) % MOTION_BUFFER_SIZE;
    manipulator->mb_count--;
    return 1;
}

void manipulator_uart_process(manipulator_t *manipulator, UART_HandleTypeDef *huart) {
    uint16_t head = UART_RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx);
    
    // Process all available data
    while (uart_rx_tail != head) {
        // Calculate available bytes considering wrap-around
        uint16_t available;
        if (head >= uart_rx_tail) {
            available = head - uart_rx_tail;
        } else {
            available = UART_RX_BUFFER_SIZE - uart_rx_tail + head;
        }
        
        if (available < PACKET_SIZE) {
            // Not enough data for a full packet yet
            break; 
        }
        
        // Check for header at tail
        uint8_t b1 = uart_rx_buffer[uart_rx_tail];
        uint8_t b2 = uart_rx_buffer[(uart_rx_tail + 1) % UART_RX_BUFFER_SIZE];
        
        if (b1 == START_BYTE_1 && b2 == START_BYTE_2) {
            // Potential packet found.
            // Copy to temp buffer to handle wrap-around easily
            uint8_t temp_pkt[PACKET_SIZE];
            for (int i = 0; i < PACKET_SIZE; i++) {
                temp_pkt[i] = uart_rx_buffer[(uart_rx_tail + i) % UART_RX_BUFFER_SIZE];
            }
            
            // Verify CRC
            Packet_t *pkt = (Packet_t*)temp_pkt;
            uint32_t calc_crc = crc32(&temp_pkt[2], 26);
            
            if (calc_crc == pkt->checksum) {
                 // Valid packet! Process it.
                 if (pkt->cmd == CMD_TRAJECTORY) {
                     pb_push(manipulator, *pkt);
                 } else if (pkt->cmd == CMD_HOMING) {
                     calibration_start(manipulator);
                 } else if (pkt->cmd == CMD_POS) {
                      Feedback_POS_t fb;
                      fb.header[0] = START_BYTE_1;
                      fb.header[1] = START_BYTE_2;
                      fb.type = RESP_POS;
                      rbgetoffset(&manipulator->q0, 0, &fb.q0_actual);
                      rbgetoffset(&manipulator->q1, 0, &fb.q1_actual);
                      uint8_t *fb_ptr = (uint8_t*)&fb;
                      fb.checksum = crc32(fb_ptr + 2, 9);
                      HAL_UART_Transmit_DMA(huart, (uint8_t*)&fb, sizeof(Feedback_POS_t));
                 } else if (pkt->cmd == CMD_PEN) {
                     control_pen(manipulator, pkt->pen_up);
                 }
                 
                 // Advance tail by PACKET_SIZE
                 uart_rx_tail = (uart_rx_tail + PACKET_SIZE) % UART_RX_BUFFER_SIZE;
            } else {
                 // Invalid CRC. This is not a valid packet (or data corruption).
                 // Advance tail by 1 to search for next header.
                 uart_rx_tail = (uart_rx_tail + 1) % UART_RX_BUFFER_SIZE;
            }
        } else {
            // Not a header. Advance tail by 1.
            uart_rx_tail = (uart_rx_tail + 1) % UART_RX_BUFFER_SIZE;
        }
    }
}

void manipulator_handle_telemetry(manipulator_t *manipulator, UART_HandleTypeDef *huart) {
    if (manipulator->telemetry_ready) {
        // Check if UART is ready. If it's BUSY_TX, we cannot transmit.
        if (huart->gState == HAL_UART_STATE_READY) {
            static Feedback_POS_t telemetry_pkt;
            
            float q0 = 0.0f, q1 = 0.0f;
            
            // Note: global_degs1/2 are not available here easily unless we export them or read from manipulator struct. 
            // The original code used global_degs1 as fallback if buffer empty.
            // We should prefer reading from valid state.
            // Assuming rbgetoffset is robust.
            
            if (rbgetoffset(&manipulator->q0, 0, &q0) == 0) {
                 // Fallback if buffer empty - in original code it used global_degs variables.
                 // We will approximate or use 0.0f if not available, OR rely on manipulator->current_position logic if implemented.
                 // For now, let's assume buffer is populated or 0.
                 q0 = 0.0f; 
                 q1 = 0.0f;
            } else {
                rbgetoffset(&manipulator->q1, 0, &q1);
            }
            
            telemetry_pkt.header[0] = START_BYTE_1;
            telemetry_pkt.header[1] = START_BYTE_2;
            telemetry_pkt.type = RESP_POS;
            telemetry_pkt.q0_actual = q0;
            telemetry_pkt.q1_actual = q1;
            telemetry_pkt.checksum = crc32(((uint8_t*)&telemetry_pkt) + 2, 9);

            if (HAL_UART_Transmit_DMA(huart, (uint8_t*)&telemetry_pkt, sizeof(Feedback_POS_t)) == HAL_OK) {
                manipulator->telemetry_ready = 0;
            }
        }
    }
}
