#!/usr/bin/env bash
ulimit -c unlimited

# $1 = execute file path
# $2 = execute config path
$1 $2 &
