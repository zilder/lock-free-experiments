#ifndef HP_H
#define HP_H

#include <atomic>
#include <thread>
#include <map>

#define MAX_THREADS 5

class ReclamationNode;

static void add_to_reclamation_list(ReclamationNode *node);
static void delete_deffered_objects(void);

struct HazardPointer
{
	std::atomic<std::thread::id>	thread;
	std::atomic<void *>				data;
};

HazardPointer hazard_pointers[MAX_THREADS];

class hp_holder
{
private:
	HazardPointer *hp;
public:
	hp_holder()
	{
		for (int i = 0; i < MAX_THREADS; ++i)
		{
			std::thread::id empty;

			/* If slot is empty then keep it to myself */
			if (hazard_pointers[i].thread.compare_exchange_strong(empty, std::this_thread::get_id()))
			{
				hp = &hazard_pointers[i];
				break;
			}

			/* TODO: add check for case when there are no empty slots */
		}
	}

	~hp_holder()
	{
		hp->thread.store(std::thread::id());
		hp->data.store(nullptr);
	}

	HazardPointer *get_hazard_pointer()
	{
		return hp;
	}
};

std::atomic<void *> &get_hazard_pointer()
{
	thread_local static hp_holder holder;
	return holder.get_hazard_pointer()->data;
}


/* Reclamation */

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

template <typename T>
void reclaim_later(T *data)
{
	ReclamationNode *new_node = new ReclamationNode(data);
	add_to_reclamation_list(new_node);

	/* Clear reclamation list when it gains certain length */
	if (new_node->length > MAX_THREADS * 2)
		delete_deffered_objects();
}

std::atomic<ReclamationNode *> reclamation_head;

static void add_to_reclamation_list(ReclamationNode *node)
{
	node->next = reclamation_head.load();
	do
	{
		node->length = (node->next ? node->next->length : 0) + 1;
	}
	while(!reclamation_head.compare_exchange_weak(node->next, node));
}

bool hazard_pointer_is_acquired(void *ptr)
{
	for (int i = 0; i < MAX_THREADS; ++i)
	{
		if (hazard_pointers[i].data == ptr)
			return true;
	}

	return false;
}

static void delete_deffered_objects(void)
{
	ReclamationNode *new_list = NULL;
	ReclamationNode *old_list = reclamation_head.load();
	while(!reclamation_head.compare_exchange_weak(old_list, nullptr));

	printf("cleanup\n");

	while (old_list)
	{
		ReclamationNode *tmp = old_list->next;

		if (!hazard_pointer_is_acquired(old_list))
			delete old_list;
		else
			add_to_reclamation_list(old_list);

		old_list = tmp;
	}
}

#endif /* HP_H */

