#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "main.h"
#include "threadpool.h"

class Test:public Task_Node{
	private:
		int printtimes;
	public:
		Test(int printtimes)
		{
			this->printtimes = printtimes;
		}
		~Test()
		{
			
		}
		void run()
		{
			for(int i = 0; i < 10; i++)
			{
				//printf("%d\n", printtimes);
				sleep(1);
			}
		}
};
		
int main(int argc, char **argv)
{
	ThreadPool *MyThreadPool = new ThreadPool();

	for(int i = 0;i < 1000; i++)
	{
		MyThreadPool->Add_Task_To_ThreadPool(new Test(i));		
	}
	sleep(3);
	MyThreadPool->Clear_Thread();
	sleep(3);
	MyThreadPool->Add_Thread();
	MyThreadPool->Add_Thread();
	sleep(10000);
	return 0;
}
