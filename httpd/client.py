#!/usr/bin/python

import requests
import os
import logging
import time

# These two lines enable debugging at httplib level (requests->urllib3->http.client)
# You will see the REQUEST, including HEADERS and DATA, and RESPONSE with HEADERS but without DATA.
# The only thing missing will be the response.body which is not logged.
try:
    import http.client as http_client
except ImportError:
    # Python 2
    import httplib as http_client
http_client.HTTPConnection.debuglevel = 1

logging.basicConfig(level=logging.DEBUG)

r = requests.get('http://localhost:1234')

print r.status_code
#print r.headers['content-type']

# For now, just pass over the kernel version and arch and a basic command line.
payload = (('kver', os.uname()[2]), ('arch', os.uname()[4]),
           ('cmd_args', '-vp4'), ('cmd_args', '-e'),
           ('cmd_args', '"probe begin { exit() }"'))
r = requests.post('http://localhost:1234/builds', data=payload)
#logging.debug("Response: Status code: %d", r.status_code)
#logging.debug("Request: %s %s %s", r.request.method, r.request.url, r.request.body)

# We should get a 'Location' header from the POST request.


uri = 'http://localhost:1234' + r.headers['location']
delay = int(r.headers['retry-after'])
while True:
    print "Waiting %d seconds..." % delay
    time.sleep(delay)
    r = requests.get(uri)
    if r.status_code == 200:
        logging.debug("Body: %s", r.text)
        break
