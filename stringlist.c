

#include <talloc.h>

#include "helpers.h"
#include "stringlist.h"

trtl_alloc trtl_must_check struct trtl_stringlist *
trtl_stringlist_add(struct trtl_stringlist *stringlist, const char *str) {
	struct trtl_stringlist *list, *tmp;

	list = talloc_zero(stringlist, struct trtl_stringlist);
	if (list == NULL) {
		// FIXME: Is this the right error handling?
		return NULL;
	}

	list->string = talloc_strdup(list, str);
	
	if (stringlist == NULL) {
		return list;
	}

	for (tmp = stringlist ; tmp->next != NULL ; tmp = tmp->next)
		;
	tmp->next = list;

	return stringlist;
}
