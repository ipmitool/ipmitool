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

for line in `eval $ipmi_cmd -c sdr list full` ; do

	IFS=,

	split=($line)

	file="$rrd_dir/$hostname-${split[0]}.rrd"

	rrdupdate "$file" "N:${split[1]}"
done
