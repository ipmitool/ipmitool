/*
 * Copyright (c) 2015 IBM Corporation
 * Copyright (c) 2019 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>

#include <systemd/sd-bus.h>

#include <ipmitool/log.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>

static sd_bus *bus;

static
struct ipmi_rs *
ipmi_dbus_sendrecv(struct ipmi_intf *intf,
                   struct ipmi_rq *req)
{
	static const char *destination = "xyz.openbmc_project.Ipmi.Host";
	static const char *object_path = "/xyz/openbmc_project/Ipmi";
	static const char *interface = "xyz.openbmc_project.Ipmi.Server";
	static const char *method_name = "execute";
	static const char SD_BUS_TYPE_3_BYTES[] = {
		SD_BUS_TYPE_BYTE, SD_BUS_TYPE_BYTE, SD_BUS_TYPE_BYTE, 0
	};
	static const char SD_BUS_TYPE_4_BYTES[] = {
		SD_BUS_TYPE_BYTE, SD_BUS_TYPE_BYTE,
		SD_BUS_TYPE_BYTE, SD_BUS_TYPE_BYTE, 0
	};
	static const char SD_BUS_TYPE_DICT_OF_VARIANTS[] = {
		SD_BUS_TYPE_ARRAY,
		SD_BUS_TYPE_DICT_ENTRY_BEGIN,
		SD_BUS_TYPE_STRING, SD_BUS_TYPE_VARIANT,
		SD_BUS_TYPE_DICT_ENTRY_END, 0
	};
	static const char SD_BUS_TYPE_IPMI_RESPONSE[] = {
		SD_BUS_TYPE_BYTE, SD_BUS_TYPE_BYTE,
		SD_BUS_TYPE_BYTE, SD_BUS_TYPE_BYTE,
		SD_BUS_TYPE_ARRAY, SD_BUS_TYPE_BYTE, 0
	};

	sd_bus_message *request = NULL;
	int rc;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message* reply = NULL;
	uint8_t recv_netfn;
	uint8_t recv_lun;
	uint8_t recv_cmd;
	uint8_t recv_cc;
	const void *data;
	size_t data_len;
	static struct ipmi_rs rsp;
	struct ipmi_rs *ipmi_response = NULL;

	if (!intf->opened || !bus)
	{
		goto out_no_free;
	}

	rsp.ccode = IPMI_CC_UNSPECIFIED_ERROR;
	rsp.data_len = 0;
	memset(rsp.data, 0, sizeof(rsp.data));

	/* The D-Bus xyz.openbmc_project.Ipmi.Server.execute interface
	 * looks like this:
	 *
	 * Request:
	 *   byte: net function
	 *   byte: lun
	 *   byte: command
	 *   byte array: data (possibly zero length)
	 *   array of (string,variant): options
	 * Response:
	 *   byte: net function
	 *   byte: lun
	 *   byte: command
	 *   byte: completion code
	 *   byte array: response data (possibly zero length)
	 */
	rc = sd_bus_message_new_method_call(bus, &request, destination,
	                                    object_path, interface,
					    method_name);
	if (rc < 0) {
		lprintf(LOG_ERR, "%s: failed to create message: %s\n",
		        __func__, strerror(-rc));
		goto out_no_free;
	}
	/* pack the header: netfn, lun, cmd */
	rc = sd_bus_message_append(request, SD_BUS_TYPE_3_BYTES, req->msg.netfn,
	                           req->msg.lun, req->msg.cmd);
	if (rc < 0) {
		lprintf(LOG_ERR, "%s: failed to append parameters\n", __func__);
		goto out_free_request;
	}
	/* pack the variable length data */
	rc = sd_bus_message_append_array(request, SD_BUS_TYPE_BYTE,
	                                 req->msg.data, req->msg.data_len);
	if (rc < 0) {
		lprintf(LOG_ERR, "%s: failed to append body\n", __func__);
		goto out_free_request;
	}

	/* Options are only needed for session-based channels, but
	 * in order to fulfill the correct signature, an empty array
	 * must be packed */
	rc = sd_bus_message_append(request, SD_BUS_TYPE_DICT_OF_VARIANTS,
	                           NULL, 0);
	if (rc < 0) {
		lprintf(LOG_ERR, "%s: failed to append options\n", __func__);
		goto out_free_request;
	}

	rc = sd_bus_call(bus, request, 0, &error, &reply);
	if (rc < 0) {
		lprintf(LOG_ERR, "%s: failed to send dbus message (%s)\n",
		        __func__, error.message);
		goto out_free_request;
	}

	/* unpack the response; check that it has the expected types */
	rc = sd_bus_message_enter_container(reply, SD_BUS_TYPE_STRUCT,
	                                    SD_BUS_TYPE_IPMI_RESPONSE);
	if (rc < 0) {
		lprintf(LOG_ERR, "%s: failed to parse reply\n", __func__);
		goto out_free_reply;
	}
	/* read the header: CC netfn lun cmd */
	rc = sd_bus_message_read(reply, SD_BUS_TYPE_4_BYTES, &recv_netfn,
	                         &recv_lun, &recv_cmd, &recv_cc);
	if (rc < 0) {
		lprintf(LOG_ERR, "%s: failed to read reply\n", __func__);
		goto out_free_reply;
	}
	/* read the variable length data */
	rc = sd_bus_message_read_array(reply, SD_BUS_TYPE_BYTE,
	                               &data, &data_len);
	if (rc < 0) {
		lprintf(LOG_ERR, "%s: failed to read reply data\n", __func__);
		goto out_free_reply;
	}
	rc = sd_bus_message_exit_container(reply);
	if (rc < 0) {
		lprintf(LOG_ERR, "%s: final unpack of message failed\n",
		        __func__);
		goto out_free_reply;
	}

	if (data_len > sizeof(rsp.data)) {
		lprintf(LOG_ERR, "%s: data too long!\n", __func__);
		goto out_free_reply;
	}

	/* At this point, all the parts are available for a response
	 * other than unspecified error. */
	rsp.ccode = recv_cc;
	rsp.data_len = data_len;
	memcpy(rsp.data, data, data_len);
	ipmi_response = &rsp;

out_free_reply:
	/* message unref will free resources owned by the message */
	sd_bus_message_unref(reply);
out_free_request:
	sd_bus_message_unref(request);
out_no_free:
	return ipmi_response;
}

static
int
ipmi_dbus_setup(struct ipmi_intf *intf)
{
	int rc;

	rc = sd_bus_default_system(&bus);
	if (rc < 0) {
		lprintf(LOG_ERR, "Can't connect to session bus: %s\n",
		        strerror(-rc));
		return -1;
	}
	intf->opened = 1;

	return 0;
}

static
void
ipmi_dbus_close(struct ipmi_intf *intf)
{
	if (intf->opened)
	{
		sd_bus_close(bus);
	}
	intf->opened = 0;
}

struct ipmi_intf ipmi_dbus_intf = {
	.name = "dbus",
	.desc = "OpenBMC D-Bus interface",
	.setup = ipmi_dbus_setup,
	.close = ipmi_dbus_close,
	.sendrecv = ipmi_dbus_sendrecv,
};


