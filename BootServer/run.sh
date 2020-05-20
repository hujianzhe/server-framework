#!/usr/bin/env bash
EXEC_PATH=.
ulimit -c unlimited

${EXEC_PATH}/BootServer.exe $1 $2 &
