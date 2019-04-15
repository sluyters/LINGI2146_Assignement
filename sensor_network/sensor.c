/*
 * GOOD LINK SYMPA https://github.com/contiki-os/contiki/blob/master/examples/rime/example-neighbors.c
 */

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include <time.h>
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

// TODO remove
struct msg_tree_breakdown_payload {
	uint8_t tree_version;
	uint8_t source_id;
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
/* Save parent + child nodes */
struct node {
	struct node *next;
	rimeaddr_t addr_via;
	uint8_t node_id;
	uint8_t n_hops;
	int timestamp;
}

struct node *parent = NULL;
struct node *childs = NULL;

uint8_t my_id = 42; // TODO Modify this
uint8_t tree_version = 0; 
int tree_stable = 0; // A the beginning, the tree is not stable 

/*-----------------------------------------------------------------------------*/
/* Message with aggregated data */
struct message *data_aggregate_msg = NULL;

/*-----------------------------------------------------------------------------*/
/* Declaration of the processes */
// TODO
PROCESS(broadcast_process, "Broadcast process");
AUTOSTART_PROCESSES(&broadcast_process);

/*-----------------------------------------------------------------------------*/
/* Helper functions */
static uint32_t encode_message(struct message *decoded_msg, char *encoded_msg) {
	uint32_t length = decoded_msg.header.length + sizeof(struct msg_header);
	// Allocate memory for encoded message
	encoded_msg = (char *) malloc(length); // TODO make allocation outside of the function ?
	offset = 0;
	// Encode the header
	memcpy(encoded_msg, (void *) decoded_msg.header, sizeof(struct msg_header));
	offset += sizeof(struct msg_header);
	// Encode the payload
	switch (decoded_msg.header.msg_type) {
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
		default:
			memcpy(encoded_msg + offset, (void *) decoded_msg.payload, decoded_msg.header.length);	
	}
	return length;
}

static void decode_message(struct message *decoded_msg, char *encoded_msg, uint16_t msg_len) {
	int offset = 0;
	// Allocate memory for decoded message
	decoded_msg = (struct message *) malloc(sizeof(struct message));	// TODO make allocation outside of the function
	decoded_msg.header = (struct msg_header *) malloc(sizeof(struct msg_header));
	// Decode the header
	memcpy(decoded_msg.header, (void *) encoded_msg, sizeof(struct msg_header));
	offset += sizeof(struct msg_header);
	decoded_msg.header.length = msg_len - offset;
	// Decode the payload
	switch (decoded_msg.header.msg_type) {
		case DESTINATION_ADVERTISEMENT:
			struct msg_dest_ad_payload *payload = (struct msg_dest_ad_payload *) malloc(decoded_msg.header.length);
			memcpy(payload, (void *) encoded_msg + offset, decoded_msg.header.length);
			decoded_msg.payload = payload;
			break; 
		case TREE_ADVERTISEMENT:
			struct msg_tree_ad_payload *payload = (struct msg_tree_ad_payload *) malloc(decoded_msg.header.length);
			memcpy(payload, (void *) encoded_msg + offset, decoded_msg.header.length);
			decoded_msg.payload = payload;
			break; 
		case TREE_INFORMATION_REQUEST:
			struct msg_tree_ad_payload *payload = (struct msg_tree_request_payload *) malloc(decoded_msg.header.length);
			memcpy(payload, (void *) encoded_msg + offset, decoded_msg.header.length);
			decoded_msg.payload = payload;
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
			struct msg_control_payload *payload = (struct msg_control_payload *) malloc(decoded_msg.header.length);
			memcpy(payload, (void *) encoded_msg + offset, sizeof(decoded_msg.header.length);
			decoded_msg.payload = payload;
			break;
		default:	
	}
}

static void free_message(struct message *msg) {
	free(msg.header);
	if (msg.payload != NULL) {
		if (msg.header.msg_type == SENSOR_DATA) {
			// Free all aggregated data
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

static void add_child(rimeaddr_t *addr_via, uint8_t node_id) {
	// Add the new child or update its data if already present
	if (childs == NULL) {
		childs = (node *) malloc(sizeof(struct node));
		childs.addr_via = *addr_via;			// Not sure
		childs.node_id = node_id;
		childs.next = NULL;
		childs.timestamp = (int) time();
	} else {
		struct node *current = childs;
		struct node *previous;
		while (current != NULL) {
			if (current.node_id == node_id) {
				// Remove this child, it will be added at the end of the queue later
				previous.next = current.next;
				free(current);
				current = previous.next;
			} else {
				previous = current;
				current = current.next;
			}
		}
		// Add new child
		struct node *new_child = (node *) malloc(sizeof(struct node));
		new_child.addr_via = *addr_via;			// Not sure
		new_child.node_id = node_id;
		new_child.next = NULL;
		new_child.timestamp = (int) time();
	}
}

static void remove_child(uint8_t node_id) {
	if (childs != NULL && childs.node_id == node_id) {
		// The child to delete is the first node
		struct node *deleted_child = childs;
		childs = childs.next;
		free(deleted_child);
	} else if (childs != NULL) {
		struct node *current = childs.next;
		struct node *previous = childs;
		while (current != NULL) {
			if (current.node_id == node_id) {
				// Delete child
				previous.next = current.next;
				free(current);
				return;	
			}
			previous = current;
			current = current.next;
		}
	}
}

static node *get_child(uint8_t node_id) {
	struct node *current = childs;
	while (current != NULL) {
		if (current.node_id == node_id) {
			return current;
		}
		current = current.next;
	}
	return NULL;
}

// Useless for now (perhaps useful later ?)
/*
static void send_to_childs(void *msg, int length) {
	struct node *current = childs;
	while (current != NULL) {
		packetbuf_copyfrom(encoded_msg, length);	// Put data inside the packet
		runicast_send(&runicast, &(current.addr_via), 1); 
	}
}
*/

static void get_msg(struct message *msg, int msg_type) {
	msg.header = (struct msg_header *) malloc(sizeof(struct msg_header));
	msg.header.version = version;
	msg.header.type = msg_type;
	switch (msg_type) {
		case TREE_ADVERTISEMENT:
			msg.payload = (struct msg_tree_ad_payload *) malloc(sizeof(struct msg_tree_ad_payload));
			msg.payload.n_hops = parent.n_hops;
			msg.payload.source_id = my_id;
			msg.payload.tree_version = tree_version;
			msg.header.length = sizeof(struct msg_tree_ad_payload);
			break;
		case DESTINATION_ADVERTISEMENT:
			msg.payload = (struct msg_dest_ad_payload *) malloc(sizeof(struct msg_dest_ad_payload));
			msg.payload.source_id = my_id;
			msg.payload.tree_version = tree_version;
			msg.header.length = sizeof(struct msg_dest_ad_payload);
			break;
		case TREE_INFORMATION_REQUEST:
			msg.payload = (struct msg_tree_request_payload *) malloc(sizeof(struct msg_tree_request_payload));
			msg.payload.tree_version = tree_version;
			msg.payload.request_attributes = tree_stable;
			msg.header.length = sizeof(struct msg_tree_request_payload);
			break;
		default:
	}
}

/*-----------------------------------------------------------------------------*/
/* Callback function when a broadcast message is received */
static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from) {
	// Decode the message
	char *encoded_msg = packetbuf_dataptr();
	struct message *decoded_msg;
	decode_message(decoded_msg, encoded_msg, packetbuf_datalen());
	
	switch (decoded_msg.header.msg_type) {
		case TREE_INFORMATION_REQUEST:
			// If the tree needs rebuilding
			if (decoded_msg.payload.request_attributes & 0x1 == 0x1) {
				// TODO what happens if 2 rebuilding requests are received, one with same version and one with higher version. Will the most recent request still be received by the root ?
				if (tree_stable && decoded_msg.payload.tree_version >= tree_version) {
					tree_stable = 0;
					// Broadcast TREE_INFORMATION_REQUEST message to destroy the tree
					packetbuf_copyfrom(encoded_msg, packetbuf_datalen());
					broadcast_send(&broadcast);
				}
			} else {
				if (parent != NULL && decoded_msg.payload.tree_version <= tree_version) {
					// Send TREE_ADVERTISEMENT response
					struct message msg;
					get_msg(&msg, TREE_ADVERTISEMENT);
					char *encoded_msg;
					uint32_t len = encode_message(&msg, encoded_msg);
					packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet
					runicast_send(&runicast, from, 1);	
				}
			}
			break;
		case TREE_ADVERTISEMENT:
			// Check version, if version >= local version, process the TREE_ADVERTISEMENT message
			if (decoded_msg.payload.tree_version >= tree_version) {
				// Check if new neighbor is better than current parent (automatically better if tree version is greater)
				// TODO Handle tree_version overflow
				if (decoded_msg.payload.tree_version > tree_version || ((parent == NULL || decode_msg.payload.n_hops < parent.n_hops) && decoded_msg.payload.tree_version == tree_version && get_child(decoded_msg.payload.source_id) == NULL)) {
					if (parent == NULL) {
						parent = (struct node *) malloc(sizeof(struct node));
						parent.next = NULL;
					}
					parent.addr = *from;		// Not sure
					parent.n_hops = decoded_msg.payload.n_hops + 1;
					// Send a DESTINATION_ADVERTISEMENT message to the new parent node
					struct message msg;
					get_msg(&msg, DESTINATION_ADVERTISEMENT);
					char *encoded_msg;
					uint32_t len = encode_message(msg, encoded_msg);
					packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet
					runicast_send(&runicast, from, 1);
					// Broadcast the new tree
					struct message msg;
					get_msg(&msg, TREE_ADVERTISEMENT);
					char *encoded_msg;
					uint32_t len = encode_message(&msg, encoded_msg);
					packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet				
					broadcast_send(&broadcast);
				} else if (parent.node_id == decoded_msg.payload.source_id)	{
					// Update the informations of the parent
					parent.n_hops = decoded_msg.payload.n_hops + 1;
					// Broadcast the new tree
					struct message msg;
					get_msg(&msg, TREE_ADVERTISEMENT);
					char *encoded_msg;
					uint32_t len = encode_message(&msg, encoded_msg);
					packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet				
					broadcast_send(&broadcast);
				}
				// Update tree version + consider the tree as stable
				tree_version = decoded_msg.payload.tree_version;
				tree_stable = 1;
			}
			break;
		default;
	}
}

// Set the function to be called when a broadcast message is received
static const struct broadcast_callbacks bc = {broadcast_recv};

/*-----------------------------------------------------------------------------*/
/* Callback function when a unicast message is received */
static void runicast_recv(struct runicast_conn *c, const rimeaddr_t *from) {
	// Decode the message
	char *encoded_msg = packetbuf_dataptr();
	struct message *decoded_msg;
	decode_message(struct message *decoded_msg, encoded_msg, packetbuf_datalen());

	switch (decoded_msg.header.msg_type) {
		case DESTINATION_ADVERTISEMENT:
			// Discard if version < local version
			if (decoded_msg.payload.tree_version >= tree_version) {
				// Add to list of childs (or update)
				add_child(from, decoded_msg.payload.source_id);
				// Forward message to parent
				packetbuf_copyfrom(msg, packetbuf_datalen());
				runicast_send(&runicast, &(parent.addr_via), 1);
				free_message(decoded_msg);
			}
			break;
		case SENSOR_DATA:
			if (data_aggregate_msg == NULL) {
				// Store this message while waiting for other messages to aggregate (TODO Add timer ?)
				data_aggregate_msg = decoded_msg;
			} else {
				// If too many messages already aggregated, send old messages + save this one as new aggregated message
				if (data_aggregate_msg.header.length + decoded_msg.header.length > 128) {
					char *encoded_msg;
					uint32_t len = encode_message(data_aggregate_msg, encoded_msg);
					packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet
					free_message(data_aggregate_msg);
					data_aggregate_msg = decoded_msg;
				} else {
					// Add to aggregated message payload
					struct msg_data_payload *current = data_aggregate_msg.payload;
					while (current.next != NULL) {
						current = current.next
					}
					current.next = decoded_msg.payload;
					decoded_msg.payload = NULL; // Avoid conflits when freeing the message
					free_message(decoded_msg);
				}
			}
			// Forward message to parent (temporary simple solution)
			packetbuf_copyfrom(msg, packetbuf_datalen());
			runicast_send(&runicast, &(parent.addr_via), 1);
			break;
		case SENSOR_CONTROL:
			// Check if message is destined to this sensor
			if (my_id == decoded_msg.payload.destination_id) {
				// TODO Adapt sensor setting
			} else {
				struct node* child = get_child(decoded_msg.payload.destination_id);
				if (child != NULL) {
					// Forward control message to child
					packetbuf_copyfrom(msg, packetbuf_datalen());
					runicast_send(&runicast, &(child.addr_via), 1);
				}
			}
			free_message(message);
			break;
		case TREE_ADVERTISEMENT:
			// TODO
		default:
	}
}

// Set the function to be called when a broadcast message is received
static const struct runicast_callbacks rc = {runicast_recv}

// TODO Open unicast -> runicast_open();

/*-----------------------------------------------------------------------------*/
/* Process */
// TODO 1 process for tree building, 1 for sensor data ? or only 1 process ?
// Broadcast process
PROCESS_THREAD(broadcast_process, ev, data)
{
	static struct etimer et;
	// TODO
	while (1) {
		// TODO more specific condition (increase time between broadcasts if no tree ?
		if (parent == NULL) {
			// Broadcast a TREE_INFORMATION_REQUEST
			struct message msg;
			get_msg(&msg, TREE_INFORMATION_REQUEST);
			char *encoded_msg;
			uint32_t len = encode_message(&msg, encoded_msg);
			packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet				
			broadcast_send(&broadcast);
		} 
	}
}

// TODO Process that generates sensor data, sends it to the root if it attached to a tree, broadcasts a TREE_INFORMATION_REQUEST (every 30 secs?) otherwise.
// TODO Regularly broadcast TREE_ADVERTISEMENT messages ?
// TODO How to check if the parent node is still up ? -> TREE_BREAKDOWN_NOTIF sent from parent when a SENSOR_DATA message is received ? -> add tree version ?
 
