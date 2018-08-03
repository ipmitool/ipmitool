This document was last updated for release 1.8.8.

This document explains how Serial Over Lan is implemented on in the
ipmitool IPMI client.  Obviously, the code itself is authoritative, but
this document should serve as a good starting point.

Serial Over Lan (SOL) is defined in the IPMI v2 specification published by
Intel and available at http://www.intel.com/design/servers/ipmi/.  SOL
functionality is built on top of the RMCP+ protocol as an additional
payload type (type 1).

The high end SOL logic is implemented in src/ipmitool/lib/ipmi_sol.c.  SOL
sessions are begun in ipmitool using the "sol activate" command.  This
command maps directly to the IPMI Activate Payload command.  It first
verifies that an RMCP+ session (lanplus interface) is being used to
establish the session.  Although the spec allows for a SOL connection to be
established on a port different than the RMCP+ port that the "activate
payload" command issued, ipmitool does not support this.

Once a session has been established (the activate payload command
succeeds), ipmitool simply loops over a select() on user input and data
returned from the BMC.  All user input is first filtered so that special
escape sequences can suspend or deactivate the SOL session and so that data
can be broken into chunks no greater than N bytes.  This maximum is
specified by the BMC in the response to the Activate Payload command.

User input to the BMC is handled in ipmitool/src/plugins/lanplus/lanplus.c.
Every SOL packet (with one exception) traveling in either direction causes
the recipient to return an acknowledgement packet, though acks themself are
not acknowledged.  The transport layer in lanplus.c handles the logic
regarding acks, partial acks, sequence numbers.  SOL acknowledgements
packets be acks, partial acks (the remote destination processed only some
of the data), and nacks (requests to stop sending packets).  Nacks are not
honored by ipmitool.

Note that one way that SOL communication differs from standard IPMI
commands, is that it is not simply a request response protocol.  Packets
may be returned asynchronously from the BMC.  When establishing a SOL
session, ipmitool registers a callback for asynchronously received data.
This call back simply prints text returned from the BMC.

Once a user has chosen to exit the SOL session (with ~.) ipmitool sends the
IPMI SOL Deactivate command to the BMC.

The standard code path for SOL logic follows:
    ipmi_sol_main (ipmi_sol.c):

    ipmi_sol_activate (ipmi_sol.c):
        Argument validation
        Creation and dispatch of IPMI Activate Payload command

    ipmi_sol_red_pill (ipmi_sol.c):
        Loop on select() for user input and data returned from the BMC
        Periodic dispatch of "keep alive" packet to the BMC.
        Send user input to the BMC and BMC data to the console.
        
        processSolUserInput (ipmi_sol.c):
            Process possible escape sequences (~., ~B, etc.)
            Send (with retries) user data to the BMC
            Partial creation of packet payload

                ipmi_lanplus_send_sol (lanplus.c):
                    Completion of packet payload
                    Send (with retries) of SOL packet

                     ipmi_lanplus_send_payload (lanplus.c):
                         Creation of RMCP+ packet
                         Details general to all V2 packet processing, as
                         well as a some logic to handle ack reception.

                     is_sol_partial_ack (lanplus.c):
                         Determine whether a data needs to be resent

        ipmi_lanplus_recv_sol (lanplus.c):
            Handle data received by the BMC.  Ack as appropriate.

