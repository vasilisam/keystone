#!/bin/bash

# Copy file from QEMU guest to host using SCP
PORT=9821
REMOTE_USER=root
REMOTE_FILE="/usr/share/keystone/examples/output.txt"
LOCAL_DEST="output.txt"

#rm -f ${LOCAL_DEST}
# Use scp with -O flag to force the use of legacy protocol to avoid sftp issues
scp -O -P $PORT ${REMOTE_USER}@localhost:$REMOTE_FILE $LOCAL_DEST

# Copy file from remote host to my local machine
#REMOTE_USER=
#LOCAL_DEST="output.txt"
