
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include <talloc.h>

#include "helpers.h"
#include "trtl_crier.h"

// Abstract crier class
struct trtl_crier {
	trtl_crier_cry_t highest_cry;
	// This is linked list.  Yes, it's really terrible.  FIXME.  Oh god please.
	struct cry *cries;
};

struct cry {
	struct cry *next;
	trtl_crier_cry_t cry;
	const char *name;

	struct cry_cb *cbs;
};

struct cry_cb {
	struct cry_cb *next;

	// Find a link back to the head of the list.
	trtl_crier_cry_t cryid;

	void (*cb)(void *user_data, trtl_crier_cry_t cry, const void *event_data);
	void *user_data;
};

typedef uint32_t trtl_crier_cry_t;

struct trtl_crier *
trtl_crier_init(void)
{
	return talloc_zero(0, struct trtl_crier);
}

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
trtl_crier_cry_t
trtl_crier_new_cry(struct trtl_crier *crier, const char *name)
{
	struct cry *cry;
	crier = talloc_get_type(crier, struct trtl_crier);
	if (!crier) return TRTL_CRY_INVALID;
	if (name == NULL || *name == 0) return TRTL_CRY_INVALID;

	for (cry = crier->cries; cry; cry = cry->next) {
		if (streq(cry->name, name)) {
			// Existing - just return it
			return cry->cry;
		}
	}

	cry = talloc_zero(crier, struct cry);
	cry->name = talloc_strdup(cry, name);
	cry->cry = ++crier->highest_cry;

	cry->next = crier->cries;
	crier->cries = cry;

	return cry->cry;
}

/**
 * Post a cry event to all registered recipients.
 *
 * @param crier Crier data structure.
 * @param cry The cry to post.
 * @param data Cry data to post with the event.
 * @return true on success, false on an error.
 */
bool
trtl_crier_post(struct trtl_crier *crier, trtl_crier_cry_t cryid, const void *data)
{
	struct cry *cry;
	crier = talloc_get_type(crier, struct trtl_crier);
	if (!crier) return TRTL_CRY_INVALID;

	for (cry = crier->cries; cry; cry = cry->next) {
		if (cry->cry == cryid) {
			// Found it:
			break;
		}
	}

	if (cry == NULL) {
		warning("Did not find cry %d\n", cry);
		return false;
	}

	for (struct cry_cb *cb = cry->cbs; cb; cb = cb->next) {
		cb->cb(cb->user_data, cryid, data);
	}

	return true;
}

/**
 * Listen for a particular cry by name.
 *
 * Creates the cry if it doesn't exist.
 */
int
trtl_crier_listen(struct trtl_crier *crier, const char *name, trtl_crier_listen_callback callback,
		  void *user_data)
{
	crier = talloc_get_type(crier, struct trtl_crier);
	if (!crier) return TRTL_CRY_INVALID;

	struct cry *cry;
doagain:
	// FIXME: these should all be helpers
	for (cry = crier->cries; cry; cry = cry->next) {
		if (streq(cry->name, name)) {
			// Found it:
			break;
		}
	}

	if (cry == NULL) {
		trtl_crier_new_cry(crier, name);
		// FIXME: Urgh
		goto doagain;
	}
	assert(cry);

	struct cry_cb *cb = talloc(cry, struct cry_cb);
	cb->cryid = cry->cry;
	cb->cb = callback;
	cb->user_data = user_data;
	cb->next = cry->cbs;
	cry->cbs = cb;

	return 0;
}
