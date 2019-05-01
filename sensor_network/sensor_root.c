#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include <time.h>
#include <string.h>
#include <stdio.h>

#include "message.h"

#include "net/rime.h"

/*-----------------------------------------------------------------------------*/
/* Configuration values */
const uint16_t runicast_channel = 142;
const uint16_t broadcast_channel = 169;
const uint8_t version = 1;

struct runicast_conn runicast;
struct broadcast_conn broadcast;

/*-----------------------------------------------------------------------------*/
uint8_t my_id = 0;
uint8_t tree_version = 0; 

/*-----------------------------------------------------------------------------*/
/* Declaration of the processes */
PROCESS(my_process, "My process");
PROCESS(gateway_process, "Gateway process");
AUTOSTART_PROCESSES(&my_process, &gateway_process);

// TODO Handle childs
// TODO Prevent concurrent access issues

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
			struct msg_tree_request_payload *payload_info_req = (struct msg_tree_request_payload *) decoded_msg->payload;
			if ((payload_info_req->request_attributes & 0x1) == 0x1 && payload_info_req->tree_version == tree_version) {
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
			runicast_send(&runicast, from, 1);	
			break;
		default:
			break;
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
	decode_message(&decoded_msg, encoded_msg, packetbuf_datalen());

	switch (decoded_msg->header->msg_type) {
		case DESTINATION_ADVERTISEMENT:;
			struct msg_dest_ad_payload *payload_dest_ad = (struct msg_dest_ad_payload *) decoded_msg->payload;
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

	broadcast_open(&broadcast, 129, &broadcast_callbacks);

	while (1) {
		// Every 25 to 35 seconds
		etimer_set(&et, CLOCK_SECOND * 25 + random_rand() % (CLOCK_SECOND * 10));

    	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		// Advertise the tree (broadcast a TREE_ADVERTISEMENT)
		send_broadcast_msg(TREE_ADVERTISEMENT);

		// Remove childs that have not sent any message since a long time (more than 240 seconds)
		remove_expired_nodes(&childs, 240);
	}

	PROCESS_END();
}

PROCESS_THREAD(gateway_process, ev, data)
{
	static struct etimer et;

	PROCESS_EXITHANDLER(runicast_close(&runicast);)

	PROCESS_BEGIN();

	runicast_open(&runicast, 146, &runicast_callbacks);

	while (1) {
		// Every second
		etimer_set(&et, CLOCK_SECOND);

    	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		
		// Read stdin
		int cmd;
		int val;
		int dst;
		scanf("%d %d %d", &cmd, &val, &dst);

		// Initialize correct control message
		struct message *msg = (struct message *) malloc(sizeof(struct message));
		msg->header = (struct msg_header *) malloc(sizeof(struct msg_header));
		msg->header->version = version;
		msg->header->msg_type = SENSOR_CONTROL;
		msg->payload = (struct msg_control_payload *) malloc(sizeof(struct msg_control_payload)); 
		switch (cmd) {
			case 0:
				// Send data periodically / Send data on change
				if (val == 1) {
					// Send data periodically
					msg->payload->command = 0x11;
				} else if (val == 0) {
					// Send data on change
					msg->payload->command = 0x10;
				}
				break;
			case 1:
				// Send data / Don't send data
				if (val == 1) {
					// Send data
					msg->payload->command = 0x21;
				} else if (val == 0) {
					// Don't send data
					msg->payload->command = 0x20;
				}
				break;
			default:
				break;
		}
		char *encoded_msg;
		uint32_t len = encode_message(msg, &encoded_msg);
		packetbuf_copyfrom(encoded_msg, len);
		if (dst < 0) {
			// TODO Send to all childs

		} else {
			// TODO Send to the child with id = dst
			msg->payload->destination_id = dst;
			//runicast_send(&runicast, &(child->addr_via), 1);
		}
		free_message(msg);
	}

	PROCESS_END();
}