#!/bin/bash
#
# bmcautoconf [interface] [channel]
#
#
# Copyright (c) 2003 Sun Microsystems, Inc.  All Rights Reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 
# Redistribution of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# 
# Redistribution in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
# 
# Neither the name of Sun Microsystems, Inc. or the names of
# contributors may be used to endorse or promote products derived
# from this software without specific prior written permission.
# 
# This software is provided "AS IS," without a warranty of any kind.
# ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
# INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
# PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
# SUN MICROSYSTEMS, INC. ("SUN") AND ITS LICENSORS SHALL NOT BE LIABLE
# FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
# OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
# SUN OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA,
# OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR
# PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF
# LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
# EVEN IF SUN HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
# 
# You acknowledge that this software is not designed or intended for use
# in the design, construction, operation or maintenance of any nuclear
# facility.

DEBUG=0

# if the wrong channel is used serious problems could occur
# Channel 6 == eth0, top interface on v60x and v65x
# Channel 6 == eth1, top interface on LX50
# Channel 7 == eth1, bottom interface  on v60x and v65x
# Channel 7 == eth0, bottom interface on LX50
CHANNEL=6
IFACE=eth0

# ipmitool interface
# open = OpenIPMI kernel driver
#	 [ipmi_msghandler, ipmi_kcs_drv, ipmi_devintf]
# imb  = Intel IMB 
IPMIINTF=open

# util locations
IPMITOOL=/usr/bin/ipmitool
PING=/bin/ping
ARP=/sbin/arp
IFCONFIG=/sbin/ifconfig
ROUTE=/sbin/route

ipmitool_lan_set ()
{
    [ $# -lt 1 ] && return
    PARAM=$1

    VALUE=
    [ $# -ge 2 ] && VALUE=$2

    if [ $DEBUG -gt 0 ]; then
	echo "Setting LAN parameter ${PARAM} to ${VALUE}"
	echo "$IPMITOOL -I $IPMIINTF lan set $CHANNEL $PARAM $VALUE"
    fi

    $IPMITOOL -I $IPMIINTF lan set $CHANNEL $PARAM $VALUE
}

if [ ! -x $IPMITOOL ]; then
    echo "Error: unable to find $IPMITOOL"
    exit 1
fi

if [ $# -ge 1 ]; then
    IFACE=$1
    if ! $IFCONFIG $IFACE | grep -q "inet addr:" >/dev/null 2>&1 ; then
	echo "Error: unable to find interface $IFACE"
	exit 1
    fi
fi

if [ $# -ge 2 ]; then
    CHANNEL=$2
    if [ $CHANNEL -ne 6 ] && [ $CHANNEL -ne 7 ]; then
	echo "Invalid channel: $CHANNEL"
	exit 1
    fi
fi

[ $DEBUG -gt 0 ] && echo "Auto-configuring $IFACE (channel $CHANNEL)"

# IP Address
IP_ADDRESS=$( $IFCONFIG $IFACE | grep "inet addr:" | awk -F"[:[:space:]]+" '{ print $4 }' )
if [ X$IP_ADDRESS = X ]; then
    echo "Unable to determine IP address for interface $IFACE"
    exit 2
fi

# Netmask
IP_NETMASK=$( $IFCONFIG $IFACE | grep "inet addr:" | awk -F"[:[:space:]]+" '{ print $8 }' )
if [ X$IP_NETMASK = X ]; then
    echo "Unable to determine IP netmask for interface $IFACE"
    exit 3
fi

# MAC Address
MAC_ADDRESS=$( $IFCONFIG $IFACE | grep "HWaddr" | awk '{ print $5 }' )
if [ X$MAC_ADDRESS = X ]; then
    echo "Unable to determine MAC address for interface $IFACE"
    exit 4
fi

# default route IP Address
DEF_ROUTE_IP=$( $ROUTE -n | awk '/^0.0.0.0/ { print $2 }' )
if [ X$DEF_ROUTE_IP = X ]; then
    echo "Unable to determine default route IP address"
    exit 5
fi

# Default Route MAC Address
# (ping it first to populate arp table)
$PING -q -c1 >/dev/null 2>&1
DEF_ROUTE_MAC=$( $ARP -an -i $IFACE | grep "$DEF_ROUTE_IP[^0-9]" | awk '{ print $4 }' )
if [ X$DEF_ROUTE_MAC = X ]; then
    echo "Unable to determine default route MAC address"
    exit 6
fi

ipmitool_lan_set "ipsrc" "static"
ipmitool_lan_set "ipaddr" $IP_ADDRESS
ipmitool_lan_set "netmask" $IP_NETMASK
ipmitool_lan_set "macaddr" $MAC_ADDRESS
ipmitool_lan_set "defgw ipaddr" $DEF_ROUTE_IP
ipmitool_lan_set "defgw macaddr" $DEF_ROUTE_MAC
ipmitool_lan_set "auth callback,user,operator,admin" "md2,md5"
ipmitool_lan_set "access" "on"
ipmitool_lan_set "user"

exit 0
