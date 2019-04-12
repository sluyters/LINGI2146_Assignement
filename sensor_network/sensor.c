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
	TREE_INFORMATION_REQUEST
};

struct msg_header {
	uint8_t version;
	uint8_t msg_type;
}

struct message {
	struct msg_header *header;
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
	encoded_msg = (char *) malloc();
	// Encode the header
	memcpy(encoded_msg, (void *) decoded_msg.header, sizeof(struct msg_header));
	// TODO Encode the payload (if any)
}

static void decode_message(struct message *decoded_msg, char *encoded_msg) {
	// Allocate memory for decoded message
	decoded_msg = (struct message *) malloc(sizeof(struct message));
	decoded_msg.header = (struct msg_header *) malloc(sizeof(struct msg_header));
	// Decode the header
	memcpy(decoded_msg.header, (void *) encoded_msg, sizeof(struct msg_header));
}

static void free_message(struct message *msg) {
	free(msg.header);
	// TODO Free Payload (if any)
	free(msg);
}

/*-----------------------------------------------------------------------------*/
/* Callback function when a broadcast message is received */
static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from) {
	// Decode the message
	char *msg = packetbuf_dataptr();
	struct message *decoded_msg;
	decode_message(struct message *decoded_msg, msg);

	if (decoded_msg.header.msg_type == TREE_INFORMATION_REQUEST) {
		if (parent != NULL) {
			// TODO Send TREE_ADVERTISEMENT response
			struct message *msg = (struct message *) malloc(sizeof(struct message));;
			msg.header = (struct msg_header *) malloc(sizeof(struct msg_header));
			msg.header.version = version;
			msg.header.type = TREE_ADVERTISEMENT;
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
	decode_message(struct message *decoded_msg, msg);

	if (decoded_msg.header.msg_type == DESTINATION_ADVERTISEMENT) {
		// TODO Add to list of childs + forward message to parent
	} else if (decoded_msg.header.msg_type == ) {
		// TODO Forward data message to root (+ aggregate)
	} else if (decoded_msg.header.msg_type == ) {
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
