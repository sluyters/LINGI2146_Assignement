/*
 * This file represents messages and their operations
 * Authors: 
 * BOSCH Sami 		- 26821500
 * SIMON Benjamin 	- 37151500
 * SLUYTERS Arthur	- 13151500
 */

#include "message.h"

uint32_t encode_message(struct message *decoded_msg, char **encoded_msg) {
	uint32_t length = decoded_msg->header->length + sizeof(struct msg_header);
	// Allocate memory for encoded message
	*encoded_msg = (char *) malloc(length); // TODO make allocation outside of the function ?
	int offset = 0;
	// Encode the header
	memcpy(*encoded_msg, (void *) decoded_msg->header, sizeof(struct msg_header));
	offset += sizeof(struct msg_header);
	// Encode the payload
	switch (decoded_msg->header->msg_type) {
		case SENSOR_DATA:;
			// Copy all payload data
			struct msg_data_payload *current = (struct msg_data_payload *) decoded_msg->payload;
			while (current != NULL) {
				memcpy(*encoded_msg + offset, (void *) current->data_header, sizeof(struct msg_data_payload_h));
				offset += sizeof(struct msg_data_payload_h);
				memcpy(*encoded_msg + offset, (void *) current->data, current->data_header->length);
				offset += current->data_header->length;
				current = current->next;
			}
			break;
		default:
			memcpy(*encoded_msg + offset, (void *) decoded_msg->payload, decoded_msg->header->length);	
	}
	return length;
}


void decode_message(struct message **decoded_msg, char *encoded_msg, uint16_t msg_len) {
	int offset = 0;
	// Allocate memory for decoded message
	struct message *new_msg = (struct message *) malloc(sizeof(struct message));	// TODO make allocation outside of the function
	new_msg->header = (struct msg_header *) malloc(sizeof(struct msg_header));
	// Decode the header
	memcpy(new_msg->header, (void *) encoded_msg, sizeof(struct msg_header));
	offset += sizeof(struct msg_header);
	new_msg->header->length = msg_len - offset;
	// Decode the payload
	switch (new_msg->header->msg_type) {
		case DESTINATION_ADVERTISEMENT:;
			struct msg_dest_ad_payload *payload_dest_ad = (struct msg_dest_ad_payload *) malloc(new_msg->header->length);
			memcpy(payload_dest_ad, (void *) encoded_msg + offset, new_msg->header->length);
			new_msg->payload = payload_dest_ad;
			break; 
		case TREE_ADVERTISEMENT:;
			struct msg_tree_ad_payload *payload_tree_ad = (struct msg_tree_ad_payload *) malloc(new_msg->header->length);
			memcpy(payload_tree_ad, (void *) encoded_msg + offset, new_msg->header->length);
			new_msg->payload = payload_tree_ad;
			break; 
		case TREE_INFORMATION_REQUEST:;
			struct msg_tree_request_payload *payload_info_req = (struct msg_tree_request_payload *) malloc(new_msg->header->length);
			memcpy(payload_info_req, (void *) encoded_msg + offset, new_msg->header->length);
			new_msg->payload = payload_info_req;
			break;
		case SENSOR_DATA:;
			struct msg_data_payload *payload_data = (struct msg_data_payload *) malloc(sizeof(struct msg_data_payload));
			// Copy data header
			payload_data->data_header = (struct msg_data_payload_h *) malloc(sizeof(struct msg_data_payload_h));
			memcpy(payload_data->data_header, (void *) encoded_msg + offset, sizeof(struct msg_data_payload_h));
			offset += sizeof(struct msg_data_payload_h);
			// Copy data payload
			payload_data->data = (void *) malloc(payload_data->data_header->length);
			memcpy(payload_data->data, (void *) encoded_msg + offset, payload_data->data_header->length);
			offset += payload_data->data_header->length;
			new_msg->payload = payload_data;

			while (offset < msg_len) {
				// Set next data payload
				payload_data->next = (struct msg_data_payload *) malloc(sizeof(struct msg_data_payload));
				payload_data = payload_data->next;
				// Copy data header
				payload_data->data_header = (struct msg_data_payload_h *) malloc(sizeof(struct msg_data_payload_h));
				memcpy(payload_data->data_header, (void *) encoded_msg + offset, sizeof(struct msg_data_payload_h));
				offset += sizeof(struct msg_data_payload_h);
				// Copy data payload
				payload_data->data = (void *) malloc(payload_data->data_header->length);
				memcpy(payload_data->data, (void *) encoded_msg + offset, payload_data->data_header->length);
				offset += payload_data->data_header->length;
			}
			payload_data->next = NULL;
			break;
		case SENSOR_CONTROL:;
			struct msg_control_payload *payload_ctrl = (struct msg_control_payload *) malloc(new_msg->header->length);
			memcpy(payload_ctrl, (void *) encoded_msg + offset, sizeof(new_msg->header->length));
			new_msg->payload = payload_ctrl;
			break;
		default:	
			break;
	}
    *decoded_msg = new_msg;
}

/**
 * Copies the message @msg to a new message
 */
struct message *copy_message(struct message *msg) {
	struct message *msg_copy = (struct message *) malloc(sizeof(struct message));
	// Copy message header
	msg_copy->header = (struct msg_header *) malloc(sizeof(struct msg_header));
	memcpy((void *) msg_copy->header, (void *) msg->header, sizeof(struct msg_header));
	// Copy message payload
	switch (msg->header->msg_type)
	{
		case SENSOR_DATA:;
			// Copy all payload data
			struct msg_data_payload *current = (struct msg_data_payload *) msg->payload;
			struct msg_data_payload *current_copy;
			int iter = 0;
			while (current != NULL) {
				if (iter == 0) {
					current_copy = (struct msg_data_payload *) malloc(sizeof(struct msg_data_payload));
					msg_copy->payload = current_copy;
				} else {
					current_copy->next = (struct msg_data_payload *) malloc(sizeof(struct msg_data_payload));
					current_copy = current_copy->next;
				}
				// Copy data header
				current_copy->data_header = (struct msg_data_payload_h *) malloc(sizeof(struct msg_data_payload_h));
				memcpy((void *) current_copy->data_header, (void *) current->data_header, sizeof(struct msg_data_payload_h));
				// Copy data
				current_copy->data = (void *) malloc(current->data_header->length);
				memcpy((void *) current_copy->data, (void *) current->data, current->data_header->length);
				// Go to next payload 
				current = current->next;
				iter++;
			}
			current_copy->next = NULL;
			break;
	
		default:;
			msg_copy->payload = (struct msg_header *) malloc(msg->header->length);
			memcpy((void *) msg_copy->payload, (void *) msg->payload, msg->header->length);
			break;
	}
	return msg_copy;
}


/**
 * Frees the memory used by @msg
 */
void free_message(struct message *msg) {
	if (msg->payload != NULL) {
		if (msg->header->msg_type == SENSOR_DATA) {
			// Free all aggregated data
			struct msg_data_payload *current = msg->payload;
			struct msg_data_payload *previous;
			while (current != NULL) {
				free(current->data);
				free(current->data_header);
				previous = current;
				current = current->next;
				free(previous);
			}
		} else {
			free(msg->payload);
		}
	}
	free(msg->header);
	free(msg);
	msg = NULL;
}
