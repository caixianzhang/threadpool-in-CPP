#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "threadpool.h"

////////////////////////////////////////////////////////////////
//	C语言版本
////////////////////////////////////////////////////////////////

pthread_queue *pthread_queue_idle;

int pthread_pool_size = THREAD_DEF_NUM;

pthread_t pthread_manager_pid;
pthread_t monitor_pthread_pool_pid;
task_queue *waiting_task_queue;

void *single_pthread_work(void * arg)
{
	pthread_detach(pthread_self());

	pthread_node *self = (pthread_node *)arg;

	pthread_mutex_lock(&self->mutex);
	self->pid = pthread_self();
	pthread_mutex_unlock(&self->mutex);

	while(1)
	{
		pthread_mutex_lock(&self->mutex);
		if(self->task == NULL)
		{
			pthread_cond_wait(&self->cond, &self->mutex);
			if(self->pthread_exit_flag == 1)
			{
				pthread_mutex_unlock(&self->mutex);
				pthread_mutex_destroy(&self->mutex);
				pthread_cond_destroy(&self->cond);
				free(self);

				return (void *)0;
			}
		}
		
		pthread_mutex_lock(&self->task->mutex);
		self->task->func(self->task->arg);
		free(self->task->arg);
		self->task->arg = NULL;
		
		pthread_mutex_unlock(&self->task->mutex);
		pthread_mutex_destroy(&self->task->mutex);

		free(self->task);
		self->task = NULL;
		self->is_execute_task = 0;
		
		pthread_mutex_lock(&pthread_queue_idle->mutex);
		if(pthread_queue_idle->pthread_queue_size == 0)
		{
			pthread_queue_idle->head = pthread_queue_idle->rear = self;
			self->next = NULL;
		}else
		{
			pthread_mutex_lock(&pthread_queue_idle->rear->mutex);
			pthread_queue_idle->rear->next = self;
			pthread_mutex_unlock(&pthread_queue_idle->rear->mutex);

			self->next = NULL;
			pthread_queue_idle->rear = self;
		}

		pthread_queue_idle->pthread_queue_size++;
		pthread_mutex_unlock(&pthread_queue_idle->mutex);
		
		pthread_mutex_unlock(&self->mutex);

		pthread_mutex_lock(&pthread_queue_idle->mutex);
		if(pthread_queue_idle->pthread_queue_size == 0)
		{
			pthread_cond_signal(&pthread_queue_idle->cond);
		}
		pthread_mutex_unlock(&pthread_queue_idle->mutex);
	}
	return (void *)0;
}

int create_pthread_pool()
{
	pthread_node *pre_pthread;
	pthread_node *next_pthread;

	int i;
	for(i = 0; i < pthread_pool_size; i++)
	{
		next_pthread = (pthread_node *)malloc(sizeof(pthread_node));
		if(i == 0)
		{
			next_pthread->pid = 0;
			next_pthread->is_execute_task = 0;
			next_pthread->pthread_exit_flag = 0;
			next_pthread->task = NULL;
			next_pthread->next = NULL;
			pthread_cond_init(&next_pthread->cond, NULL);
			pthread_mutex_init(&next_pthread->mutex, NULL);
			pthread_create(&next_pthread->pid, NULL, single_pthread_work, next_pthread);

			pthread_queue_idle->head = pre_pthread = next_pthread;

			continue;
		}

		if(i == pthread_pool_size - 1)
		{
			next_pthread->pid = 0;
			next_pthread->is_execute_task = 0;
			next_pthread->pthread_exit_flag = 0;
			next_pthread->task = NULL;
			next_pthread->next = NULL;

			pthread_mutex_lock(&pre_pthread->mutex);
			pre_pthread->next = next_pthread;
			pthread_mutex_unlock(&pre_pthread->mutex);

			pthread_cond_init(&next_pthread->cond, NULL);
			pthread_mutex_init(&next_pthread->mutex, NULL);
			pthread_create(&next_pthread->pid, NULL, single_pthread_work, next_pthread);

			pthread_queue_idle->rear = pre_pthread = next_pthread;

			continue;
		}

		next_pthread->pid = 0;
		next_pthread->is_execute_task = 0;
		next_pthread->pthread_exit_flag = 0;
		next_pthread->task = NULL;

		pthread_mutex_lock(&pre_pthread->mutex);
		pre_pthread->next = next_pthread;
		pthread_mutex_unlock(&pre_pthread->mutex);

		pthread_cond_init(&next_pthread->cond, NULL);
		pthread_mutex_init(&next_pthread->mutex, NULL);
		pthread_create(&next_pthread->pid, NULL, single_pthread_work, next_pthread);

		pre_pthread = next_pthread;
	}
	pthread_queue_idle->pthread_queue_size = pthread_pool_size;

	return 0;
}

void *pthread_manager(void * arg)
{
	pthread_detach(pthread_self());
	while(1)
	{
		sem_wait(&waiting_task_queue->NewTaskToExecute);
		pthread_node *temp_pthread = NULL;
		task_node *temp_task = NULL;
		pthread_mutex_lock(&waiting_task_queue->mutex);
		temp_task = waiting_task_queue->head;
		if(waiting_task_queue->task_queue_size == 1)
		{
			waiting_task_queue->head =waiting_task_queue->rear = NULL;
		}else
		{
			waiting_task_queue->head = temp_task->next;
		}

		waiting_task_queue->task_queue_size--;

		pthread_mutex_unlock(&waiting_task_queue->mutex);
		
		pthread_mutex_lock(&pthread_queue_idle->mutex);

		if(pthread_queue_idle->pthread_queue_size == 0)
		{
			pthread_cond_wait(&pthread_queue_idle->cond, &pthread_queue_idle->mutex);
		}

		temp_pthread = pthread_queue_idle->head;
		if(pthread_queue_idle->pthread_queue_size == 1)
		{
			pthread_queue_idle->head = pthread_queue_idle->rear = NULL;
		}else
		{
			pthread_mutex_lock(&temp_pthread->mutex);
			pthread_queue_idle->head = temp_pthread->next;
			pthread_mutex_unlock(&temp_pthread->mutex);
		}

		pthread_queue_idle->pthread_queue_size--;
		pthread_mutex_unlock(&pthread_queue_idle->mutex);

		temp_task->pid = temp_pthread->pid;
		temp_task->next = NULL;
		temp_task->is_work = 1;

		pthread_mutex_lock(&temp_pthread->mutex);
		temp_pthread->is_execute_task = 1;
		temp_pthread->task = temp_task;
		temp_pthread->next = NULL;
		pthread_cond_signal(&temp_pthread->cond);
		pthread_mutex_unlock(&temp_pthread->mutex);
	}
	return (void *)0;
}

int init_pthread_pool()
{
	pthread_queue_idle = (pthread_queue *)malloc(sizeof(pthread_queue));
	pthread_queue_idle->pthread_queue_size = 0;
	pthread_queue_idle->head = pthread_queue_idle->rear = NULL;
	pthread_mutex_init(&pthread_queue_idle->mutex, NULL);
	pthread_cond_init(&pthread_queue_idle->cond, NULL);
	
	waiting_task_queue = (task_queue *)malloc(sizeof(task_queue));
	waiting_task_queue->head = waiting_task_queue->rear = NULL;
	waiting_task_queue->task_queue_size = 0;
	pthread_mutex_init(&waiting_task_queue->mutex, NULL);
	sem_init(&waiting_task_queue->NewTaskToExecute, 0, 0);
	
	create_pthread_pool();
	
	pthread_create(&pthread_manager_pid, NULL, pthread_manager, NULL);
	
	return 0;
}

int AddTaskToQueue(task_node * NewTask)
{
	pthread_mutex_init(&NewTask->mutex, NULL);
	NewTask->pid = 0;
	NewTask->is_work = 0;
	NewTask->next = NULL;
	
	pthread_mutex_lock(&waiting_task_queue->mutex);
	NewTask->work_id = waiting_task_queue->task_queue_size + 1;
	if(NewTask->work_id > TASK_MAX_NUM)
	{
		pthread_mutex_unlock(&waiting_task_queue->mutex);
		printf("threadpool.c, int AddTaskToQueue(task_node * NewTask):task is too much\n");
		pthread_mutex_destroy(&NewTask->mutex);
		free(NewTask->arg);
		free(NewTask);
		return -1;
	}
	if(waiting_task_queue->task_queue_size == 0)
	{
		waiting_task_queue->head = waiting_task_queue->rear = NewTask;
		NewTask->next  = NULL;
		waiting_task_queue->task_queue_size++;
	}else
	{
		NewTask->next  = NULL;
		pthread_mutex_lock(&waiting_task_queue->rear->mutex);
		waiting_task_queue->rear->next = NewTask;
		pthread_mutex_unlock(&waiting_task_queue->rear->mutex);

		waiting_task_queue->rear = NewTask;
		waiting_task_queue->task_queue_size++;
	}
	sem_post(&waiting_task_queue->NewTaskToExecute);
	pthread_mutex_unlock(&waiting_task_queue->mutex);

	return 0;
}

void monitor_pthread_pool()
{
	int i = 0;
	pthread_node *temp_pthread = NULL;
	
	pthread_mutex_lock(&pthread_queue_idle->mutex);
	//printf("threadpool.c, we have %d thread work!\n", pthread_pool_size -  pthread_queue_idle->pthread_queue_size);
	if(pthread_queue_idle->pthread_queue_size >  THREAD_IDLE_REDUNDANCE_MAX)
	{
		while(pthread_queue_idle->pthread_queue_size > THREAD_IDLE_REDUNDANCE_MAX/2)
		{
			temp_pthread = pthread_queue_idle->head;
			pthread_mutex_lock(&temp_pthread->mutex);
			pthread_queue_idle->head = temp_pthread->next;
			temp_pthread->pthread_exit_flag = 1;
			pthread_cond_signal(&temp_pthread->cond);
			pthread_mutex_unlock(&temp_pthread->mutex);
			pthread_queue_idle->pthread_queue_size--;
			pthread_pool_size--;
		}
	}else if(pthread_queue_idle->pthread_queue_size < THREAD_IDLE_REDUNDANCE_MIN)
	{
		if(pthread_queue_idle->pthread_queue_size == 0)
		{
			temp_pthread =(pthread_node *)malloc(sizeof(pthread_node));
			
			temp_pthread->pid = 0;
			temp_pthread->is_execute_task = 0;
			temp_pthread->pthread_exit_flag = 0;
			temp_pthread->task = NULL;
			temp_pthread->next = NULL;
			
			pthread_queue_idle->head = pthread_queue_idle->rear = temp_pthread;
			

			pthread_cond_init(&temp_pthread->cond, NULL);
			pthread_mutex_init(&temp_pthread->mutex, NULL);
			pthread_create(&temp_pthread->pid, NULL, single_pthread_work, temp_pthread);
			pthread_queue_idle->pthread_queue_size++;
			pthread_pool_size++;
		}

		for(i = 0; i < 10; i++)
		{
			temp_pthread = (pthread_node *)malloc(sizeof(pthread_node));
			
			temp_pthread->pid = 0;
			temp_pthread->is_execute_task = 0;
			temp_pthread->pthread_exit_flag = 0;
			temp_pthread->task = NULL;
			temp_pthread->next = NULL;
			
			pthread_mutex_lock(&pthread_queue_idle->rear->mutex);
			pthread_queue_idle->rear = temp_pthread;
			pthread_mutex_unlock(&pthread_queue_idle->rear->mutex);
			
			pthread_cond_init(&temp_pthread->cond, NULL);
			pthread_mutex_init(&temp_pthread->mutex, NULL);
			pthread_create(&temp_pthread->pid, NULL, single_pthread_work, temp_pthread);
			pthread_queue_idle->pthread_queue_size++;
			pthread_pool_size++;
		}
	}else
	{
		//printf("threadpool.c, void monitor_pthread_pool():threadpool work well!\n");
	}
	pthread_mutex_unlock(&pthread_queue_idle->mutex);
}


////////////////////////////////////////////////////////////////
//	C++语言版本
////////////////////////////////////////////////////////////////
Task_Node::Task_Node()
{
	pthread_mutex_init(&Task_Node_Mutex, NULL);
}

Task_Node::~Task_Node()
{
	pthread_mutex_destroy(&Task_Node_Mutex);
}

inline void Task_Node::Set_Thread_ID(pthread_t Thread_ID)
{
	this->Thread_ID = Thread_ID;
}

inline void Task_Node::Set_Task_ID(int Task_ID)
{
	this->Task_ID = Task_ID;
}

inline void Task_Node::Set_Is_Work(bool Is_Task_Work)
{
	this->Is_Task_Work = Is_Task_Work;
}

inline void Task_Node::Set_Next_Task(Task_Node *Next_Task)
{
	this->Next_Task = Next_Task;
}

inline void Task_Node::Set_Task_Arg(void *Task_Arg)
{
	this->Task_Arg = Task_Arg;
}

inline void *Task_Node::Get_Task_Arg()
{
	return this->Task_Arg;
}

Task_Queue::Task_Queue()
{
	Task_Queue_Size = 0;
	pthread_mutex_init(&Task_Queue_Mutex, NULL);	
	sem_init(&Task_To_Execute, 0, 0);
	Head_Task = NULL;
	Rear_Task = NULL;	
}

virtual Task_Queue::~Task_Queue()
{
	pthread_mutex_destroy(&Task_Queue_Mutex);
	sem_destroy(&Task_To_Execute);
}
inline void Task_Queue::lock()
{
	pthread_mutex_lock(&Task_Queue_Mutex);
}
inline void Task_Queue::wait()
{
	sem_wait(&Task_To_Execute);
}
inline void Task_Queue::post()
{
	sem_post(&Task_To_Execute);
}
inline void Task_Queue::unlock()
{
	pthread_mutex_unlock(&Task_Queue_Mutex);
}

inline int Task_Queue::Get_Task_Queue_Size()
{
	return Task_Queue_Size;
}
inline void Task_Queue::Inc_Task_Queue_Size()
{
	Task_Queue_Size++;
}
inline void Task_Queue::Dec_Task_Queue_Size()
{
	Task_Queue_Size--;
}

void Task_Queue::Add_Task(Task_Node *Temp_Task)
{
	pthread_mutex_init(&NewTask->mutex, NULL);
	NewTask->pid = 0;
	NewTask->is_work = 0;
	NewTask->next = NULL;
	
	pthread_mutex_lock(&waiting_task_queue->mutex);
	NewTask->work_id = waiting_task_queue->task_queue_size + 1;
	if(NewTask->work_id > TASK_MAX_NUM)
	{
		pthread_mutex_unlock(&waiting_task_queue->mutex);
		printf("threadpool.c, int AddTaskToQueue(task_node * NewTask):task is too much\n");
		pthread_mutex_destroy(&NewTask->mutex);
		free(NewTask->arg);
		free(NewTask);
		return -1;
	}
	if(waiting_task_queue->task_queue_size == 0)
	{
		waiting_task_queue->head = waiting_task_queue->rear = NewTask;
		NewTask->next  = NULL;
		waiting_task_queue->task_queue_size++;
	}else
	{
		NewTask->next  = NULL;
		pthread_mutex_lock(&waiting_task_queue->rear->mutex);
		waiting_task_queue->rear->next = NewTask;
		pthread_mutex_unlock(&waiting_task_queue->rear->mutex);

		waiting_task_queue->rear = NewTask;
		waiting_task_queue->task_queue_size++;
	}
	sem_post(&waiting_task_queue->NewTaskToExecute);
	pthread_mutex_unlock(&waiting_task_queue->mutex);

	return 0;

	Temp_Task->Set_Task_ID(Get_Task_Queue_Size() + 1);
	if(Get_Task_Queue_Size() + 1 > Task_Max_Num)
	{
		
	}
	
}

Task_Node *Task_Queue::Get_Task()
{
	
}

Thread::Thread(Thread_Queue *Idle_Thread_Queue)
{
	//获取空闲线程队列地址
	this->Idle_Thread_Queue = Idle_Thread_Queue;

	//初始化条件变量及锁
	pthread_cond_init(&Thread_Cond, NULL);
	pthread_mutex_init(&Thread_Mutex, NULL);


	//初始化其他属性
	Is_Execute_Task = false;
	Thread_Exit_Flag = false;
	Thread_Task = NULL;
	Next_Thread = NULL;

	//启动线程
	pthread_create(&Thread_Pid, NULL, Thread_Work, this);
}

//析构函数完成线程退出
virtual Thread::~Thread()
{
	//线程退出标志位置1
	lock();
	Thread_Exit_Flag == true;;
	//通知线程退出
	signal();
	unlock();

	//等待线程退出，并回收资源
	pthread_join(Thread_Pid, NULL);

	//销毁锁及条件变量
	pthread_mutex_destroy(&Thread_Mutex);
	pthread_cond_destroy(&Thread_Cond);
}

inline void Thread::lock()
{
	pthread_mutex_lock(&Thread_Mutex);
}
inline void Thread::wait()
{
	pthread_cond_wait(&Thread_Cond, &Thread_Mutex);
}
inline void Thread::signal()
{
	pthread_cond_signal(&Thread_Cond);
}
inline void Thread::unlock()
{
	pthread_mutex_unlock(&Thread_Mutex);
}

inline void Thread::Set_Thread_Task(Task_Node *Thread_Task)
{
	this->Thread_Task = Thread_Task;
}

inline bool Thread::Set_Execute_Task_Flag(bool Is_Execute_Task)
{
	this->Is_Execute_Task = Is_Execute_Task;
}

inline bool Thread::Get_Execute_Task_Flag()
{
	return this->Is_Execute_Task;
}

inline bool Thread::Set_Thread_Exit(bool Thread_Exit_Flag)
{
	this->Thread_Exit_Flag = Thread_Exit_Flag;
}

inline bool Thread::Get_Thread_Exit()
{
	return Thread_Exit_Flag;
}

inline void Thread::Set_Next_Thread(Thread *Next_Thread)
{
	this->Next_Thread = Next_Thread;
}

void *Thread::Thread_Work(void *arg)
{
	Thread *MyThread = (Thread *)arg;
	
	while(1)
	{
		MyThread->lock();
		if(MyThread->Get_Execute_Task_Flag())
		{
			MyThread->wait();
			if(Get_Thread_Exit())
			{
				MyThread->unlock();
				return (void *)0;
			}
		}

		//执行分配给该线程的任务
		MyThread->Thread_Task->run(MyThread->Thread_Task->Get_Task_Arg());

		//任务执行完毕，释放任务节点，重置线程任务
		delete MyThread->Thread_Task;
		MyThread->Set_Thread_Task(NULL);
	
		//将本线程加入到空闲线程队列中去
		MyThread->Add_Thread_To_Idle_Queue();
	
		//解锁
		MyThread->unlock();

		//告知有新的空闲线程到来。
		Idle_Thread_Queue->Signal_New_Thread_Come();
	}
	return (void *)0;
}

Thread_Queue::Thread_Queue()
{
	pthread_mutex_init(&Thread_Queue_Mutex, NULL);
	pthread_cond_init(&Thread_Queue_Cond, NULL);

	Thread_Queue_Size = 0;
	Head_Thread = NULL;
	Rear_Thread = NULL;
}

virtual Thread_Queue::~Thread_Queue()
{
	//销毁锁及条件变量
	pthread_mutex_destroy(&Thread_Queue_Mutex);
	pthread_cond_destroy(&Thread_Queue_Cond);
}
inline void Thread_Queue::lock()
{
	pthread_mutex_lock(&Thread_Queue_Mutex);
}
inline void Thread_Queue::wait()
{
	pthread_cond_wait(&Thread_Queue_Cond, &Thread_Queue_Mutex);	
}
inline void Thread_Queue::signal()
{
	pthread_cond_signal(&Thread_Queue_Cond);
}
inline void Thread_Queue::unlock()
{
	pthread_mutex_unlock(&Thread_Queue_Mutex);
}

inline int Thread_Queue::Get_Thread_Queue_Size()
{	
	return Thread_Queue_Size;
}

inline void Thread_Queue::Inc_Thread_Queue_Size()
{
	Thread_Queue_Size++;
}

inline void Thread_Queue::Dec_Thread_Queue_Size()
{
	Thread_Queue_Size--;
}

//设置头尾节点
inline void Thread_Queue::Set_Thread_Queue_Head(Thread *Head_Thread)
{
	this->Head_Thread = Head_Thread;
}
inline void Thread_Queue::Set_Thread_Queue_Rear(Thread *Rear_Thread)
{
	this->Rear_Thread = Rear_Thread;
}

//获取头尾节点
inline Thread *Thread_Queue::Get_Thread_Queue_Head()
{
	return Head_Thread;
}
inline Thread *Thread_Queue::Get_Thread_Queue_Rear()
{
	return Rear_Thread;
}

void Thread_Queue::Add_Thread_To_Idle_Queue(Thread *Idle_Thread);
{
	//获取空闲线程队列的访问锁
	Idle_Thread_Queue->lock();
	//判断当前空闲线程队列的大小
	int Idle_Queue_Size = Idle_Thread_Queue->Get_Thread_Queue_Size();
	if(Idle_Queue_Size == 0)
	{
		Idle_Thread_Queue->Set_Thread_Queue_Head(this);
		Idle_Thread_Queue->Set_Thread_Queue_Rear(this);
		this->Set_Next_Thread(NULL);
	}else
	{
		//修改末尾线程队列尾节点线程的属性
		Thread *RearThread = Idle_Thread_Queue->Get_Thread_Queue_Rear();
		RearThread->lock();
		//将本线程加入到空闲线程队列中去
		RearThread->Set_Next_Thread(this);
		//将空闲线程队列尾节点后向指针置NULL;
		this->Set_Next_Thread(NULL);
		RearThread->unlock();

		//将空闲线程队列尾节点刷新为自己
		Idle_Thread_Queue->Set_Thread_Queue_Rear(this);
	}

	//刷新空闲线程队列大小
	Idle_Thread_Queue->Inc_Thread_Queue_Size();
	//释放空闲线程队列锁
	Idle_Thread_Queue->unlock();
}

inline void Thread_Queue::Signal_New_Thread_Come()
{
	lock();
	signal();
	unlock();
}

ThreadPool::ThreadPool()
{
	Idle_Thread_Queue = new Thread_Queue(); 
	
	Thread *pre_pthread;
	Thread *next_pthread;

	for(int i = 0; i < Idle_Thread_Num; i++)
	{
		next_pthread = new Thread();
		if(i == 0)
		{
			Idle_Thread_Queue->
			continue;
		}

		if(i == pthread_pool_size - 1)
		{
			next_pthread->pid = 0;
			next_pthread->is_execute_task = 0;
			next_pthread->pthread_exit_flag = 0;
			next_pthread->task = NULL;
			next_pthread->next = NULL;

			pthread_mutex_lock(&pre_pthread->mutex);
			pre_pthread->next = next_pthread;
			pthread_mutex_unlock(&pre_pthread->mutex);

			pthread_cond_init(&next_pthread->cond, NULL);
			pthread_mutex_init(&next_pthread->mutex, NULL);
			pthread_create(&next_pthread->pid, NULL, single_pthread_work, next_pthread);

			pthread_queue_idle->rear = pre_pthread = next_pthread;

			continue;
		}

		next_pthread->pid = 0;
		next_pthread->is_execute_task = 0;
		next_pthread->pthread_exit_flag = 0;
		next_pthread->task = NULL;

		pthread_mutex_lock(&pre_pthread->mutex);
		pre_pthread->next = next_pthread;
		pthread_mutex_unlock(&pre_pthread->mutex);

		pthread_cond_init(&next_pthread->cond, NULL);
		pthread_mutex_init(&next_pthread->mutex, NULL);
		pthread_create(&next_pthread->pid, NULL, single_pthread_work, next_pthread);

		pre_pthread = next_pthread;
	}
	pthread_queue_idle->pthread_queue_size = pthread_pool_size;

	return 0;
}

virtual ThreadPool::~ThreadPool()
{
	
}

ThreadPool::AddTask()
{
	
}


