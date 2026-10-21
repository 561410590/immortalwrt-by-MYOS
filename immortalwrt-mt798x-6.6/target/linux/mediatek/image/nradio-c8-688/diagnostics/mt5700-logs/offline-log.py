#!/usr/bin/env python3
"""Receive vendor MDON offline log over the existing Serial_C interface.

Protocol checked against SP1C01 offlinelog_pack FileSender: XMODEM-1K,
CRC16, block 1 first, and three-byte EOT (04, sequence, complement).
Diagnostic only; no SIM, modem mode, routing, or startup changes.
"""
import argparse
import binascii
import json
import os
from pathlib import Path
import select
import subprocess
import termios
import time
import tty
import zlib


def receive(port, output, mode):
    protocol = Path('/sys/class/tty', Path(port).name, 'device/../bInterfaceProtocol')
    if protocol.read_text().strip().lower() != '1c':
        raise RuntimeError('Port is not the enumerated Serial_C interface')
    for entry in Path('/proc').glob('[0-9]*/fd/*'):
        try:
            if os.readlink(entry) == port:
                raise RuntimeError('Serial_C already owned: ' + str(entry))
        except (FileNotFoundError, PermissionError):
            pass
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    original = termios.tcgetattr(fd)
    command = None
    completed = False
    try:
        tty.setraw(fd)
        termios.tcflush(fd, termios.TCIFLUSH)
        command = subprocess.Popen(
            ['/usr/sbin/atsd_cli', '-i', 'cpe', '-c', 'AT^MDON=' + str(mode), '-w', '5000'],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + 120
        pending = bytearray()
        data = bytearray()
        expected = 1
        started = False
        last_receive = time.monotonic()
        last_handshake = 0
        errors = 0
        while time.monotonic() < deadline:
            now = time.monotonic()
            if not started and now - last_handshake >= 1:
                os.write(fd, b'C')
                last_handshake = now
            if select.select([fd], [], [], 0.2)[0]:
                chunk = os.read(fd, 4096)
                if chunk:
                    pending.extend(chunk)
                    last_receive = now
            if now - last_receive > (15 if started else 45):
                raise RuntimeError('Log transfer timed out; received bytes=' + str(len(data)))
            while pending:
                kind = pending[0]
                size = 1024 if kind == 2 else 128 if kind == 1 else None
                if kind == 4:
                    if len(pending) < 3:
                        break
                    if pending[1] != expected or pending[1] ^ pending[2] != 255:
                        raise RuntimeError('Invalid vendor EOT sequence')
                    os.write(fd, b'\x06')
                    completed = True
                    break
                if kind == 24:
                    raise RuntimeError('Modem cancelled log transfer')
                if size is None:
                    raise RuntimeError('Unexpected Serial_C frame prefix=' + pending[:16].hex())
                if len(pending) < size + 5:
                    break
                frame = bytes(pending[:size + 5])
                del pending[:size + 5]
                payload = frame[3:-2]
                valid = frame[1] ^ frame[2] == 255 and binascii.crc_hqx(payload, 0) == int.from_bytes(frame[-2:], 'big')
                if not valid:
                    errors += 1
                    if errors > 5:
                        raise RuntimeError('Repeated damaged log frames')
                    os.write(fd, b'\x15')
                    continue
                if frame[1] == expected:
                    data.extend(payload)
                    if len(data) > 64 * 1024 * 1024:
                        raise RuntimeError('Diagnostic log exceeded size limit')
                    expected = (expected + 1) & 255
                    started = True
                elif frame[1] != (expected - 1) & 255:
                    raise RuntimeError('Unexpected log block sequence')
                os.write(fd, b'\x06')
            if completed:
                break
        if not completed or not data:
            raise RuntimeError('No complete offline log received')
        decoder = zlib.decompressobj(31)
        decoder.decompress(data, 128 * 1024 * 1024)
        if not decoder.eof:
            raise RuntimeError('Incomplete or oversized gzip log')
        length = len(data) - len(decoder.unused_data)
        with open(output, 'xb') as destination:
            destination.write(data[:length])
        print(json.dumps({'complete': True, 'bytes': length, 'output': output, 'mode': mode}), flush=True)
    finally:
        if not completed:
            os.write(fd, b'\x18\x18')
        termios.tcsetattr(fd, termios.TCSANOW, original)
        os.close(fd)
        if command:
            try:
                response = command.communicate(timeout=7)[0]
            except subprocess.TimeoutExpired:
                command.terminate()
                response = command.communicate(timeout=3)[0]
            print('MDON response: ' + response.decode(errors='replace').strip(), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='/dev/ttyUSB2')
    parser.add_argument('--output', required=True)
    parser.add_argument('--mode', type=int, choices=(1, 2, 3, 4, 5), default=3)
    args = parser.parse_args()
    receive(args.port, args.output, args.mode)
