/*
 * GOOD LINK SYMPA https://github.com/contiki-os/contiki/blob/master/examples/rime/example-neighbors.c
 */

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include <string.h>

//#define DEBUG DEBUG_PRINT

#include "net/rime/rime.h"	
/*-----------------------------------------------------------------------------*/
/* Configuration values */
const uint16_t runicast_channel = 142;
const uint16_t broadcast_channel = 169;
const uint8_t version = 1;

struct runicast_conn runicast;
struct broadcast_conn broadcast;

/*-----------------------------------------------------------------------------*/
/* Structure of the messages */
enum {
	DESTINATION_ADVERTISEMENT, 
	TREE_ADVERTISEMENT, 
	TREE_INFORMATION_REQUEST,
	SENSOR_DATA,
	SENSOR_CONTROL
};

struct msg_header {
	uint8_t version;
	uint8_t msg_type;
}

struct msg_dest_ad_payload {
	uint8_t source_id;
}

struct msg_tree_ad_payload {
	uint8_t n_hops;
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
	uint32_t length;
	struct msg_header *header;
	void *payload;
}

/*-----------------------------------------------------------------------------*/
/* Save parent + child nodes */
struct node {
	struct node *next;
	rimeaddr_t addr;
	uint8_t n_hops;
}

struct node *parent;
struct node *childs;

/*-----------------------------------------------------------------------------*/
/* Declaration of the processes */
// TODO
PROCESS(broadcast_process, "Broadcast process");
AUTOSTART_PROCESSES(&broadcast_process);

/*-----------------------------------------------------------------------------*/
/* Helper functions */
static void encode_message(struct message *decoded_msg, char *encoded_msg) {
	// Allocate memory for encoded message
	encoded_msg = (char *) malloc(decoded_msg.length);
	offset = 0;
	// Encode the header
	memcpy(encoded_msg, (void *) decoded_msg.header, sizeof(struct msg_header));
	offset += sizeof(struct msg_header);
	// Encode the payload
	switch (decoded_msg.header.msg_type) {
		case DESTINATION_ADVERTISEMENT:
			memcpy(encoded_msg + offset, (void *) decoded_msg.payload, sizeof(struct msg_dest_ad_payload));
			break; 
		case TREE_ADVERTISEMENT:
			memcpy(encoded_msg + offset, (void *) decoded_msg.payload, sizeof(struct msg_tree_ad_payload));
			break; 
		case TREE_INFORMATION_REQUEST:
			// Do nothing (no payload)
			break;
		case SENSOR_DATA:
			// Copy all payload data
			struct msg_data_payload *current = decoded_msg.payload;
			while (current != NULL) {
				memcpy(encoded_msg + offset, (void *) current.data_header, sizeof(struct msg_data_payload_h));
				offset += sizeof(struct msg_data_payload_h);
				memcpy(encoded_msg + offset, (void *) current.data, current.data_header.length);
				offset += current.data_header.length;
				current = current.next;
			}
			break;
		case SENSOR_CONTROL:
			memcpy(encoded_msg + offset, (void *) decoded_msg.payload, sizeof(struct msg_control_payload));
			break;
		default:	
	}
}

static void decode_message(struct message *decoded_msg, char *encoded_msg, uint16_t msg_len) {
	int offset = 0;
	// Allocate memory for decoded message
	decoded_msg = (struct message *) malloc(sizeof(struct message));
	decoded_msg.length = msg_len;
	decoded_msg.header = (struct msg_header *) malloc(sizeof(struct msg_header));
	// Decode the header
	memcpy(decoded_msg.header, (void *) encoded_msg, sizeof(struct msg_header));
	offset += sizeof(struct msg_header);
	// Decode the payload
	switch (decoded_msg.header.msg_type) {
		case DESTINATION_ADVERTISEMENT:
			struct msg_dest_ad_payload *payload = (struct msg_dest_ad_payload *) malloc(sizeof(struct msg_dest_ad_payload));
			memcpy(payload, (void *) encoded_msg + offset, sizeof(struct msg_dest_ad_payload));
			decoded_msg.payload = payload;
			break; 
		case TREE_ADVERTISEMENT:
			struct msg_tree_ad_payload *payload = (struct msg_tree_ad_payload *) malloc(sizeof(struct msg_tree_ad_payload));
			memcpy(payload, (void *) encoded_msg + offset, sizeof(struct msg_tree_ad_payload));
			decoded_msg.payload = payload;
			break; 
		case TREE_INFORMATION_REQUEST:
			decoded_msg.payload = NULL;
			break;
		case SENSOR_DATA:
			struct msg_data_payload *payload = (struct msg_data_payload *) malloc(sizeof(struct msg_data_payload));
			// Copy data header
			payload.data_header = (struct msg_data_payload_h *) malloc(sizeof(struct msg_data_payload_h));
			memcpy(payload.data_header, (void *) encoded_msg + offset, sizeof(struct msg_data_payload_h));
			offset += sizeof(struct msg_data_payload_h);
			// Copy data payload
			payload.data = (void *) malloc(payload.data_header.length);
			memcpy(payload.data, (void *) encoded_msg + offset, payload.data_header.length));
			offset += payload.data_header.length;
			decoded_msg.payload = payload;

			while (offset < msg_len) {
				// Set next data payload
				payload.next = (struct msg_data_payload *) malloc(sizeof(struct msg_data_payload));
				payload = payload.next;
				// Copy data header
				payload.data_header = (struct msg_data_payload_h *) malloc(sizeof(struct msg_data_payload_h));
				memcpy(payload.data_header, (void *) encoded_msg + offset, sizeof(struct msg_data_payload_h));
				offset += sizeof(struct msg_data_payload_h);
				// Copy data payload
				payload.data = (void *) malloc(payload.data_header.length);
				memcpy(payload.data, (void *) encoded_msg + offset, payload.data_header.length));
				offset += payload.data_header.length;
			}
			payload.next = NULL;
			break;
		case SENSOR_CONTROL:
			struct msg_control_payload *payload = (struct msg_control_payload *) malloc(sizeof(struct msg_control_payload));
			memcpy(payload, (void *) encoded_msg + offset, sizeof(struct msg_control_payload));
			decoded_msg.payload = payload;
			break;
		default:	
	}
}

static void free_message(struct message *msg) {
	free(msg.header);
	if (msg.payload != NULL) {
		if (msg.header.msg_type == SENSOR_DATA) {
			// Free all aggregate data
			struct msg_data_payload *current = msg.payload;
			struct msg_data_payload *previous;
			while (current != NULL) {
				free(current->data);
				free(current->data_header);
				previous = current;
				current = current.next;
				free(previous);
			}
		} else {
			free(msg.payload);
		}
	}
	free(msg);
}

/*-----------------------------------------------------------------------------*/
/* Callback function when a broadcast message is received */
static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from) {
	// Decode the message
	char *msg = packetbuf_dataptr();
	struct message *decoded_msg;
	decode_message(struct message *decoded_msg, msg, packetbuf_datalen());

	if (decoded_msg.header.msg_type == TREE_INFORMATION_REQUEST) {
		if (parent != NULL) {
			// TODO Send TREE_ADVERTISEMENT response
			struct message *msg = (struct message *) malloc(sizeof(struct message));;
			msg.header = (struct msg_header *) malloc(sizeof(struct msg_header));
			msg.header.version = version;
			msg.header.type = TREE_ADVERTISEMENT;
			msg.length = sizeof(struct msg_header);
			// TODO Add content to response
			char *encoded_msg;
			encode_message(msg, encoded_msg);
			packetbuf_copyfrom(encoded_msg, sizeof(encoded_msg));	// Put data inside the packet
			runicast_send(&runicast, from, 1);
		}
	} else if (decoded_msg.header.msg_type == TREE_ADVERTISEMENT) {
		if (parent == NULL) {
			// TODO Check if the new parent is not a child + check if malloc OK
			parent = (struct node *) malloc(sizeof(struct node));
			parent.addr = *from;		// Not sure
			parent.next = NULL;
			parent.n_hops = 
			// Send a DESTINATION_ADVERTISEMENT message to the new parent node
			struct message *msg = (struct message *) malloc(sizeof(struct message));;
			msg.header = (struct msg_header *) malloc(sizeof(struct msg_header));
			msg.header.version = version;
			msg.header.type = DESTINATION_ADVERTISEMENT;
			char *encoded_msg;
			encode_message(msg, encoded_msg);
			packetbuf_copyfrom(encoded_msg, sizeof(encoded_msg));	// Put data inside the packet
			runicast_send(&runicast, from, 1);
		} else {
			// TODO Check if new neighbor is better than current parent	
	
		}
	}
}

// Set the function to be called when a broadcast message is received
static const struct broadcast_callbacks bc = {broadcast_recv};

/*-----------------------------------------------------------------------------*/
/* Callback function when a unicast message is received */
static void runicast_recv(struct runicast_conn *c, const rimeaddr_t *from) {
	// Decode the message
	char *msg = packetbuf_dataptr();
	struct message *decoded_msg;
	decode_message(struct message *decoded_msg, msg, packetbuf_datalen());

	if (decoded_msg.header.msg_type == DESTINATION_ADVERTISEMENT) {
		// TODO Add to list of childs + 
		// TODO Forward message to parent
	} else if (decoded_msg.header.msg_type == SENSOR_DATA) {
		// TODO Forward data message to root (+ aggregate)
	} else if (decoded_msg.header.msg_type == SENSOR_CONTROL) {
		// TODO Forward control message to correct destination (how to know where to forward if not direct child ?)
	}
}

// Set the function to be called when a broadcast message is received
static const struct runicast_callbacks rc = {runicast_recv}

// TODO Open unicast -> runicast_open();

/*-----------------------------------------------------------------------------*/
/* Process */
// Broadcast process
PROCESS_THREAD(broadcast_process, ev, data)
{
	// TODO
}
