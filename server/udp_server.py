#!/usr/bin/env python3
"""
UDP server for LTE-M frames (counterpart of Peer B for LoRa).

For every received DATA frame it sends back an ACK in the same Communication
Frame format (docs/03-frame-structure.md):
  - same DEVICE_ID and SEQUENCE as the received frame,
  - STATUS = 0x00 (OK) or 0x01 (CRC error),
  - INTERFACE = 0x02 (LTE-M),
  - SIGNAL_POWER / SIGNAL_QUALITY = 0x80 (not available for LTE-M).

Usage:  python3 udp_server.py [--port 7000]
"""
import argparse
import socket
import struct
from datetime import datetime

FRAME_MAGIC = 0xAA55
FRAME_TYPE_DATA = 0x01
FRAME_TYPE_ACK = 0x02

HEADER_FORMAT = ">HHBIH"  # MAGIC, DEVICE_ID, FRAME_TYPE, SEQUENCE, PAYLOAD_LENGTH (Big Endian)
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)  # 11 B
CRC_SIZE = 2

ACK_STATUS_OK = 0x00
ACK_STATUS_CRC_ERROR = 0x01
ACK_INTERFACE_LTE = 0x02
SIGNAL_NONE = 0x80


def crc16_modbus(data: bytes) -> int:
    """CRC-16/MODBUS: poly 0x8005 (reflected 0xA001), init 0xFFFF."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc


def encode_frame(device_id: int, frame_type: int, sequence: int, payload: bytes) -> bytes:
    header = struct.pack(HEADER_FORMAT, FRAME_MAGIC, device_id, frame_type, sequence, len(payload))
    body = header + payload
    return body + struct.pack(">H", crc16_modbus(body))


def build_ack(device_id: int, sequence: int, status: int) -> bytes:
    payload = bytes([status, ACK_INTERFACE_LTE, SIGNAL_NONE, SIGNAL_NONE])
    return encode_frame(device_id, FRAME_TYPE_ACK, sequence, payload)


def log(message: str) -> None:
    print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]}] {message}", flush=True)


def handle_datagram(data: bytes):
    """Returns (ack_bytes or None, description)."""
    if len(data) < HEADER_SIZE + CRC_SIZE:
        return None, "too short - ignored"

    magic, device_id, frame_type, sequence, payload_length = struct.unpack_from(HEADER_FORMAT, data)
    if magic != FRAME_MAGIC:
        return None, f"wrong MAGIC 0x{magic:04X} - ignored"
    if len(data) != HEADER_SIZE + payload_length + CRC_SIZE:
        return None, f"length mismatch (PAYLOAD_LENGTH={payload_length}) - ignored"
    if frame_type != FRAME_TYPE_DATA:
        return None, f"FRAME_TYPE 0x{frame_type:02X} is not DATA - ignored"

    crc_received = struct.unpack_from(">H", data, HEADER_SIZE + payload_length)[0]
    crc_computed = crc16_modbus(data[:HEADER_SIZE + payload_length])
    status = ACK_STATUS_OK if crc_received == crc_computed else ACK_STATUS_CRC_ERROR

    description = (f"DATA device=0x{device_id:04X} SEQ={sequence} payload={payload_length} B "
                   f"CRC {'OK' if status == ACK_STATUS_OK else 'ERROR'}")
    return build_ack(device_id, sequence, status), description


def main() -> None:
    parser = argparse.ArgumentParser(description="UDP server for LTE-M Communication Frames")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=7000)
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((args.host, args.port))
    log(f"Listening on UDP {args.host}:{args.port}")

    last_sequence = {}  # device_id -> last SEQUENCE, to mark retransmissions

    while True:
        data, address = sock.recvfrom(2048)
        log(f"RX {len(data)} B from {address[0]}:{address[1]}: {data.hex(' ').upper()}")

        ack, description = handle_datagram(data)
        if ack is None:
            log(f"  {description}")
            continue

        device_id, sequence = struct.unpack_from(">H", data, 2)[0], struct.unpack_from(">I", data, 5)[0]
        if last_sequence.get(device_id) == sequence:
            description += " (retransmission)"
        last_sequence[device_id] = sequence

        sock.sendto(ack, address)
        log(f"  {description} -> ACK {len(ack)} B: {ack.hex(' ').upper()}")


if __name__ == "__main__":
    main()
