#!/bin/bash
#
#  Copyright (c) 2003 Fredrik Ohrn.  All Rights Reserved.
#
#  See the included COPYING file for license details.
#

# Edit the variables

hostname=$HOSTNAME

ipmi_cmd="/usr/local/bin/ipmitool -I open"
rrd_dir="/some/dir/rrd"

# No need to edit below this point.

IFS="
"

for line in `eval $ipmi_cmd -c -v sdr list full` ; do

	IFS=,

	split=($line)

	file="$rrd_dir/$hostname-${split[0]}.rrd"

	if [ -e "$file" ] ; then
		echo "Skipping existing file $file"
		continue
	fi

	echo "Creating file $file"

	rrdtool create "$file" \
		--step 300 DS:var:GAUGE:900:${split[16]}:${split[17]} \
		RRA:AVERAGE:0.5:1:288 \
		RRA:AVERAGE:0.5:6:336 \
		RRA:AVERAGE:0.5:12:720
done
