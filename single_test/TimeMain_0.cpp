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


int main(int argc, char* argv[])
{
	int testnum = 2000;
  	if (argc > 1) {
      	testnum = atoi(argv[1]);
  	}
	char *p;

	errno = 0;
	int bufferSizez = 0; // the size of the buffer is fixed to NODE_SIZE 
	//uint64_t numEllemens =0;
	MpScQueue<int> queue(bufferSizez);

	uint64_t begin;
  	uint64_t end;
  	uint64_t mean_w=0,max_w=0,mean_r=0,max_r=0;
  	int value = 1;

	for (int i=0;i<testnum;i++){
        begin = rdtsc();

        queue.enqueue(i);

        end = rdtsc();
        
        mean_w =mean_w + (end - begin);
        if(max_w<(end - begin)) max_w = (end - begin);
    
  	}

	for (int i=0;i<testnum;i++){
        begin = rdtsc();

        queue.dequeue(value);

        end = rdtsc();
        mean_r =mean_r + (end - begin);
        if(max_r<(end - begin)) max_r = (end - begin);
  	}  
  	mean_w = mean_w/testnum;
 	mean_r = mean_r/testnum;
    
  	printf("Producer:The mean delay is %ld ticks, the max dealy is %ld ticks\n",mean_w,max_w);
  	printf("Consumer:The mean delay is %ld ticks, the max dealy is %ld ticks\n",mean_r,max_r);

	return 0;
}