/**
 * General event system for turtle.
 *
 * Objects/whatever can register for an event notification.  When they happen they will be posted
 * synchonously to the system.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Abstract crier class
struct trtl_crier;

typedef uint32_t trtl_crier_cry_t;
#define TRTL_CRY_INVALID 0

struct trtl_crier *trtl_crier_init(void);

typedef void (*trtl_crier_listen_callback)(void *user_data, trtl_crier_cry_t cry,
					   const void *event_data);

/**
 * Register a new cry.
 *
 * Returns the new cry ID or TRTL_CRY_INVALID.  If the cry already exists the same id is returned.
 *
 * The cry ID is used to post the cry event.
 *
 * @param crier Crier class
 * @param name Name of the event to register.
 */
trtl_crier_cry_t trtl_crier_new_cry(struct trtl_crier *crier, const char *name);

/**
 * Post a cry event to all registered recipients.
 *
 * @param crier Crier data structure.
 * @param cry The cry to post.
 * @param data Cry data to post with the event.
 * @return true on success, false on an error.
 */
bool trtl_crier_post(struct trtl_crier *crier, trtl_crier_cry_t cry, const void *data);

/**
 * Listen for a particular cry by name.
 *
 * Creates the cry if it doesn't exist.
 */
int trtl_crier_listen(struct trtl_crier *crier, const char *name, trtl_crier_listen_callback, void *data);
