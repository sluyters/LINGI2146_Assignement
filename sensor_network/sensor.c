/*
 * GOOD LINK SYMPA https://github.com/contiki-os/contiki/blob/master/examples/rime/example-neighbors.c
 */

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include <time.h>
#include <string.h>

#include "message.h"

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
/* Sensor settings */
int send_data = 0; // By default, don't send data
int send_periodically = 0; // By default, send data only when there is a change (not periodically)

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
int aggregate_msg_timestamp = 0;

/*-----------------------------------------------------------------------------*/
/* Declaration of the processes */
// TODO
PROCESS(broadcast_process, "Broadcast process");
AUTOSTART_PROCESSES(&broadcast_process);

/*-----------------------------------------------------------------------------*/
/* Helper functions */
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

static void handle_tree_advertisement_msg(struct message *msg) {
	// Check version, if version >= local version, process the TREE_ADVERTISEMENT message
	if (msg.payload.tree_version >= tree_version) {
		// Check if new neighbor is better than current parent (automatically better if tree version is greater)
		if ((msg.payload.tree_version > tree_version || tree_version - msg.payload.tree_version > 245) || ((parent == NULL || msg.payload.n_hops < parent.n_hops) && get_child(msg.payload.source_id) == NULL)) {
			if (parent == NULL) {
				parent = (struct node *) malloc(sizeof(struct node));
				parent.next = NULL;
			}
			parent.addr = *from;		// Not sure
			parent.n_hops = msg.payload.n_hops + 1;
			// Send a DESTINATION_ADVERTISEMENT message to the new parent node
			struct message *dest_msg = (struct message *) malloc(sizeof(struct message));	
			get_msg(dest_msg, DESTINATION_ADVERTISEMENT);
			char *encoded_msg;
			uint32_t len = encode_message(dest_msg, encoded_msg);
			packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet
			runicast_send(&runicast, from, 1);
			free(encoded_msg);
			free_message(dest_msg);
			// Broadcast the new tree
			struct message *ad_msg = (struct message *) malloc(sizeof(struct message));	
			get_msg(ad_msg, TREE_ADVERTISEMENT);
			uint32_t len = encode_message(ad_msg, encoded_msg);
			packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet				
			broadcast_send(&broadcast);
			free(encoded_msg);
			free_message(ad_msg);
		} else if (parent.node_id == msg.payload.source_id)	{
			// Update the informations of the parent
			parent.n_hops = msg.payload.n_hops + 1;
			// Broadcast the new tree
			struct message *ad_msg = (struct message *) malloc(sizeof(struct message));	
			get_msg(&ad_msg, TREE_ADVERTISEMENT);
			char *encoded_msg;
			uint32_t len = encode_message(&ad_msg, encoded_msg);
			packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet				
			broadcast_send(&broadcast);
			free(encoded_msg);
			free_message(ad_msg);
		}
		// Update tree version + consider the tree as stable
		tree_version = msg.payload.tree_version;
		tree_stable = 1;
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
			if ((decoded_msg.payload.request_attributes & 0x1) == 0x1) {
				if (decoded_msg.payload.tree_version > tree_version || (tree_stable && decoded_msg.payload.tree_version == tree_version)) {
					tree_stable = 0;
					// Broadcast TREE_INFORMATION_REQUEST message to destroy the tree
					packetbuf_copyfrom(encoded_msg, packetbuf_datalen());
					broadcast_send(&broadcast);
				}
			} else {
				if (parent != NULL && decoded_msg.payload.tree_version <= tree_version) {
					// Send TREE_ADVERTISEMENT response
					struct message *msg = (struct message *) malloc(sizeof(struct message));
					get_msg(msg, TREE_ADVERTISEMENT);
					char *encoded_msg;
					uint32_t len = encode_message(msg, encoded_msg);
					packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet
					runicast_send(&runicast, from, 1);	
					free(encoded_msg);
					free_message(msg);
				}
			}
			break;
		case TREE_ADVERTISEMENT:
			handle_tree_advertisement_msg(decoded_msg);
			free_message(decoded_msg)
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
				// Store this message while waiting for other messages to aggregate
				data_aggregate_msg = decoded_msg;
				aggregate_msg_timestamp = (int) time();
			} else {
				// If too many messages already aggregated, send old messages + save this one as new aggregated message
				if (data_aggregate_msg.header.length + decoded_msg.header.length > 128) {
					char *encoded_msg;
					uint32_t len = encode_message(data_aggregate_msg, encoded_msg);
					packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet
					runicast_send(&runicast, &(parent.addr_via), 1);
					free_message(data_aggregate_msg);
					free(encoded_msg);
					data_aggregate_msg = decoded_msg;
					aggregate_msg_timestamp = (int) time();
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
			break;
		case SENSOR_CONTROL:
			// Check if message is destined to this sensor
			if (my_id == decoded_msg.payload.destination_id) {
				// Adapt sensor setting (each control message must contain all settings)
				send_data = decoded_msg.payload.command & 0x1;
				send_periodically = (decoded_msg.payload.command & 0x2) >> 1;

			} else {
				struct node* child = get_child(decoded_msg.payload.destination_id);
				if (child != NULL) {
					// Forward control message to child
					packetbuf_copyfrom(msg, packetbuf_datalen());
					runicast_send(&runicast, &(child.addr_via), 1);
				}
			}
			free_message(decoded_msg);
			break;
		case TREE_ADVERTISEMENT:
			handle_tree_advertisement_msg(decoded_msg);
			free_message(decoded_msg)
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
 
