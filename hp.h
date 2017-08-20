#ifndef HP_H
#define HP_H

#include <atomic>

#define MAX_THREADS 5

template <typename T>
static void destruct(void *ptr)
{
	delete static_cast<T*>(ptr);
}

struct ReclamationNode
{
	ReclamationNode *next;
	void *data;
	std::function<void(void*)> destructor;
	int length;

	template <typename T>
	ReclamationNode(T *_data) :
		next(nullptr),
		data(_data),
		destructor(&destruct<T>),
		length(1)
	{}

	~ReclamationNode()
	{
		destructor(data);
	}
};

std::atomic<void *> &get_hazard_pointer();
bool hazard_pointer_is_acquired(void *ptr);

void delete_deffered_objects(void);
void add_to_reclamation_list(ReclamationNode *node);

/*
 * reclaim_later
 *		Adds data pointer to a reclamation list and calls reclamation routine
 *		if there are many enough objects to guarantee that at least some of
 *		them will be deleted
 */
template <typename T>
void reclaim_later(T *data)
{
	ReclamationNode *new_node = new ReclamationNode(data);
	add_to_reclamation_list(new_node);

	/* Clear reclamation list when it gains certain length */
	if (new_node->length > MAX_THREADS * 2)
		delete_deffered_objects();
}


#endif /* HP_H */

