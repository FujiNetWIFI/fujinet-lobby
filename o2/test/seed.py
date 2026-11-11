#!/usr/bin/env python3
"""seed.py -- POST each room of a JSON list to a lobby server's /server.

    seed.py FILE URL
"""
import json
import sys
import urllib.request

for room in json.load(open(sys.argv[1])):
    req = urllib.request.Request(sys.argv[2], data=json.dumps(room).encode(),
                                 headers={"Content-Type": "application/json"})
    urllib.request.urlopen(req).read()
