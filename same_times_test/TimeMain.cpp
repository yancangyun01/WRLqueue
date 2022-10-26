// this main test the queue by fix operation and testing the time


//g++ TimeMain.cpp -std=c++11 -O3 -march=native  -o output  -pthread -fexceptions


//./output 48 100 100000


#include <stdio.h>
#include <stdlib.h> 
#include <time.h> 
#include <fstream>
#include <cmath>
#include <iostream>
#include <thread>
#include <vector>
#include <numa.h>
#include <string>
#include <algorithm> 
#include <chrono>
#include "time.h"

#include "MpScQueue.h"


std::atomic<long> totalNumMpScQueueAction(0);
long NumDeq=0,FailDeq=0;
bool run = false;
int thread_num = 1;
int time_to_sleep=1;
bool stop = false;


void Producer(MpScQueue<int> &queue, uint64_t size, int id)
{
	long numMyOps = 0;
	int res = 1;
	//---------------------------------
    //内核绑定
    // cpu_set_t set;
    // CPU_ZERO(&set);
    // CPU_SET(id, &set);
    // pthread_setaffinity_np(pthread_self(),sizeof(set),&set);
    //---------------------------------
    //设置优先级
    // struct sched_param sp;
    // bzero((void*)&sp, sizeof(sp));
    // int policy = sched_get_priority_max(SCHED_FIFO);
    // sp.sched_priority = policy;
    // pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    //---------------------------------
	while (run == false)
	{			
		std::atomic_thread_fence(std::memory_order_seq_cst);
		sched_yield();
	}
	

	while ( stop==false)
	{
		queue.enqueue(id);
		numMyOps++;
	}
	totalNumMpScQueueAction.fetch_add(numMyOps);
	printf("Producer ID %d:number of operations is %ld\n",id,numMyOps);

}

void Consumer(MpScQueue<int> &queue, uint64_t size)
{
	long numMyOps = 0, num = 0;
	int res = 1;
	//---------------------------------
    //内核绑定
    // cpu_set_t set;
    // CPU_ZERO(&set);
    // CPU_SET(0, &set);
    // pthread_setaffinity_np(pthread_self(),sizeof(set),&set);
    //---------------------------------
    //设置优先级
    // struct sched_param sp;
    // bzero((void*)&sp, sizeof(sp));
    // int policy = sched_get_priority_max(SCHED_FIFO);
    // sp.sched_priority = policy;
    // pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    //---------------------------------
	while (run == false)
	{			
		std::atomic_thread_fence(std::memory_order_seq_cst);
		sched_yield();
	}
	

	while ( stop==false)
	{
		if(queue.dequeue(res)){
			num++;
		}
		numMyOps++;
	}
	NumDeq =num;
	FailDeq = NumDeq-num;
	printf("Consumer:number of data fetched %ld, last dequeue value is %d\n",num,res);

}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
void testMyqueueWithDeq(int num_threads, uint64_t size, int numEllemens)
{

	MpScQueue<int> mpScQueue(size);
	std::vector<std::thread> threads(num_threads);
	//int cpu_out_list[128] = { 0,64,8,72,16,80,24,88,32,96,40,104,48,112,56,120,2,66,10,74,18,82,26,90,34,98,42,106,50,114,58,122,4,68,12,76,20,84,28,92,36,100,44,108,52,116,60,124,6,70,14,78,22,86,30,94,38,102,46,110,54,118,62,126
		//,1,65, 9,73,17,81, 25 ,89,33,97, 41,105, 49,113 ,57,121,3,67, 11,75, 19,83, 27,91, 35,99, 43,107, 51,115, 59,123,5,69, 13,77, 21,85, 29,93, 37,101, 45,109, 53 ,117,61,126,7,71, 15,79, 23,87, 31,95, 39 ,103,47,111, 55,119, 63,127 }; // the second thread of  cpu 1 ( that the outer and inner thread will run on the same cpu) 

	//int cpu_out_list[128] = { 0 , 64,8 , 72,16 , 80,24 , 88,32 , 96,40 , 104,48 , 112,56 , 120,2 , 66,10 , 74,18 , 82,26 , 90,34 , 98,42 , 106,50 , 114,58 , 122,4 , 68,12 , 76,20 , 84,28 , 92,36 , 100,44 , 108,52 , 116,60 , 124,6 , 70,14 , 78,22 , 86,30 , 94,38 , 102,46, 110,54 , 118,62 , 126,1 , 65,9 , 73,17 , 81,25 , 89,33 , 97,41 , 105,49 , 113,57 , 121,3 , 67,11 , 75,19 , 83,27 , 91,35 , 99,43 , 107,51 , 115,59 , 123,5 ,69,13 , 77,21 , 85,29 , 93,37 , 101,45 , 109,53 , 117,61 , 125,7 , 71,15 , 79,23, 87,31 , 95,39 , 103,47 , 111,55 , 119,63 , 127 };
	int cpu_out_list[128] = { 2 , 66,10 , 74,18 , 82,26 , 90,34 , 98,42 , 106,50 , 114,58 , 122,4 , 68,12 , 76,20 , 84,28 , 92,36 , 100,44 , 108,52 , 116,60 , 124,6 , 70,14 , 78,22 , 86,30 , 94,38 , 102,46, 110,54 , 118,62 , 126,0 , 64,8 , 72,16 , 80,24 , 88,32 , 96,40 , 104,48 , 112,56 , 120 ,1 , 65,9 , 73,17 , 81,25 , 89,33 , 97,41 , 105,49 , 113,57 , 121,3 , 67,11 , 75,19 , 83,27 , 91,35 , 99,43 , 107,51 , 115,59 , 123,5 ,69,13 , 77,21 , 85,29 , 93,37 , 101,45 , 109,53 , 117,61 , 125,7 , 71,15 , 79,23, 87,31 , 95,39 , 103,47 , 111,55 , 119,63 , 127 };

if (num_threads>1)
{
	threads[0] = std::thread(Consumer, std::ref(mpScQueue), numEllemens);
 }
 else{
 threads[0] = std::thread(Producer, std::ref(mpScQueue), numEllemens, 0);
 }

	for (int i = 1; i < num_threads; i++)
	{
		threads[i] = std::thread(Producer, std::ref(mpScQueue), numEllemens, i);
	}

	std::atomic_thread_fence(std::memory_order_seq_cst);
	run = true;
	std::this_thread::sleep_for(std::chrono::seconds(time_to_sleep));
	stop = true;


	for (auto& t : threads) {
		t.join();
	}

	
	std::atomic_thread_fence(std::memory_order_seq_cst);
	
	long total_actions = totalNumMpScQueueAction.load() +NumDeq;
	cout << "***\ttotal Producer actions are "<<totalNumMpScQueueAction.load()/ time_to_sleep<<" frequency\t***"<< endl;
	cout << "***\ttotal Consumer actions are "<<NumDeq/ time_to_sleep<<" frequency\t***"<< endl;
	cout << "***\ttotal Consumer failed are "<<FailDeq/ time_to_sleep<<" frequency\t***"<< endl;
	cout << "***\ttotal Consumer success number is "<<(NumDeq-FailDeq)/ time_to_sleep<<" frequency\t***"<< endl;
	cout << "***\ttotal actions are "<<total_actions/ time_to_sleep<<" frequency\t***"<< endl;
	cout << num_threads << " , " << total_actions << " , " << total_actions / time_to_sleep  << " , " <<time_to_sleep  << " , "<< NumDeq<< endl;

}



int main(int argc, char* argv[])
{
	if (argc < 1)
	{
		cout << " need to specify the amount of threads  " << endl;
		return 0;
	}
	
	char *p;

	errno = 0;
	thread_num = std::stoi(argv[1], nullptr, 10);
	if (argc > 2) {
        time_to_sleep = atoi(argv[2]);
    }
	int bufferSizez = 0; // the size of the buffer is fixed to NODE_SIZE 
	uint64_t numEllemens =0;


	// for graph in paper 
		//testMyqueue(thread, bufferSizez, numEllemens);
		testMyqueueWithDeq(thread_num, bufferSizez, numEllemens);


	

	//int a;
	//scanf("%d", &a);
	return 0;
}