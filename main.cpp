#include <stdio.h>
#include <thread>
#include "stack.h"

static void thread_routine(void);

Stack<int> stack;


int main()
{
	std::thread thread1(thread_routine);
	std::thread thread2(thread_routine);

	thread1.join();
	thread2.join();
}

static void thread_routine(void)
{
	std::shared_ptr<int> val;

	for (int i = 0; i < 20; i++)
	{
		stack.push(i);
	}

	while ((val = stack.pop()))
	{
		printf("value: %i\n", *val);
	}
}
