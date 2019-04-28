/*
 * GOOD LINK SYMPA https://github.com/contiki-os/contiki/blob/master/examples/rime/example-neighbors.c
 */

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>

#include "message.h"

//#define DEBUG DEBUG_PRINT

#include "net/rime.h"	

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
};

struct node *parent = NULL;
struct node *childs = NULL;

uint8_t my_id = 42; // TODO Modify this
uint8_t tree_version = 0; 
int tree_stable = 0; // A the beginning, the tree is not stable 

/*-----------------------------------------------------------------------------*/
/* Message with aggregated data */
struct message *data_aggregate_msg = NULL;
struct ctimer aggregate_ctimer;

/*-----------------------------------------------------------------------------*/
/* Declaration of the processes */
PROCESS(my_process, "My process");
PROCESS(sensor_process, "Sensor process");
AUTOSTART_PROCESSES(&my_process, &sensor_process);

/*-----------------------------------------------------------------------------*/
/* Helper functions */

/* Callback function when the time of aggregate messages ends */
static void send_aggregate_msg(void *ptr) {
	char *encoded_msg;
	uint32_t len = encode_message(data_aggregate_msg, &encoded_msg);
	packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet
	runicast_send(&runicast, &(parent->addr_via), 1);
	free_message(data_aggregate_msg);
	free(encoded_msg);
	data_aggregate_msg = NULL;
}

/**
 * Adds the new node to the @nodes list, or update its data if it is already present
 */
static void add_node(struct node **nodes, const rimeaddr_t *addr_via, uint8_t node_id, uint8_t n_hops) {
	if (*nodes == NULL) {
		// If the list is empty, create a new node
		*nodes = (struct node *) malloc(sizeof(struct node));
		(*nodes)->addr_via = *addr_via;			// Not sure
		(*nodes)->node_id = node_id;
		(*nodes)->next = NULL;
		(*nodes)->n_hops = n_hops;
		(*nodes)->timestamp = (int) time(NULL);
	} else if ((*nodes)->node_id == node_id && (*nodes)->next == NULL) {
		// If the first node matches node_id and there is no other node, update it
		(*nodes)->addr_via = *addr_via;			// Not sure
		(*nodes)->n_hops = n_hops;
		(*nodes)->timestamp = (int) time(NULL);
	} else {
		// If the list is not empty, check each node until we reach the last node. If a match is found, remove it and add it to the end
		struct node *current = *nodes;
		struct node *previous = current;
		// If the first node matches node_id
		if (current->node_id == node_id) {
			*nodes = current->next;
			free(current);
			current = *nodes;
		}
		while (current != NULL) {
			if (current->node_id == node_id) {
				// Remove this node, it will be added at the end of the queue later
				previous->next = current->next;
				free(current);
				current = previous->next;
			} else {
				previous = current;
				current = current->next;
			}
		}
		// Add new node
		struct node *new_node = (struct node *) malloc(sizeof(struct node));
		new_node->addr_via = *addr_via;			// Not sure
		new_node->node_id = node_id;
		new_node->next = NULL;
		new_node->n_hops = n_hops;
		new_node->timestamp = (int) time(NULL);
	}
}

/**
 * Returns the node corresponding to @node_id from @nodes
 */
static void remove_node(struct node **nodes, uint8_t node_id) {
	if (*nodes != NULL) {
		if ((*nodes)->node_id == node_id) {
			// The node to delete is the first node
			struct node *deleted_node = *nodes;
			*nodes = (*nodes)->next;
			free(deleted_node);
		} else {
			struct node *current = (*nodes)->next;
			struct node *previous = *nodes;
			while (current != NULL) {
				if (current->node_id == node_id) {
					// Delete node
					previous->next = current->next;
					free(current);
					return;	
				}
				previous = current;
				current = current->next;
			}
		}
	}
}

/**
 * Removes all the expired nodes from @nodes
 */
static void remove_expired_nodes(struct node **nodes, int max_elapsed_secs) {
	int now = (int) time(NULL);
	struct node *deleted_node;
	while (*nodes != NULL && (now - (*nodes)->timestamp > max_elapsed_secs)) {
		deleted_node = *nodes;
		*nodes = (*nodes)->next;
		free(deleted_node);
	}
}

/**
 * Returns the node corresponding to |node_id if present in @nodes, NULL otherwise
 */
static struct node *get_node(struct node *nodes, uint8_t node_id) {
	struct node *current = nodes;
	while (current != NULL) {
		if (current->node_id == node_id) {
			return current;
		}
		current = current->next;
	}
	return NULL;
}

// Useless for now (perhaps useful later ?)
/*
static void send_to_childs(void *msg, int length) {
	struct node *current = childs;
	while (current != NULL) {
		packetbuf_copyfrom(encoded_msg, length);	// Put data inside the packet
		runicast_send(&runicast, &(current->addr_via), 1); 
	}
}
*/

/**
 * Initializes a simple message of type @msg_type
 */
static void get_msg(struct message *msg, int msg_type) {
	msg->header = (struct msg_header *) malloc(sizeof(struct msg_header));
	msg->header->version = version;
	msg->header->msg_type = msg_type;
	switch (msg_type) {
		case SENSOR_DATA:;
			msg->payload = NULL;
			msg->header->length = 0;
			break;
		case TREE_ADVERTISEMENT:;
			struct msg_tree_ad_payload *payload_tree_ad = (struct msg_tree_ad_payload *) malloc(sizeof(struct msg_tree_ad_payload));
			payload_tree_ad->n_hops = parent->n_hops;
			payload_tree_ad->source_id = my_id;
			payload_tree_ad->tree_version = tree_version;
			msg->header->length = sizeof(struct msg_tree_ad_payload);
			msg->payload = payload_tree_ad;
			break;
		case DESTINATION_ADVERTISEMENT:;
			struct msg_dest_ad_payload *payload_dest_ad = (struct msg_dest_ad_payload *) malloc(sizeof(struct msg_dest_ad_payload));
			payload_dest_ad->source_id = my_id;
			payload_dest_ad->tree_version = tree_version;
			msg->header->length = sizeof(struct msg_dest_ad_payload);
			msg->payload = payload_dest_ad;
			break;
		case TREE_INFORMATION_REQUEST:;
			struct msg_tree_request_payload *payload_info_req = (struct msg_tree_request_payload *) malloc(sizeof(struct msg_tree_request_payload));
			payload_info_req->tree_version = tree_version;
			if (tree_stable) {
				payload_info_req->request_attributes = 0x0;
			} else {
				payload_info_req->request_attributes = 0x1;
			}
			msg->header->length = sizeof(struct msg_tree_request_payload);
			msg->payload = payload_info_req;
			break;
		default:
			break;
	}
}

/**
 * Sends a simple broadcast message of type @msg_type
 */ 
static void send_broadcast_msg(int msg_type) {
	struct message *msg = (struct message *) malloc(sizeof(struct message));
	char *encoded_msg;	
	get_msg(msg, msg_type);
	uint32_t len = encode_message(msg, &encoded_msg);
	packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet
	broadcast_send(&broadcast);
	free(encoded_msg);
	free_message(msg);
}

/**
 * Sends a simple unicast message of type @msg_type to @addr_dest
 */ 
static void send_runicast_msg(int msg_type, const rimeaddr_t *addr_dest) {
	struct message *msg = (struct message *) malloc(sizeof(struct message));
	char *encoded_msg;	
	get_msg(msg, msg_type);
	uint32_t len = encode_message(msg, &encoded_msg);
	packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet
	runicast_send(&runicast, addr_dest, 1);
	free(encoded_msg);
	free_message(msg);
}

// TODO from undeclared (line 433)
static void handle_tree_advertisement_msg(struct message *msg, const rimeaddr_t *from) {
	// Check version, if version >= local version, process the TREE_ADVERTISEMENT message
	struct msg_tree_ad_payload *payload = (struct msg_tree_ad_payload *) msg->payload;
	if (payload->tree_version >= tree_version || tree_version - payload->tree_version > 245) {
		// Check if new neighbor is better than current parent (automatically better if tree version is greater)
		if ((payload->tree_version > tree_version || tree_version - payload->tree_version > 245) || ((parent == NULL || payload->n_hops < parent->n_hops) && get_node(childs, payload->source_id) == NULL)) {
			if (parent != NULL) {
				remove_node(&parent, parent->node_id);
			}
			// Add the new parent
			add_node(&parent, from, payload->source_id, payload->n_hops + 1);
			// Send a DESTINATION_ADVERTISEMENT message to the new parent node
			send_runicast_msg(DESTINATION_ADVERTISEMENT, from);
			// Broadcast the new tree
			send_broadcast_msg(TREE_ADVERTISEMENT);
			// Update tree version + consider the tree as stable
			tree_version = payload->tree_version;
			tree_stable = 1;
		} else if (parent->node_id == payload->source_id)	{
			// Update the informations of the parent
			add_node(&parent, from, payload->source_id, payload->n_hops + 1);
			// Broadcast the new tree
			send_broadcast_msg(TREE_ADVERTISEMENT);
			// Update tree version + consider the tree as stable
			tree_version = payload->tree_version;
			tree_stable = 1;
		}
	}
}


static void handle_sensor_data_msg(struct message *msg) {
	if (childs == NULL) {
		// If no child, send data directly (no need to aggregate)
		char *encoded_msg;
		uint32_t len = encode_message(msg, &encoded_msg);
		packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet
		runicast_send(&runicast, &(parent->addr_via), 1);
		free_message(msg);
		free(encoded_msg);
	} else if (data_aggregate_msg == NULL) {
		// Store this message while waiting for other messages to aggregate + set timer
		data_aggregate_msg = msg;
		ctimer_set(&aggregate_ctimer, CLOCK_SECOND * 30, send_aggregate_msg, NULL);
	} else if (data_aggregate_msg->header->length + msg->header->length > 128) {
		// If too many messages already aggregated, send old messages + save this one as new aggregated message
		char *encoded_msg;
		uint32_t len = encode_message(data_aggregate_msg, &encoded_msg);
		packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet
		runicast_send(&runicast, &(parent->addr_via), 1);
		free_message(data_aggregate_msg);
		free(encoded_msg);
		data_aggregate_msg = msg;
		ctimer_reset(&aggregate_ctimer);
	} else {
		// Add to aggregated message payload
		struct msg_data_payload *current = data_aggregate_msg->payload;
		while (current->next != NULL) {
			current = current->next;
		}
		current->next = msg->payload;
		data_aggregate_msg->header->length += msg->header->length;
		msg->payload = NULL; // Avoid conflits when freeing the message
		free_message(msg);
	}
}

/*-----------------------------------------------------------------------------*/
/* Callback function when a broadcast message is received */
static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from) {
	// Decode the message
	char *encoded_msg = packetbuf_dataptr();
	struct message *decoded_msg;
	decode_message(&decoded_msg, encoded_msg, packetbuf_datalen());
	
	switch (decoded_msg->header->msg_type) {
		case TREE_INFORMATION_REQUEST:;
			struct msg_tree_request_payload *payload_info_req = (struct msg_tree_request_payload *) decoded_msg->payload;
			// If the tree needs rebuilding
			if ((payload_info_req->request_attributes & 0x1) == 0x1) {
				if (payload_info_req->tree_version > tree_version || (tree_stable && payload_info_req->tree_version == tree_version)) {
					tree_stable = 0;
					// Broadcast TREE_INFORMATION_REQUEST message to destroy the tree
					packetbuf_copyfrom(encoded_msg, packetbuf_datalen());
					broadcast_send(&broadcast);
				}
			} else {
				if (parent != NULL && payload_info_req->tree_version <= tree_version) {
					// Send TREE_ADVERTISEMENT response
					send_runicast_msg(TREE_ADVERTISEMENT, from);
				}
			}
			break;
		case TREE_ADVERTISEMENT:
			handle_tree_advertisement_msg(decoded_msg, from);
			free_message(decoded_msg);
			break;
		default:
			break;
	}
}

// Set the function to be called when a broadcast message is received
static const struct broadcast_callbacks broadcast_callbacks = {broadcast_recv};

/*-----------------------------------------------------------------------------*/
/* Callback function when a unicast message is received */
static void runicast_recv(struct runicast_conn *c, const rimeaddr_t *from) {
	// Decode the message
	char *encoded_msg = packetbuf_dataptr();
	struct message *decoded_msg;
	decode_message(&decoded_msg, encoded_msg, packetbuf_datalen());

	switch (decoded_msg->header->msg_type) {
		case DESTINATION_ADVERTISEMENT:;
			struct msg_dest_ad_payload *payload_dest_ad = (struct msg_dest_ad_payload *) decoded_msg->payload;
			// Discard if version < local version
			if (payload_dest_ad->tree_version >= tree_version) {
				// Add to list of childs (or update)
				add_node(&childs, from, payload_dest_ad->source_id, 0);
				// Forward message to parent
				packetbuf_copyfrom(encoded_msg, packetbuf_datalen());
				runicast_send(&runicast, &(parent->addr_via), 1);
				free_message(decoded_msg);
			}
			break;
		case SENSOR_DATA:
			handle_sensor_data_msg(decoded_msg);
			break;
		case SENSOR_CONTROL:;
			struct msg_control_payload *payload_ctrl = (struct msg_control_payload *) decoded_msg->payload;
			// Check if message is destined to this sensor
			if (my_id == payload_ctrl->destination_id) {
				// Adapt sensor setting (each control message must contain all settings)
				send_data = payload_ctrl->command & 0x1;
				send_periodically = (payload_ctrl->command & 0x2) >> 1;

			} else {
				struct node* child = get_node(childs, payload_ctrl->destination_id);
				if (child != NULL) {
					// Forward control message to child
					packetbuf_copyfrom(decoded_msg, packetbuf_datalen());
					runicast_send(&runicast, &(child->addr_via), 1);
				}
			}
			free_message(decoded_msg);
			break;
		case TREE_ADVERTISEMENT:
			handle_tree_advertisement_msg(decoded_msg, from);
			free_message(decoded_msg);
		default:
			break;
	}
}

// Set the function to be called when a broadcast message is received
static const struct runicast_callbacks runicast_callbacks = {runicast_recv};

/*
static void exit_handler(struct *broadcast_conn bc, struct *runicast_conn rc) {
	broadcast_close(bc);
	runicast_close(rc);
}
*/

/*-----------------------------------------------------------------------------*/
/* Process */
PROCESS_THREAD(my_process, ev, data)
{
	static struct etimer et;

	PROCESS_EXITHANDLER(broadcast_close(&broadcast);) 

	PROCESS_BEGIN();

	runicast_open(&runicast, 146, &runicast_callbacks);

	while (1) {
		// Every 25 to 35 seconds
		etimer_set(&et, CLOCK_SECOND * 25 + random_rand() % (CLOCK_SECOND * 10));

    	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		remove_expired_nodes(&parent, 90);
		if (parent == NULL) {
			tree_stable = 0;
			// Broadcast a TREE_INFORMATION_REQUEST
			send_broadcast_msg(TREE_INFORMATION_REQUEST);
		} else {
			// Advertise the tree (broadcast a TREE_ADVERTISEMENT)
			send_broadcast_msg(TREE_ADVERTISEMENT);
		}

		// Remove childs that have not sent any message since a long time (more than 240 seconds)
		remove_expired_nodes(&childs, 240);
	}

	PROCESS_END();
}

PROCESS_THREAD(sensor_process, ev, data)
{
	static struct etimer et;
	uint8_t iter = 0;

	PROCESS_EXITHANDLER(runicast_close(&runicast);)

	PROCESS_BEGIN();

	broadcast_open(&broadcast, 129, &broadcast_callbacks);

	while (1) {
		iter += 1;
		// Every 20 to 40 seconds
		etimer_set(&et, CLOCK_SECOND * 20 + random_rand() % (CLOCK_SECOND * 20));

    	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		// TODO Generate data, create message
		struct message *msg = (struct message *) malloc(sizeof(struct message));	
		get_msg(msg, SENSOR_DATA);
		struct msg_data_payload *payload = (struct msg_data_payload *) malloc(sizeof(struct msg_data_payload));
		payload->data_header =  (struct msg_data_payload_h *) malloc(sizeof(struct msg_data_payload_h));
		payload->data_header->source_id = my_id;
		payload->data_header->subject_id = 42;
		payload->data_header->length = sizeof(int);
		int *data = (int *) malloc(sizeof(int));
		*data = 69; // Sensor value
		payload->data = data;
		msg->header->length = sizeof(struct msg_data_payload_h) + sizeof(int);
		msg->payload = payload;

		// Send data
		handle_sensor_data_msg(msg);

		// Send DESTINATION_ADVERTISEMENT to indicate that this node is still up (every 120 seconds)
		if (iter % 4 == 0) {
			send_runicast_msg(TREE_ADVERTISEMENT, &(parent->addr_via));
		}
	}

	PROCESS_END();
}
