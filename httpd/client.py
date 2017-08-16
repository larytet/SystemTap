#!/usr/bin/python

import requests
import os
import os.path
import logging
import time
import json
import sys
import platform

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
    if 'location' not in r.headers:
        logging.debug("No 'location' in headers")
        return
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
        
        if 'uuid' not in jd:
            logging.debug("Couldn't find 'uuid' in JSON data")
            sys.exit(-1)
        uuid = jd['uuid']

        # Download the stdout and stderr file items.
        stdout_file = ''
        stderr_file = ''
        if 'stdout_location' in jd:
            uri = '%s/%s' % (server_base, jd['stdout_location'])
            stdout_file = download_file(uri)
        else:
            logging.debug("Couldn't find 'stdout' in JSON data")
            sys.exit(-1)
        if 'stderr_location' in jd:
            uri = '%s/%s' % (server_base, jd['stderr_location'])
            stderr_file = download_file(uri)
        else:
            logging.debug("Couldn't find 'stderr' in JSON data")
            sys.exit(-1)
        
        # Here we want to download each item in the optional 'files'
        # array. This is optional since not all stap invocations
        # produce an output file (like a module).
        if 'files' in jd:
            for item in jd['files']:
                if 'location' in item and 'mode' in item:
                    uri = '%s/%s' % (server_base, item['location'])
                    download_file(uri)
                    os.chmod(os.path.basename(item['location']), item['mode'])
                else:
                    logging.debug("File info '%s' isn't complete" % item)

        # Display the stderr and stdout files, then delete them.
        if len(stderr_file):
            f = open(stderr_file)
            sys.stderr.writelines(f.readlines())
            f.close()
            os.remove(stderr_file)
        if len(stdout_file):
            f = open(stdout_file)
            sys.stdout.writelines(f.readlines())
            f.close()
            os.remove(stdout_file)
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

distro_name = platform.linux_distribution()[0]
distro_version = platform.linux_distribution()[1]
logging.debug("distro '%s'", distro_name)

# For now, just pass over the kernel version and arch and a basic command line.
payload = (('kver', os.uname()[2]), ('arch', os.uname()[4]),
           ('distro_name', distro_name), ('distro_version', distro_version),
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
           ('distro_name', distro_name), ('distro_version', distro_version),
           ('cmd_args', '-v'), ('cmd_args', 'hello_world.stp'))
files = {'files': open('hello_world.stp', 'rb')}
r = requests.post(server_base + '/builds', data=payload, files=files)

handle_post_result(r)

#
# Lets try a "stap -L".
#
payload = (('kver', os.uname()[2]), ('arch', os.uname()[4]),
           ('distro_name', distro_name), ('distro_version', distro_version),
           ('cmd_args', '-L'), ('cmd_args', 'kernel.function("sys_open")'))
r = requests.post(server_base + '/builds', data=payload)

handle_post_result(r)

logging.debug("Exiting")
