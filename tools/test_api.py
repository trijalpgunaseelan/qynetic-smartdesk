#!/usr/bin/env python3
"""Non-destructive API smoke tests; requires a running, provisioned device."""
import argparse,json,urllib.request,urllib.error
p=argparse.ArgumentParser();p.add_argument('--host',required=True);p.add_argument('--token',required=True);a=p.parse_args()
base='http://'+a.host+'/api/v1/'
def request(path,body=None,auth=True):
 headers={'Content-Type':'application/json'}
 if auth:headers['Authorization']='Bearer '+a.token
 data=None if body is None else json.dumps(body).encode()
 try:
  with urllib.request.urlopen(urllib.request.Request(base+path,data=data,headers=headers),timeout=5) as r:return r.status,r.read().decode()
 except urllib.error.HTTPError as e:return e.code,e.read().decode()
assert request('health',auth=False)[0]==200
assert request('status',auth=False)[0]==401
assert request('battery',{'level':101})[0]==400
assert request('battery',{'level':-1})[0]==400
assert request('battery',{'level':50})[0]==200
assert request('reminders',{'reminders':[]})[0]==200
assert request('calendar',{'events':[]})[0]==200
assert request('battery',{'level':42,'extra':'x'*9000})[0]==413
print('API smoke tests passed (battery and lists changed on device).')
