#include <atomic>
#include <thread>
#include <map>

#include "hp.h"

class ReclamationNode;

struct HazardPointer
{
	std::atomic<std::thread::id>	thread;
	std::atomic<void *>				data;
};

/*
 * Hazard pointers array. Each thread gets its own hp (see hp_holder for
 * details)
 */
HazardPointer hazard_pointers[MAX_THREADS];

/* Reclamation list */
std::atomic<ReclamationNode *> reclamation_head;

/*
 * hp_holder
 *		Utility class which helps to assign the thread that requests this
 *		holder to a certain position in the HP array
 */
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
		/*
		 * Reset hazard pointer when thread finishes to allow future threads
		 * to reuse it
		 */
		hp->thread.store(std::thread::id());
		hp->data.store(nullptr);
	}

	HazardPointer *get_hazard_pointer()
	{
		return hp;
	}
};

/*
 * get_hazard_pointer
 *		Return hazard pointer associated with caller's thread
 */
std::atomic<void *> &get_hazard_pointer()
{
	thread_local static hp_holder holder;
	return holder.get_hazard_pointer()->data;
}

/* Reclamation */
void add_to_reclamation_list(ReclamationNode *node)
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

void delete_deffered_objects(void)
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

