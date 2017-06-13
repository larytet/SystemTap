#!/usr/bin/python

import requests
import os
import logging
import time
import json
import sys

def download_file(url):
    local_filename = url.split('/')[-1]
    # NOTE the stream=True parameter
    r = requests.get(url, stream=True)
    if r.status_code == 200:
        with open(local_filename, 'wb') as f:
            for chunk in r.iter_content(chunk_size=1024): 
                if chunk: # filter out keep-alive new chunks
                    f.write(chunk)
        logging.debug("Download '%s'", local_filename)
        return local_filename
    else:
        logging.debug("Couldn't download file")
        return ""

def handle_post_result(r):
    # We should get a 'Location' header from the POST request.
    uri = server_base + r.headers['location']
    delay = int(r.headers['retry-after'])
    result_found = False
    while True:
        logging.debug("Waiting %d seconds...", delay)
        time.sleep(delay)

        # We want to handle redirects ourselves just to be sure
        # everything is working properly.
        r = requests.get(uri, allow_redirects=False)
        logging.debug("Status code: %d", r.status_code)
        if r.status_code == 200:
            logging.debug("200 Body: %s", r.text)

            # The body should be valid JSON
            try:
                jd = json.loads(r.text)
            except ValueError:
                logging.debug("Couldn't parse JSON data")

        elif r.status_code == 303:
            logging.debug("303 Body: %s", r.text)

            # The body should be valid JSON
            try:
                jd = json.loads(r.text)
            except ValueError:
                logging.debug("Couldn't parse JSON data")
            uri = server_base + r.headers['location']
            result_found = True
            break
        else:
            logging.debug("Unhandled status code: %d", r.status_code)
            sys.exit(-1)

    if not result_found:
        logging.debug("Didn't see 303 status code")
        sys.exit(-1)

    # Get the result info
    r = requests.get(uri)
    if r.status_code == 200:
        logging.debug("Body: %s", r.text)

        # The body should be valid JSON
        try:
            jd = json.loads(r.text)
        except ValueError:
            logging.debug("Couldn't parse JSON data")
            sys.exit(-1)
        
        # If we've got a "module" item, try to download it.
        if 'uuid' not in jd:
            logging.debug("Couldn't find 'uuid' in JSON data")
            sys.exit(-1)
        uuid = jd['uuid']
        if 'module' in jd:
            uri = '%s/results/%s/%s' % (server_base, uuid, jd['module'])
            download_file(uri)
        else:
            logging.debug("Couldn't find 'module' in JSON data")
            sys.exit(-1)
    else:
        logging.debug("Couldn't get result")
        sys.exit(-1)
    return


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

server_base = 'http://localhost:1234'
r = requests.get(server_base)

#print r.headers['content-type']

# For now, just pass over the kernel version and arch and a basic command line.
payload = (('kver', os.uname()[2]), ('arch', os.uname()[4]),
           ('cmd_args', '-vp4'), ('cmd_args', '-e'),
           ('cmd_args', 'probe begin { exit() }'))
r = requests.post(server_base + '/builds', data=payload)

handle_post_result(r)

#
# Now lets try uploading a file along with the POST.
#
# FIXME: How do we know we need to send the file?
#
payload = (('kver', os.uname()[2]), ('arch', os.uname()[4]),
           ('cmd_args', '-v'), ('cmd_args', 'hello_world.stp'))
files = {'files': open('hello_world.stp', 'rb')}
r = requests.post(server_base + '/builds', data=payload, files=files)

handle_post_result(r)

logging.debug("Exiting")
