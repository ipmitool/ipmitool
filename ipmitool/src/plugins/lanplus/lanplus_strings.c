#include "lanplus.h"

const struct valstr ipmi_rakp_return_codes[] = {

	{ IPMI_RAKP_STATUS_NO_ERRORS,                          "no errors"                           },
	{ IPMI_RAKP_STATUS_INSUFFICIENT_RESOURCES_FOR_SESSION, "insufficient resources for session"  },
	{ IPMI_RAKP_STATUS_INVALID_SESSION_ID,                 "invalid session ID"                  },
	{ IPMI_RAKP_STATUS_INVALID_PAYLOAD_TYPE,               "invalid payload type"                },
	{ IPMI_RAKP_STATUS_INVALID_AUTHENTICATION_ALGORITHM,   "invalid authentication algorithm"    },
	{ IPMI_RAKP_STATUS_INVALID_INTEGRITTY_ALGORITHM,       "invalid integrity algorithm"         },
	{ IPMI_RAKP_STATUS_NO_MATCHING_AUTHENTICATION_PAYLOAD, "no matching authentication algorithm"},
	{ IPMI_RAKP_STATUS_NO_MATCHING_INTEGRITY_PAYLOAD,      "no matching integrity payload"       },
	{ IPMI_RAKP_STATUS_INACTIVE_SESSION_ID,                "inactive session ID"                 },
	{ IPMI_RAKP_STATUS_INVALID_ROLE,                       "invalid role"                        },
	{ IPMI_RAKP_STATUS_UNAUTHORIZED_ROLE_REQUESTED,        "unauthorized role requested"         },
	{ IPMI_RAKP_STATUS_INSUFFICIENT_RESOURCES_FOR_ROLE,    "insufficient resources for role"     },
	{ IPMI_RAKP_STATUS_INVALID_NAME_LENGTH,                "invalid name length"                 },
	{ IPMI_RAKP_STATUS_UNAUTHORIZED_NAME,                  "unauthorized name"                   },
	{ IPMI_RAKP_STATUS_UNAUTHORIZED_GUID,                  "unauthorized GUID"                   },
	{ IPMI_RAKP_STATUS_INVALID_INTEGRITY_CHECK_VALUE,      "invlalid integrity check value"      },
	{ IPMI_RAKP_STATUS_INVALID_CONFIDENTIALITY_ALGORITHM,  "invalid confidentiality algorithm"   },
	{ IPMI_RAKP_STATUS_NO_CIPHER_SUITE_MATCH,              "no matching cipher suite"            },
	{ IPMI_RAKP_STATUS_ILLEGAL_PARAMTER,                   "illegal parameter"                   },
	{ 0,                                                   0                                     },
};


const struct valstr ipmi_priv_levels[] = {
	{ IPMI_PRIV_CALLBACK, "callback" },
	{ IPMI_PRIV_USER,     "user"     },
	{ IPMI_PRIV_OPERATOR, "operator" },
	{ IPMI_PRIV_ADMIN,    "admin"    },
	{ IPMI_PRIV_OEM,      "oem"      },
	{ 0,                  0          },
};


const struct valstr ipmi_auth_algorithms[] = {
	{ IPMI_AUTH_RAKP_NONE,      "none"      },
	{ IPMI_AUTH_RAKP_HMAC_SHA1, "hmac_sha1" },
	{ IPMI_AUTH_RAKP_HMAC_MD5,  "hmac_md5"  },
	{ 0,                        0           },
};

const struct valstr ipmi_integrity_algorithms[] = {
	{ IPMI_INTEGRITY_NONE,         "none" },
	{ IPMI_INTEGRITY_HMAC_SHA1_96, "hmac_sha1_96" },
	{ IPMI_INTEGRITY_HMAC_MD5_128, "hmac_md5_128" },
	{ IPMI_INTEGRITY_MD5_128 ,     "md5_128"      },
	{ 0,                            0             },
};

const struct valstr ipmi_encryption_algorithms[] = {
	{ IPMI_CRYPT_NONE,        "none"        },
	{ IPMI_CRYPT_AES_CBC_128, "aes_cbc_128" },
	{ IPMI_CRYPT_XRC4_128,    "xrc4_128"    },
	{ IPMI_CRYPT_XRC4_40,     "xrc4_40"     },
	{ 0,                      0             },
};

