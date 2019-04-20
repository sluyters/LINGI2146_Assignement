#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include <time.h>
#include <string.h>

//#define DEBUG DEBUG_PRINT

#include "net/rime/rime.h"	

/*-----------------------------------------------------------------------------*/
/* Structure of the messages */
enum {
	DESTINATION_ADVERTISEMENT, 
	TREE_ADVERTISEMENT, 
	TREE_INFORMATION_REQUEST,
	SENSOR_DATA,
	SENSOR_CONTROL
};

// I think there is a better way to do this
struct msg_header {
	uint8_t version;	// Version of the protocol (less bits ?)
	uint8_t msg_type;	// Type of the message
	uint16_t length;	// Length of the payload
}

struct msg_dest_ad_payload {
	uint8_t tree_version;
	uint8_t source_id;
}

struct msg_tree_ad_payload {
	uint8_t tree_version;
	uint8_t source_id;
	uint8_t n_hops;
}

struct msg_tree_request_payload {
	uint8_t tree_version;
	uint8_t request_attributes;		// if request_attributes & 0x1 == 1 => tree_broken
}

struct msg_data_payload_h {
	uint8_t source_id;
	uint8_t topic_id;
	uint8_t length;
}

struct msg_data_payload {
	struct msg_data_payload_h *data_header;
	void *data;
	struct msg_data_payload *next;
}

struct msg_control_payload {
	uint8_t destination_id;
	uint8_t command;
}

struct message {
	struct msg_header *header;
	void *payload;
}

/*-----------------------------------------------------------------------------*/
/* Functions */

static uint32_t encode_message(struct message *decoded_msg, char *encoded_msg);

static void decode_message(struct message *decoded_msg, char *encoded_msg, uint16_t msg_len); 

static void free_message(struct message *msg);
