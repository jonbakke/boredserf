#!/bin/sh
#
# See the LICENSE file for copyright and license details. 
#

xidfile="$HOME/tmp/tabbed-boredserf.xid"
uri=""

if [ "$#" -gt 0 ];
then
	uri="$1"
fi

runtabbed() {
	tabbed -dn tabbed-bs -r 2 boredserf -e '' "$uri" >"$xidfile" \
		2>/dev/null &
}

if [ ! -r "$xidfile" ];
then
	runtabbed
else
	xid=$(cat "$xidfile")
	xprop -id "$xid" >/dev/null 2>&1
	if [ $? -gt 0 ];
	then
		runtabbed
	else
		boredserf -e "$xid" "$uri" >/dev/null 2>&1 &
	fi
fi

