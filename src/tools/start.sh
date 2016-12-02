#!/bin/sh

export PATH=$PATH:/opt/bin/

stop.sh
sleep 1

#ulimit -c unlimited

ARG="-d 2 -b"

core_daemon $ARG
mm_daemon $ARG
