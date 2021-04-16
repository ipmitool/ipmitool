#pragma once

#define IPMI_DUMMY_DEFAULTSOCK "/tmp/.ipmi_dummy"

struct dummy_rq {
	struct {
		uint8_t netfn;
		uint8_t lun;
		uint8_t cmd;
		uint8_t target_cmd;
		uint16_t data_len;
		uint8_t *data;
	} msg;
};

struct dummy_rs {
	struct {
		uint8_t netfn;
		uint8_t cmd;
		uint8_t seq;
		uint8_t lun;
	} msg;

	uint8_t ccode;
	int data_len;
	uint8_t *data;
};
