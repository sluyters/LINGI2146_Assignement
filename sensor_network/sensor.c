/*
 * GOOD LINK SYMPA https://github.com/contiki-os/contiki/blob/master/examples/rime/example-neighbors.c
 */

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include <string.h>

//#define DEBUG DEBUG_PRINT

#include "net/rime/rime.h"	

/*-----------------------------------------------------------------------------*/
/* Information about a neighboring node */
struct neighbor_node {
	struct neighbor_node *next;
	linkaddr_t addr;
	// TODO 
}

/*-----------------------------------------------------------------------------*/
/* Declaration of the processes */
// TODO
PROCESS(broadcast_process, "Broadcast process");
AUTOSTART_PROCESSES(&broadcast_process);

/*-----------------------------------------------------------------------------*/
/* Callback function when a broadcast message is received */
static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from) {
	// TODO
}

// Set the function to be called when a broadcast message is received
static const struct broadcast_callbacks bc = {broadcast_recv};

/*-----------------------------------------------------------------------------*/
/* Callback function when a unicast message is received */
static void runicast_recv(struct runicast_conn *c, const rimeaddr_t *from) {
	// TODO
}

// Set the function to be called when a broadcast message is received
static const struct runicast_callbacks rc = {runicast_recv}

/*-----------------------------------------------------------------------------*/
/* Process */
// Broadcast process
PROCESS_THREAD(broadcast_process, ev, data)
{
	// TODO
}
