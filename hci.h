/* 5.4.1 HCI Command Packet */

/* OpCode Group Field (OGF) and OpCode Command Field (OCF). */

#define	OPCODE(ogf, ocf)		(ocf | (ogf << 10))

#define	OGF_LE	0x08

#define	HCI_LE_READ_LOCAL_FEATURES	OPCODE(OGF_LE, 0x3)
#define	HCI_LE_SET_ADV_PARAMETERS	OPCODE(OGF_LE, 0x6)

struct bt_hci_cmd {
	uint16_t opcode;
	uint8_t params_len;
	void *params;
};

struct bt_hci_evt {
	uint8_t evcode;
	uint8_t params_len;
	void *params;
};

struct adv_param {
	uint16_t interval_min;
	uint16_t interval_max;
	uint8_t adv_type;
	uint8_t own_addr_type;
	uint8_t peer_addr_type;
	uint8_t peer_addr[6];
	uint8_t channel_map;
	uint8_t filter_policy;
};
