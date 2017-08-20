#ifndef STACK_H
#define STACK_H

#include <memory>
#include "hp.h"

template <typename T>
class Stack
{
private:
	struct Node
	{
		std::shared_ptr<T> data;
		Node *next;

		Node (T const &data_) :
			data(std::make_shared<T>(data_)), next(nullptr)
		{}
	};

	std::atomic<Node *> head;
public:
	Stack()
	{
		head.store(nullptr);
	}

	void push(T &data)
	{
		Node *new_node = new Node(data);
		new_node->next = head.load();

		/* replace old head with a new one */
		while(!head.compare_exchange_weak(new_node->next, new_node));
	}

	std::shared_ptr<T> pop()
	{
		std::atomic<void *> &hp = get_hazard_pointer();
		Node *old_head = head.load();

		do
		{
			/* Make sure the hazard pointer acquires the right node */
			Node *temp;
			do
			{
				temp = old_head;
				hp.store(old_head);
				old_head = head.load();
			}
			while (old_head != head);
		}
		while (old_head &&
			!head.compare_exchange_weak(old_head, old_head->next));
		hp.store(nullptr);

		//std::shared_ptr<T> res = old_head ?
		//	old_head->data : std::make_shared<T>(nullptr);
		std::shared_ptr<T> res;

		if (old_head)
		{
			res.swap(old_head->data);
			/*
			 * Delete old head right away if no hazard pointers aquired. Otherwise
			 * defer reclamation
			 */
			if (hazard_pointer_is_acquired(old_head))
				delete old_head;
			else
				reclaim_later(old_head);
		}

		return res;
	}
};

#endif /* STACK_H */

