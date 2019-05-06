#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "dev/serial-line.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "message.h"
#include "node.h"

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
/* Save child nodes */
struct node *childs = NULL;

/*-----------------------------------------------------------------------------*/
uint8_t my_id = 0;
uint8_t tree_version = 0; 

/*-----------------------------------------------------------------------------*/
/* Declaration of the processes */
PROCESS(my_process, "My process");
PROCESS(gateway_process, "Gateway process");
AUTOSTART_PROCESSES(&my_process, &gateway_process);

// TODO Prevent concurrent access issues
// TODO Fix memory leaks

/*-----------------------------------------------------------------------------*/
/* Helper funcions */

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
			payload_tree_ad->n_hops = 0;
			payload_tree_ad->source_id = my_id;
			payload_tree_ad->tree_version = tree_version;
			msg->header->length = sizeof(struct msg_tree_ad_payload);
			msg->payload = payload_tree_ad;
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

/*-----------------------------------------------------------------------------*/
/* Callback function when a broadcast message is received */
static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from) {
	// Decode the message
	char *encoded_msg = packetbuf_dataptr();
	struct message *decoded_msg;
	decode_message(&decoded_msg, encoded_msg, packetbuf_datalen());
	
	switch (decoded_msg->header->msg_type) {
		case TREE_INFORMATION_REQUEST:;
			// If the tree needs rebuilding, increment the tree version 
			printf("Received TREE_INFO_REQ\n");
			struct msg_tree_request_payload *payload_info_req = (struct msg_tree_request_payload *) decoded_msg->payload;
			if (((payload_info_req->request_attributes & 0x1) == 0x1) && (payload_info_req->tree_version == tree_version)) {
				printf("Received TREE_INFO_REQ (version increase)\n");
				tree_version++;
			}
				
			// Send TREE_ADVERTISEMENT response
			struct message msg;
			struct msg_tree_ad_payload *payload = (struct msg_tree_ad_payload *) malloc(sizeof(struct msg_tree_ad_payload));
			payload->n_hops = 0;
			payload->source_id = my_id;
			payload->tree_version = tree_version;
			msg.header->length = sizeof(struct msg_tree_ad_payload);
			msg.payload = payload;
			char *encoded_msg;
			uint32_t len = encode_message(&msg, &encoded_msg);
			packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet
			runicast_send(&runicast, from, n_retransmissions);	
			break;
		default:
			break;
	}
}

// Set the function to be called when a broadcast message is received
static const struct broadcast_callbacks bc = {broadcast_recv};

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
			// Add to list of childs (or update)
			add_node(&childs, from, payload_dest_ad->source_id, 0);
			// Send the info to the gateway
			printf("ADVERTISE %d %d\n", payload_dest_ad->source_id, payload_dest_ad->subject_id);
			break;
		case SENSOR_DATA:;
			// Send data to the gateway.
			struct msg_data_payload *current_payload = (struct msg_data_payload *) decoded_msg->payload;
			// Go through each data in the message
			while(current_payload != NULL) {
				printf("PUBLISH %d %d %s\n", current_payload->data_header->source_id, current_payload->data_header->subject_id, (char *) current_payload->data);
				current_payload = current_payload->next;
			}
			break;
		default:
			break;
	}

	free_message(decoded_msg);
}

// Set the function to be called when a broadcast message is received
static const struct runicast_callbacks rc = {runicast_recv};

/*-----------------------------------------------------------------------------*/
/* Process */
PROCESS_THREAD(my_process, ev, data)
{
	static struct etimer et;

	PROCESS_EXITHANDLER(broadcast_close(&broadcast);) 

	PROCESS_BEGIN();
	
	clock_library_init();

	broadcast_open(&broadcast, 129, &bc);

	// Every 25 to 35 seconds
	//etimer_set(&et, CLOCK_SECOND * 25 + random_rand() % (CLOCK_SECOND * 10));
	etimer_set(&et, CLOCK_SECOND * 1 + random_rand() % (CLOCK_SECOND * 2));
	while (1) {
    	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		// Advertise the tree (broadcast a TREE_ADVERTISEMENT)
		send_broadcast_msg(TREE_ADVERTISEMENT);

		// Remove childs that have not sent any message since a long time (more than 240 seconds)
		//remove_expired_nodes(&childs, 240);
		remove_expired_nodes(&childs, 25);
		
		etimer_reset(&et);
	}

	PROCESS_END();
}

PROCESS_THREAD(gateway_process, ev, data)
{
	PROCESS_EXITHANDLER(runicast_close(&runicast);)

	PROCESS_BEGIN();

	runicast_open(&runicast, 146, &rc);

	for (;;) {
	
		PROCESS_YIELD();
		
		if(ev == serial_line_event_message) {
			printf("received line: %s\n", (char *)data);
		
			// Read stdin
			int cmd = -1;
			int val = -1;
			int dst = -1;
			int n_data = 0;
			char *input = (char *) data;
			// Decode input
			while (*input && (n_data < 3)) {
				if (isdigit(*input) || ((*input == '-') && isdigit(*(input+1)))) {
					// Number
					if (n_data == 0) {
						cmd = strtol(input, &input, 10);
					} else if (n_data == 1) {
						val = strtol(input, &input, 10);
					} else {
						dst = strtol(input, &input, 10);
					}
					n_data++;
				} else {
					// Move to next char
					input++;
				}
			}

			// Initialize correct control message
			struct message *msg = (struct message *) malloc(sizeof(struct message));
			msg->header = (struct msg_header *) malloc(sizeof(struct msg_header));
			msg->header->version = version;
			msg->header->msg_type = SENSOR_CONTROL;
			struct msg_control_payload *payload = (struct msg_control_payload *) malloc(sizeof(struct msg_control_payload)); 
			switch (cmd) {
				case 0:
					// Send data periodically / Send data on change
					if (val == 1) {
						// Send data periodically
						payload->command = 0x11;
					} else if (val == 0) {
						// Send data on change
						payload->command = 0x10;
					}
					break;
				case 1:
					// Send data / Don't send data
					if (val == 1) {
						// Send data
						payload->command = 0x21;
					} else if (val == 0) {
						// Don't send data
						payload->command = 0x20;
					}
					break;
				default:
					break;
			}
			char *encoded_msg;
			uint32_t len = encode_message(msg, &encoded_msg);
			packetbuf_copyfrom(encoded_msg, len);
			if (dst < 0) {
				// Send to all childs
				struct node *current_child = childs;
				while (current_child != NULL) {
					payload->destination_id = current_child->node_id;
					msg->payload = payload;
					char *encoded_msg;
					uint32_t len = encode_message(msg, &encoded_msg);
					packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet
					runicast_send(&runicast, &(current_child->addr_via), n_retransmissions);
					current_child = current_child->next;
					free(encoded_msg);
				}
			} else {
				// Send to the child with id = dst
				payload->destination_id = dst;
				msg->payload = payload;
				struct node *child = get_node(childs, dst);
				if (child != NULL) {
					char *encoded_msg;
					uint32_t len = encode_message(msg, &encoded_msg);
					packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet
					runicast_send(&runicast, &(child->addr_via), n_retransmissions);
					free(encoded_msg);
				}
			}
			free_message(msg);
		}
	}

	PROCESS_END();
}
