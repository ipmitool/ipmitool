#!/bin/bash

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
