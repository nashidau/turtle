
// Interface for render objects; implement these methods
struct renderobject {
	void (*draw)(struct renderobject *);
};
