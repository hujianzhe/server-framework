#!/usr/bin/env bash
EXEC_PATH=.
ulimit -c unlimited

kill -2 `pidof ${EXEC_PATH}/BootServer.exe`
