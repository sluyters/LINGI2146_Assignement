#include "message.h"

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