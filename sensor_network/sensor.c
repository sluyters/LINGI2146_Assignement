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

struct message {
	uint8_t version;
	uint8_t msg_type;
	uint8
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
}

static void decode_message(struct message *decoded_msg, char *encoded_msg) {
}

/*-----------------------------------------------------------------------------*/
/* Callback function when a broadcast message is received */
static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from) {
	if (parent == NULL) {
		// TODO Check if the new parent is not a child + check if malloc OK
		parent = (struct node *) malloc(sizeof(struct node));
		parent.addr = *from;		// Not sure
		parent.next = NULL;
		parent.n_hops = 
		// Send a DESTINATION_ADVERTISEMENT message to the new parent node
		struct message msg;
		msg.version = version;
		msg.type = DESTINATION_ADVERTISEMENT;
		char *encoded_msg;
		encode_message(msg, encoded_msg);
		packetbuf_copyfrom(encoded_msg, sizeof(encoded_msg));	// Put data inside the packet
		runicast_send(&runicast, from, 1);
	} else {	
	
	}
}

// Set the function to be called when a broadcast message is received
static const struct broadcast_callbacks bc = {broadcast_recv};

/*-----------------------------------------------------------------------------*/
/* Callback function when a unicast message is received */
static void runicast_recv(struct runicast_conn *c, const rimeaddr_t *from) {
	// TODO
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
