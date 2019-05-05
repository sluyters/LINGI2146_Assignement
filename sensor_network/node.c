#include "node.h"

void clock_library_init(void) {
	clock_init();
}

/**
 * Adds the new node to the @nodes list, or update its data if it is already present
 */
void add_node(struct node **nodes, const rimeaddr_t *addr_via, uint8_t node_id, uint8_t n_hops) {
	if (*nodes == NULL) {
		// If the list is empty, create a new node
		*nodes = (struct node *) malloc(sizeof(struct node));
		rimeaddr_copy(&((*nodes)->addr_via), addr_via);
		(*nodes)->node_id = node_id;
		(*nodes)->next = NULL;
		(*nodes)->n_hops = n_hops;
		(*nodes)->timestamp = (unsigned long) clock_seconds();
	} else if ((*nodes)->node_id == node_id && (*nodes)->next == NULL) {
		// If the first node matches node_id and there is no other node, update it
		rimeaddr_copy(&((*nodes)->addr_via), addr_via);
		(*nodes)->n_hops = n_hops;
		(*nodes)->timestamp = (unsigned long) clock_seconds();
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
		rimeaddr_copy(&(new_node->addr_via), addr_via);
		new_node->node_id = node_id;
		new_node->next = NULL;
		new_node->n_hops = n_hops;
		new_node->timestamp = (unsigned long) clock_seconds();
		previous->next = new_node;
	}
}

/**
 * Returns the node corresponding to @node_id from @nodes
 */
void remove_node(struct node **nodes, uint8_t node_id) {
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
void remove_expired_nodes(struct node **nodes, int max_elapsed_secs) {
	int now = (unsigned long) clock_seconds();
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
struct node *get_node(struct node *nodes, uint8_t node_id) {
	struct node *current = nodes;
	while (current != NULL) {
		if (current->node_id == node_id) {
			return current;
		}
		current = current->next;
	}
	return NULL;
}
