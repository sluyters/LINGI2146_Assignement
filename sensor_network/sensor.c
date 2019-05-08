/*
 * GOOD LINK SYMPA https://github.com/contiki-os/contiki/blob/master/examples/rime/example-neighbors.c
 */

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include "node-id.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "message.h"
#include "node.h"

//#define DEBUG DEBUG_PRINT

#include "net/rime.h"	

/*-----------------------------------------------------------------------------*/
/* Configuration values */
const uint16_t runicast_channel = 142;
const uint16_t broadcast_channel = 169;
const uint8_t version = 1;
const int n_retransmissions = 1;

struct runicast_conn runicast;
struct broadcast_conn broadcast;

/*-----------------------------------------------------------------------------*/
/* Sensor settings */
int send_data = 1; // By default, don't send data
int send_periodically = 1; // By default, send data only when there is a change (not periodically)

/*-----------------------------------------------------------------------------*/
/* Save parent + child nodes */
struct node *parent = NULL;
struct node *childs = NULL;

/*-----------------------------------------------------------------------------*/
uint8_t my_id = 29; // TODO Modify this
uint8_t my_subject_id = 0;
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
	runicast_send(&runicast, &(parent->addr_via), n_retransmissions);
	free_message(data_aggregate_msg);
	free(encoded_msg);
	data_aggregate_msg = NULL;
}

// TODO problem with runicast transmitting while trying to send other things -> create a list of messages to send, and send them from one process ?

// TODO fix bug where the correct data is not correctly sent / aggregated / transmitted and results in corrupted data at the root node

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
			payload_dest_ad->subject_id = my_subject_id;
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
 * Sends a simple runicast message of type @msg_type to @addr_dest
 */ 
static void send_runicast_msg(int msg_type, const rimeaddr_t *addr_dest) {
	struct message *msg = (struct message *) malloc(sizeof(struct message));
	char *encoded_msg;	
	get_msg(msg, msg_type);
	uint32_t len = encode_message(msg, &encoded_msg);
	int ret = 0;
	uint8_t iter = 0;
	packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet
	runicast_send(&runicast, addr_dest, n_retransmissions);
	free(encoded_msg);
	free_message(msg);
}


static void handle_tree_advertisement_msg(struct message *msg, const rimeaddr_t *from) {
	// Check version, if version >= local version, process the TREE_ADVERTISEMENT message, don't accept TREE_ADVERTISEMENT with same tree_version if the tree is not stable
	struct msg_tree_ad_payload *payload = (struct msg_tree_ad_payload *) msg->payload;
	printf("Received TREE_AD TreeV:%d Src:%d Nhops:%d\n", payload->tree_version, payload->source_id, payload->n_hops);
	if ((payload->tree_version > tree_version) || ((tree_version - payload->tree_version) > 245) || ((payload->tree_version == tree_version) && tree_stable)) {
		// Check if new neighbor is better than current parent (automatically better if tree version is greater)
		if ((payload->tree_version > tree_version || tree_version - payload->tree_version > 245) || ((parent == NULL || (payload->n_hops + 1) < parent->n_hops) && get_node(childs, payload->source_id) == NULL)) {
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
			// Don't send TREE_ADVERTISEMENT if no relevant information update
			if ((tree_version != payload->tree_version) || (payload->n_hops + 1 != parent->n_hops)) {
				// Broadcast the new tree
				send_broadcast_msg(TREE_ADVERTISEMENT);
				// Update tree version + consider the tree as stable
				tree_version = payload->tree_version;
			}
			// Update the informations of the parent
			add_node(&parent, from, payload->source_id, payload->n_hops + 1);
			// Consider the tree as stable
			tree_stable = 1;
		}
	}
}


static void handle_sensor_data_msg(struct message *msg) {
	if (childs == NULL) {
		// TODO why no child ?
		printf("Transfer data - No childs\n");
		// If no child, send data directly (no need to aggregate)
		char *encoded_msg;
		uint32_t len = encode_message(msg, &encoded_msg);
		packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet
		runicast_send(&runicast, &(parent->addr_via), n_retransmissions);
		free(encoded_msg);
	} else if (data_aggregate_msg == NULL) {
		printf("Transfer data - Aggregatefirst\n");
		// Store this message while waiting for other messages to aggregate + set timer
		data_aggregate_msg = copy_message(msg);
		ctimer_set(&aggregate_ctimer, CLOCK_SECOND * 30, send_aggregate_msg, NULL);
	} else if (data_aggregate_msg->header->length + msg->header->length > 128) {
		printf("Transfer data - Aggregatetoomany\n");
		// If too many messages already aggregated, send old messages + save this one as new aggregated message
		char *encoded_msg;
		uint32_t len = encode_message(data_aggregate_msg, &encoded_msg);
		packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet
		runicast_send(&runicast, &(parent->addr_via), n_retransmissions);
		free_message(data_aggregate_msg);
		free(encoded_msg);
		data_aggregate_msg = copy_message(msg);
		ctimer_reset(&aggregate_ctimer);
	} else {
		printf("Transfer data - Aggregateadd\n");
		// Add to aggregated message payload
		struct msg_data_payload *current = data_aggregate_msg->payload;
		while (current->next != NULL) {
			current = current->next;
		}
		current->next = msg->payload;
		data_aggregate_msg->header->length += msg->header->length;
		msg->payload = NULL; // Avoid conflits when freeing the message
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
			printf("Received TREE_INFO_REQ (broadcast)\n");
			struct msg_tree_request_payload *payload_info_req = (struct msg_tree_request_payload *) decoded_msg->payload;
			// If the tree needs rebuilding
			if (((payload_info_req->request_attributes & 0x1) == 0x1) && ((payload_info_req->tree_version > tree_version) || (tree_stable && payload_info_req->tree_version == tree_version))) {
				tree_stable = 0;
				// Broadcast TREE_INFORMATION_REQUEST message to destroy the tree
				packetbuf_copyfrom(encoded_msg, packetbuf_datalen());
				broadcast_send(&broadcast);
			} else if ((parent != NULL) && (payload_info_req->tree_version <= tree_version)) {
				// Send TREE_ADVERTISEMENT response
				send_runicast_msg(TREE_ADVERTISEMENT, from);
			}
			break;
		case TREE_ADVERTISEMENT:
			handle_tree_advertisement_msg(decoded_msg, from);
			break;
		default:
			break;
	}
	free_message(decoded_msg);
}

// Set the function to be called when a broadcast message is received
static const struct broadcast_callbacks broadcast_callbacks = {broadcast_recv};

/*-----------------------------------------------------------------------------*/
/* Callback function when a runicast message is received */
static void runicast_recv(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno) {
	// Decode the message
	char *encoded_msg = packetbuf_dataptr();
	struct message *decoded_msg;
	decode_message(&decoded_msg, encoded_msg, packetbuf_datalen());

	switch (decoded_msg->header->msg_type) {
		case DESTINATION_ADVERTISEMENT:;
			struct msg_dest_ad_payload *payload_dest_ad = (struct msg_dest_ad_payload *) decoded_msg->payload;
			printf("Received DEST_AD TreeV:%d Src:%d Subject:%d\n", payload_dest_ad->tree_version, payload_dest_ad->source_id, payload_dest_ad->subject_id);
			// Discard if version < local version
			if (payload_dest_ad->tree_version >= tree_version) {
				// Add to list of childs (or update)
				add_node(&childs, from, payload_dest_ad->source_id, 0);
				// Forward message to parent
				packetbuf_copyfrom(encoded_msg, packetbuf_datalen());
				int ret = runicast_send(&runicast, &(parent->addr_via), n_retransmissions);
				printf("Received DEST_AD (forwarded) TreeV:%d Src:%d Ret:%d\n", payload_dest_ad->tree_version, payload_dest_ad->source_id, ret);
			}
			break;
		case SENSOR_DATA:
			printf("Received SENSOR_DATA (runicast)\n");
			handle_sensor_data_msg(decoded_msg);
			break;
		case SENSOR_CONTROL:;
			printf("Received SENSOR_CONTROL (runicast)\n");
			struct msg_control_payload *payload_ctrl = (struct msg_control_payload *) decoded_msg->payload;
			// Check if message is destined to this sensor
			if (my_id == payload_ctrl->destination_id) {
				// Adapt sensor setting
				if ((payload_ctrl->command & ~0x1) == 0x10) {
					send_periodically = payload_ctrl->command & 0x1;
				} else if ((payload_ctrl->command & ~0x1) == 0x20) {
					send_data = payload_ctrl->command & 0x1;
				}
			} else {
				struct node* child = get_node(childs, payload_ctrl->destination_id);
				if (child != NULL) {
					// Forward control message to child
					packetbuf_copyfrom(encoded_msg, packetbuf_datalen());
					runicast_send(&runicast, &(child->addr_via), n_retransmissions);
				}
			}
			break;
		case TREE_ADVERTISEMENT:
			printf("Received TREE_AD (runicast)\n");
			handle_tree_advertisement_msg(decoded_msg, from);
		default:
			break;
	}
	free_message(decoded_msg);
}

// Set the function to be called when a broadcast message is received
static const struct runicast_callbacks runicast_callbacks = {runicast_recv};

/*-----------------------------------------------------------------------------*/
/* Process */
PROCESS_THREAD(my_process, ev, data)
{
	static struct etimer et;

	PROCESS_EXITHANDLER(broadcast_close(&broadcast);) 

	PROCESS_BEGIN();
	
	clock_library_init();
	random_init(node_id);
	
	my_id = node_id;
	
	broadcast_open(&broadcast, 129, &broadcast_callbacks);

	// Every 25 to 35 seconds
	//etimer_set(&et, CLOCK_SECOND * 25 + random_rand() % (CLOCK_SECOND * 10));
	etimer_set(&et, CLOCK_SECOND * 200 + random_rand() % (CLOCK_SECOND * 150));
	while (1) {
    	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		//remove_expired_nodes(&parent, 90);
		remove_expired_nodes(&parent, 45); // TODO Conflict risk
		if (parent == NULL) {
			tree_stable = 0;
			// Broadcast a TREE_INFORMATION_REQUEST
			send_broadcast_msg(TREE_INFORMATION_REQUEST);
		} else {
			// Advertise the tree (broadcast a TREE_ADVERTISEMENT)
			send_broadcast_msg(TREE_ADVERTISEMENT);
		}

		// Remove childs that have not sent any message since a long time (more than 240 seconds)
		//remove_expired_nodes(&childs, 240);
		remove_expired_nodes(&childs, 90);	// TODO Conflict risk
		
		etimer_reset(&et);
	}

	PROCESS_END();
}
uint8_t iter = 0;
int last_data = -1;
int data;
struct message *msg;
struct msg_data_payload *payload ;
PROCESS_THREAD(sensor_process, ev, data)
{
	static struct etimer et;

	PROCESS_EXITHANDLER(runicast_close(&runicast);)

	PROCESS_BEGIN();

	runicast_open(&runicast, 146, &runicast_callbacks);
	
	// Every 20 to 40 seconds
	//etimer_set(&et, CLOCK_SECOND * 20 + random_rand() % (CLOCK_SECOND * 20));
	etimer_set(&et, CLOCK_SECOND * 300 + random_rand() % (CLOCK_SECOND * 200));
	while (1) {
		iter += 1;

    	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		if (send_data && (parent != NULL)) {
			printf("Sending data\n");
			// TODO generate random sensor data
			data = 69; // Sensor value

			// Send data
			if (send_periodically || (data != last_data)) {
				msg = (struct message *) malloc(sizeof(struct message));	
				get_msg(msg, SENSOR_DATA);
				payload = (struct msg_data_payload *) malloc(sizeof(struct msg_data_payload));
				payload->data_header =  (struct msg_data_payload_h *) malloc(sizeof(struct msg_data_payload_h));
				payload->data_header->source_id = my_id;
				payload->data_header->subject_id = my_subject_id;
				payload->data_header->length = sizeof(int);
				payload->data = malloc(sizeof(int));
				memcpy(payload->data, &data, sizeof(int));
				payload->next = NULL;
				msg->header->length = sizeof(struct msg_data_payload_h) + sizeof(int);
				msg->payload = payload;

				handle_sensor_data_msg(msg);	// TODO Conflict risk
				free_message(msg);
			}
			last_data = data;
		}

		// Send DESTINATION_ADVERTISEMENT to indicate that this node is still up (every 120 seconds)
		if ((iter % 2 == 0) && (parent != NULL)) {
			send_runicast_msg(DESTINATION_ADVERTISEMENT, &(parent->addr_via));
		}
		etimer_reset(&et);
	}

	PROCESS_END();
}
