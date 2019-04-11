/*
 * GOOD LINK SYMPA https://github.com/contiki-os/contiki/blob/master/examples/rime/example-neighbors.c
 */

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include <string.h>

#define DEBUG DEBUG_PRINT

#include "net/rime/rime.h"	// Added

/* Information about a neighboring node */
struct neighbor_node {
	struct neighbor_node *next;
	linkaddr_t addr;
}


