#!/usr/bin/env bash
EXEC_PATH=./BootServer
ulimit -c unlimited

kill -2 `pidof ${EXEC_PATH}/BootServer.exe`
