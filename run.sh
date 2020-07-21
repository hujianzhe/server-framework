#!/usr/bin/env bash
EXEC_PATH=./BootServer
ulimit -c unlimited

${EXEC_PATH}/BootServer.exe $1 $2 &
