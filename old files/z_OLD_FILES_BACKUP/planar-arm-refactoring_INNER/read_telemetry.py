import serial
import struct
import time
import sys

# Configurazione Seriale
SERIAL_PORT = '/dev/ttyACM0' # Modifica con la tua porta (es. COM3 su Windows)
BAUD_RATE = 115200

# Definizioni Protocollo
START_BYTE_1 = 0xA5
START_BYTE_2 = 0x5A
RESP_POS = 0x02
PACKET_SIZE = 15 # Header(2) + Type(1) + Q0(4) + Q1(4) + CRC(4)

def crc32(data):
    """Calcola CRC32 standard (polinomio 0xEDB88320)"""
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
    return ~crc & 0xFFFFFFFF

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
        print(f"Connesso a {SERIAL_PORT} @ {BAUD_RATE}")
    except serial.SerialException as e:
        print(f"Errore apertura seriale: {e}")
        sys.exit(1)

    print("In attesa di dati di telemetria...")
    
    buffer = bytearray()
    
    try:
        while True:
            # Leggi dati disponibili
            if ser.in_waiting > 0:
                chunk = ser.read(ser.in_waiting)
                buffer.extend(chunk)
            
            # Cerca pacchetti nel buffer
            while len(buffer) >= PACKET_SIZE:
                # Cerca Header
                if buffer[0] == START_BYTE_1 and buffer[1] == START_BYTE_2:
                    # Header trovato, controlla se abbiamo tutto il pacchetto
                    packet = buffer[:PACKET_SIZE]
                    
                    # Estrai campi
                    # Formato: < (little endian), x (pad byte skipped manually), B (type), f (q0), f (q1), I (crc)
                    # Header (2) | Type (1) | Q0 (4) | Q1 (4) | CRC (4)
                    
                    msg_type = packet[2]
                    
                    if msg_type == RESP_POS:
                        # Payload per CRC: Type(1) + Q0(4) + Q1(4) = 9 bytes
                        payload_for_crc = packet[2:11]
                        received_crc = struct.unpack('<I', packet[11:15])[0]
                        
                        calculated_crc = crc32(payload_for_crc)
                        
                        if calculated_crc == received_crc:
                            q0, q1 = struct.unpack('<ff', packet[3:11])
                            print(f"POS: q0={q0:.4f} rad, q1={q1:.4f} rad")
                        else:
                            print(f"CRC Error: Calc {calculated_crc:08X} != Recv {received_crc:08X}")
                    
                    # Rimuovi pacchetto processato
                    buffer = buffer[PACKET_SIZE:]
                    
                else:
                    # Header non trovato all'inizio, scarta 1 byte e riprova (sliding window)
                    buffer.pop(0)
            
            time.sleep(0.005) # Piccola pausa per non saturare la CPU

    except KeyboardInterrupt:
        print("\nChiusura...")
        ser.close()

if __name__ == "__main__":
    main()
