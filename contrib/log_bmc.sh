#!/bin/sh
#############################################################################
#
# log_bmc.sh: Add SEL entries to indicate OS Boot/Install status.
#
# version:      0.1
#
# Authors:      Charles Rose <charles_rose@dell.com>
#               Jordan Hargrave <jordan_hargrave@dell.com>
#
# Description:  Script to log OS boot/install status to the BMC. Primarily
#		meant for use in automated installs and start up scripts.
#		Will provide administrators with OS boot/install status in
#		BMC and aid with debugging.
#
#               Example usage:
#               # ./log_bmc.sh inst_start
#               # ipmitool sel list
#		b | 05/07/2014 | 12:07:32 | OS Boot | Installation started
#
#               See here for details:
#               https://fedoraproject.org/wiki/Features/AgentFreeManagement
#
#############################################################################
IPMI_CMD="/usr/bin/ipmitool"

#############################################################################
# SEL Event types from ipmi_sel.h
OS_STOP="0x20"
OS_BOOT="0x1f"
# SEL Event data from ipmi_sel.h
GRACEFUL_SHUTDOWN="0x03" # OS Stop/Shutdown: Installation started
BOOT_COMPLETED="0x01" # OS Boot: Installation started
INSTALL_STARTED="0x07" # OS Boot: Installation started
INSTALL_COMPLETED="0x08" # OS Boot: Installation completed
INSTALL_ABORTED="0x09" # OS Boot: Installation aborted
INSTALL_FAILED="0x0a" # OS Boot: Installation failed

##########################################################################

# check for ipmi functionality.
check_ipmi()
{
	# ensures presence of ipmitool and /dev/ipmi*
	${IPMI_CMD} mc info > /dev/null 2>&1
	[ $? -ne 0 ] && RETVAL=2
}

# Write out the events to SEL
ipmi_sel_add()
{
	# Refer ipmitool(1) event for details on format.
	printf "0x04 %s 0x00 0x6f %s 0x00 0x00" ${type} ${status} > \
		${tmpfile} && \
		${IPMI_CMD} sel add ${tmpfile} > /dev/null 2>&1
	[ $? -ne 0 ] && RETVAL=3
}

### Main
# Most of the status is for this event type
tmpfile=$(/usr/bin/mktemp)
RETVAL=0
type=${OS_BOOT}

case ${1} in
	os_shutdown)   type=${OS_STOP}; status=${GRACEFUL_SHUTDOWN} ;;
	os_boot)       status=${BOOT_COMPLETED} ;;
	inst_start)    status=${INSTALL_STARTED} ;;
	inst_complete) status=${INSTALL_COMPLETED} ;;
	inst_abort)    status=${INSTALL_ABORTED} ;;
	inst_fail)     status=${INSTALL_FAILED} ;;
	*)             RETVAL=1 ;;
esac

[ ${RETVAL} -eq 0 ] && check_ipmi
[ ${RETVAL} -eq 0 ] && ipmi_sel_add ${status}

case ${RETVAL} in
	0) ;;
	1) printf -- %s\\n "Usage: $0 <os_boot|os_shutdown|inst_start|inst_complete|inst_abort|inst_fail>" ;;
	2) printf -- %s\\n "failed to communicate with BMC." ;;
	3) printf -- %s\\n "error adding ipmi sel entry." ;;
esac

[ -f ${tmpfile} ] && rm -f ${tmpfile} > /dev/null 2>&1

exit ${RETVAL}
### End
