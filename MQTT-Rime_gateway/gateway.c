#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include <time.h>
#include <string.h>

#include "message.h"

#include "net/rime/rime.h"

uint8_t my_id = 0; // TODO Modify this
uint8_t tree_version = 0; 

// List of subjects, their subscribers and the sensors corresponding to this subject
// TODO multiple sensors per subject ? or unique sensor per subject ?
struct subject {
	struct node *sensors = NULL;
	struct xxx *subscribers = NULL; // TODO
	char *subject_name = NULL;
	uint8_t subject_id; 
	struct subject *next = NULL;
}

struct subject *subjects = NULL;

/*-----------------------------------------------------------------------------*/
/* Callback function when a broadcast message is received */
static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from) {
	// Decode the message
	char *encoded_msg = packetbuf_dataptr();
	struct message *decoded_msg;
	decode_message(decoded_msg, encoded_msg, packetbuf_datalen());
	
	switch (decoded_msg.header.msg_type) {
		case TREE_INFORMATION_REQUEST:
			// If the tree needs rebuilding, increment the tree version 
			if (decoded_msg.payload.request_attributes & 0x1 == 0x1 && decoded_msg.payload.tree_version == tree_version) {
				tree_version++;
			}
				
			// Send TREE_ADVERTISEMENT response
			struct message msg;
			msg.payload = (struct msg_tree_ad_payload *) malloc(sizeof(struct msg_tree_ad_payload));
			msg.payload.n_hops = 0;
			msg.payload.source_id = my_id;
			msg.payload.tree_version = tree_version;
			msg.header.length = sizeof(struct msg_tree_ad_payload);
			char *encoded_msg;
			uint32_t len = encode_message(&msg, encoded_msg);
			packetbuf_copyfrom(encoded_msg, len);	// Put data inside the packet
			runicast_send(&runicast, from, 1);	
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
			// TODO Add this sensor to the list of sensors + add its subject to the list of subjects
			struct msg_data_payload *current_subject = subjects;
			while (current_subject != NULL) {
				if (current_subject.subject_id == current_payload.data_header.subject_id) {
					// Check if sensor already in the list of sensors -> update timestamp
				}
				current_subject = current_subject.next;
			}
			if (current_subject == NULL) {
				// If no subject corresponds to this data, add it to the list + send SENSOR_CONTROL
			}

			break;
		case SENSOR_DATA:
			// TODO Send data to the receivers. If no receiver for this data, send a SENSOR_CONTROL message to the sensor(s) sending this data to disable it
			struct msg_data_payload *current_payload = decoded_msg.payload;
			// Go through each data in the message
			while(current_payload != NULL) {
				// Check if there is a receiver for this data
				struct msg_data_payload *current_subject = subjects;
				while (current_subject != NULL) {
					if (current_subject.subject_id == current_payload.data_header.subject_id) {
						if (current_subject.subscribers == NULL) {
							// Send SENSOR_CONTROL message to the sensors to stop sending data
						} else {
							// Send data to subscribers
						}
					}
					current_subject = current_subject.next;
				}
				if (current_subject == NULL) {
					// If no subject corresponds to this data, add it to the list + send SENSOR_CONTROL
				}
				current_payload = current_payload.next;
			}
			break;
		default:
	}
	// TODO free data
}

// Set the function to be called when a broadcast message is received
static const struct runicast_callbacks rc = {runicast_recv}
