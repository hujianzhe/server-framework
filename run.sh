#!/usr/bin/env bash
ulimit -c unlimited

$1 $2 &
