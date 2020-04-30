#!/usr/bin/env bash
EXEC_PATH=.
ulimit -c unlimited

${EXEC_PATH}/mq_server.exe &
