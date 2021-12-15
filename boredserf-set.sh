#!/bin/sh

# This file is part of boredserf.
# It might be useful as part of a cmd() pipeline,
# using $BS_WINID to identify the appropriate window.

usage="Usage: $0 winid find|go <text>"

[ $# -lt 2 ] && { echo "$usage"; return; }
command -v xprop >/dev/null 2>&1 || { echo "Requires xprop."; return; }

id=$1
shift

case $1 in
find) type=_BS_FIND;;
go) type=_BS_GO;;
*) echo "$usage"; return;;
esac
shift

xprop -id $id -format $type 8u -set $type "$@"
