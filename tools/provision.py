#!/usr/bin/env python3
"""Provision SmartDesk over USB serial. Keep token private."""
import argparse, getpass, secrets, sys
try:
 import serial
except ImportError:
 sys.exit('Install pyserial: python3 -m pip install pyserial')
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--baud',type=int,default=115200);a=p.parse_args()
ssid=input('iPhone hotspot name: ').strip(); password=getpass.getpass('Hotspot password: '); token=secrets.token_hex(24)
if not (1<=len(ssid.encode())<=32 and 8<=len(password)<=63):sys.exit('SSID must be 1-32 bytes, password 8-63 characters')
if any(c in ssid+password for c in '\r\n'):sys.exit('Newlines not supported')
with serial.Serial(a.port,a.baud,timeout=1) as s:
 for line in (f'SSID {ssid}\n',f'PASS {password}\n',f'TOKEN {token}\n','SAVE\n'):
  s.write(line.encode());s.flush()
print('API token (store securely; shown once):',token)
print('Reset the board to connect to the hotspot.')
