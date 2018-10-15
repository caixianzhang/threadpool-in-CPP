#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <semaphore.h>

////////////////////////////////////////////////////////////////
//	C语言版本
////////////////////////////////////////////////////////////////


#define TASK_MAX_NUM	100
#define THREAD_IDLE_REDUNDANCE_MAX	50
#define THREAD_IDLE_REDUNDANCE_MIN	3
#define THREAD_DEF_NUM	20

typedef struct TASK_NODE{
	pthread_mutex_t mutex;
	void *arg;
	void *(*func)(void *);
	pthread_t pid;
	int work_id;
	int is_work;
	struct TASK_NODE *next;
}task_node;

typedef struct TASK_PARAMETER{
		void *arg;
		void *(*func)(void *);
}task_parameter;

typedef struct TASK_QUEUE{
	int task_queue_size;
	pthread_mutex_t mutex;
	sem_t NewTaskToExecute;
	struct TASK_NODE *head;
	struct TASK_NODE *rear;
}task_queue;

typedef struct PTHREAD_NODE{
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	pthread_t pid;
	int is_execute_task;
	int pthread_exit_flag;
	struct TASK_NODE *task;
	struct PTHREAD_NODE *next;
}pthread_node;

typedef struct PTHREAD_QUEUE{
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int pthread_queue_size;
	struct PTHREAD_NODE *head;
	struct PTHREAD_NODE *rear;
}pthread_queue;

void *single_pthread_work(void *arg);
int create_pthread_pool();
void *pthread_manager(void *arg);
int init_pthread_pool();
int AddTaskToQueue(task_node *NewTask);
void monitor_pthread_pool();


////////////////////////////////////////////////////////////////
//	C++语言版本
////////////////////////////////////////////////////////////////

//任务节点
class Task_Node{
	private:
		//访问该任务节点的锁
		pthread_mutex_t Task_Node_Mutex;

		//具体要执行的任务的参数，任务执行完毕后，参数内存由调用者释放
		//struct Task_Param *Param;
		void *Task_Arg;
		
		//执行该任务的线程ID
		pthread_t Thread_ID;

		//该任务的标记序号
		int Task_ID;

		//该任务是否开始执行
		bool Is_Task_Work;

		//该任务在队列中的下一个节点
		Task_Node *Next_Task;

	public:
		Task_Node();
		virtual ~Task_Node();

		inline void Set_Thread_ID(pthread_t Thread_ID);
		inline void Set_Task_ID(int Task_ID);
		inline void Set_Is_Work(bool Is_Task_Work);
		inline void Set_Next_Task(Task_Node *Next_Task);
		
		inline void Set_Task_Arg(void *Task_Arg);
		inline void *Get_Task_Arg();
		virtual void run(void *arg) = 0;
};
	
//任务队列
class Task_Queue{
	private:
		enum Queue_Size{Task_Max_Num = 1000};
		//任务队列大小
		int Task_Queue_Size;
		//访问任务队列锁
		pthread_mutex_t Task_Queue_Mutex;
		//任务队列信号量
		sem_t Task_To_Execute;
		//任务队列头尾节点
		Task_Queue *Head_Task;
		Task_Queue *Rear_Task;	

	public:
		Task_Queue();
		virtual ~Task_Queue();

		inline void lock();
		inline void wait();
		inline void post();
		inline void unlock();
		
		void Add_Task(Task_Node *Temp_Task);
		Task_Node *Get_Task();

		//获取队列大小
		inline int Get_Task_Queue_Size();

		//队列大小加一
		inline void Inc_Task_Queue_Size();
		//队列大小减一
		inline void Dec_Task_Queue_Size();
};

//单个线程的属性
class Thread{
	private:
		//单个线程唤醒条件变量
		pthread_cond_t Thread_Cond;

		//修改单个线程属性的线程锁
		pthread_mutex_t Thread_Mutex;

		//单个线程的线程ID
		pthread_t Thread_Pid;

		//该线程是否在执行任务
		bool Is_Execute_Task;

		//任务执行完毕后是否退出
		bool Thread_Exit_Flag;

		//分配给该线程的任务
		Task_Node *Thread_Task;

		//空闲线程队列
		Thread_Queue *Idle_Thread_Queue;
			
		//在线程队列中的下一个线程
		Thread *Next_Thread;
		
	public:
		Thread(Thread_Queue *Idle_Thread_Queue);
		virtual ~Thread();	

		inline void lock();
		inline void wait();
		inline void signal();
		inline void unlock();

		inline void Set_Thread_Task(Task_Node *Thread_Task);
	
		inline bool Set_Execute_Task_Flag(bool Is_Execute_Task);
		inline bool Get_Execute_Task_Flag();

		inline bool Set_Thread_Exit(bool Thread_Exit_Flag);
		inline bool Get_Thread_Exit();

		inline void Set_Next_Thread(Thread *Next_Thread);

		//单个线程实际开始工作
		void *Thread_Work(void *arg);
};	

//线程队列(主要针对空闲线程队列)
class Thread_Queue{
	private:
		//访问修改该线程队列的锁
		pthread_mutex_t Thread_Queue_Mutex;

		//线程队列唤醒条件变量
		pthread_cond_t Thread_Queue_Cond;

		//线程队列大小
		int Thread_Queue_Size;

		//线程队列头尾节点
		Thread *Head_Thread;
		Thread *Rear_Thread;
		
	public:
		Thread_Queue();
		virtual ~Thread_Queue();

		inline void lock();
		inline void wait();
		inline void signal();
		inline void unlock();

		//获取队列大小
		inline int Get_Thread_Queue_Size();

		//队列大小加一
		inline void Inc_Thread_Queue_Size();
		//队列大小减一
		inline void Dec_Thread_Queue_Size();

		//设置头尾节点
		inline void Set_Thread_Queue_Head(Thread *Head_Thread);
		inline void Set_Thread_Queue_Rear(Thread *Rear_Thread);

		//获取头尾节点
		inline Thread *Get_Thread_Queue_Head();
		inline Thread *Get_Thread_Queue_Rear();

		//任务执行完毕，需要将线程加入到空闲线程队列中去
		void Add_Thread_To_Idle_Queue(Thread *Idle_Thread);
		
		//提供外部接口用于外部通知有新的空闲线程到来
		inline void Signal_New_Thread_Come();
};


//线程池
class ThreadPool{
	private:
		//设置预制线程数大小为100,最大线程数为2000
		enum Thread_Num{Idle_Thread_Num = 100, Max_Thread_Num = 2000};
		
		//空闲线程队列
		Thread_Queue *Idle_Thread_Queue;

		//任务队列
		Task_Queue	*Ready_Task;
	public:
		ThreadPool();
		virtual ~ThreadPool();

		//向线程池中加入任务
		int Add_Task_To_ThreadPool();
};

#endif
