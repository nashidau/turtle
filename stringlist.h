// Simple list of strings.
// Super inefficeint.  Just a work around for C's amazing lack of types.

// FIXME: Filename should be trtl_stringlist?

struct trtl_stringlist {
	struct trtl_stringlist *next;
	const char *string;
};

/**
 * Appends an item to the end of a string list.
 *
 * Returns a new list.  If the list is empty (NULL) returns new node.  Appending is O(n).
 *
 * To release, just call talloc_free on the head.
 *
 * @param stringline Existing list or NULL.
 * @param str Item to insert - will be duplicated.
 * @return New or updated list.
 */
struct trtl_stringlist *
trtl_stringlist_add(struct trtl_stringlist *stringlist, const char *str);
