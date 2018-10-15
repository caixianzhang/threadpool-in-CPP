#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "main.h"
#include "threadpool.h"

void *p(void *arg)
{
	for(int i = 0; i < *((int *)arg); i++)
	{
		printf("%d\n", *((int *)arg));
		sleep(1);
	}
	printf("exit %d\n",*((int *)arg));
	return (void *)0;
}

void *Monitor_System(void *arg)
{
	pthread_detach(pthread_self());
	while(1)
	{
		monitor_pthread_pool();
		sleep(2);
	}
	return (void *)0;
}

pthread_t monitor;
int main(int argc, char **argv)
{
	init_pthread_pool();
	pthread_create(&monitor, NULL, Monitor_System, NULL);
	for(int i = 0; i < 45; i++)
	{
		task_node *temp_task = (task_node *)malloc(sizeof(task_node));
		temp_task->arg = (int *)malloc(sizeof(int));
		*((int *)temp_task->arg) = i + 10;
		temp_task->func = p;
		AddTaskToQueue(temp_task);
	}
	while(1);
	return 0;
}
