#!/usr/bin/env python
from __future__ import unicode_literals, print_function

import os.path
import sys
import time

import requests

api_url = 'https://ci.appveyor.com/api'
account_name = os.getenv('APPVEYOR_ACCOUNT')
project_slug = os.getenv('APPVEYOR_SLUG')
headers = {'Authorization': 'Bearer ' + os.getenv('APPVEYOR_TOKEN')}

# Trigger the AppVeyor build
r = requests.post(api_url + '/builds', {
    'accountName': account_name,
    'projectSlug': project_slug,
    'branch': os.getenv('TRAVIS_BRANCH'),
    'commitID': os.getenv('TRAVIS_COMMIT')
}, headers=headers)
r.raise_for_status()
build = r.json()
print('Started AppVeyor build (buildId={buildId}, version={version})'.format(**build))

# Wait until the build has finished
while True:
    url = '{}/projects/{}/{}/build/{}'.format(api_url, account_name, project_slug,
                                              build['version'])
    r = requests.get(url, headers=headers)
    r.raise_for_status()
    build = r.json()['build']
    status = build['status']
    if status in ('starting', 'queued', 'running'):
        print('Build status: {}; checking again in 5 seconds'.format(status))
        time.sleep(5)
    elif status == 'success':
        print('Build successful')
        job_ids = [job['jobId'] for job in build['jobs']]
        break
    else:
        print('Build failed with status: {}'.format(status), file=sys.stderr)
        sys.exit(1)

# Download the artifacts to wheelhouse/
os.mkdir('wheelhouse')
for job_id in job_ids:
    r = requests.get('{}/buildjobs/{}/artifacts'.format(api_url, job_id), headers=headers)
    r.raise_for_status()
    for artifact in r.json():
        url = '{}/buildjobs/{}/artifacts/{}'.format(api_url, job_id, artifact['fileName'])
        r = requests.get(url, headers=headers, stream=True)
        r.raise_for_status()
        file_name = 'wheelhouse/' + os.path.basename(artifact['fileName'])
        with open(file_name, 'wb') as f:
            for chunk in r.iter_content(None):
                f.write(chunk)

        print('Downloaded ' + f.name)
