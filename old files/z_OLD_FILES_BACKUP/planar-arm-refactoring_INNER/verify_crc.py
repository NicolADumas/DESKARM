import zlib
import struct

def crc32_stm32(data):
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc = crc >> 1
    return ~crc & 0xFFFFFFFF

# Packet Data: Cmd(02) + 25 bytes of 00
data = bytes([0x02]) + bytes([0x00] * 25)

# Calculate CRC
calculated_crc = crc32_stm32(data)
print(f"Calculated CRC: 0x{calculated_crc:08X}")

# User's CRC bytes: ] \xf5 \xa6 \xba
# ] = 0x5D
user_crc_bytes = b'\x5d\xf5\xa6\xba'
user_crc_val = struct.unpack('<I', user_crc_bytes)[0]
print(f"User Packet CRC: 0x{user_crc_val:08X}")

if calculated_crc == user_crc_val:
    print("CRC Match!")
else:
    print("CRC Mismatch!")
