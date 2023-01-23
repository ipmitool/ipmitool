#!/bin/bash

# Build the ipmi_sim & lanserv
simdir=./openipmi-code/lanserv
mkdir -p ${simdir}/my_statedir

# TODO
# cp lan.conf to /usr/local/etc/ipmi/ folder
# run once to initialzie ipmi_sim folder structure
${simdir}/ipmi_sim -c ${simdir}/lan.conf -f ${simdir}/ipmisim1.emu -s ${simdir}/my_statedir <<< quit


# Install SDR into state directory
cp ${simdir}/ipmisim1.bsdr ${simdir}/my_statedir/ipmi_sim/ipmisim1/sdr.20.main
# rerun the ipmi_sim
${simdir}/ipmi_sim -c ${simdir}/lan.conf -f ${simdir}/ipmisim1.emu -s ${simdir}/my_statedir -n &
simpid=`echo $!`

robot test.robot
kill -9 $simpid
