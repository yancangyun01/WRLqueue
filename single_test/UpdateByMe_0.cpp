#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include "time.h"
//-------------------------------
#include <atomic>
#include <vector>
#include <stdint.h>
#include <sys/sysinfo.h>
//-------------------------------

//-------------------------------
const size_t queueSize = 16*16;

//-------------------------------

struct ringbuffer {
  std::vector<int> data_{};
  alignas(64) std::atomic<size_t> readIdx_{0};
  //alignas(64) size_t writeIdxCached_{0};
  alignas(64) std::atomic<size_t> writeIdx_{0};
  //alignas(64) size_t readIdxCached_{0};

  ringbuffer(size_t capacity) : data_(capacity, -1) {}

  bool push(int val) {
    auto writeIdx = writeIdx_.fetch_add(1, std::memory_order_relaxed);
    writeIdx = writeIdx & (queueSize-1);
    
    if(data_[writeIdx]==-1){
        if(__sync_bool_compare_and_swap(&data_[writeIdx], -1, val)){
            return true;
        }
    }

    return false;
  }

  bool pop(int &val) {
    auto readIdx = readIdx_.fetch_add(1, std::memory_order_relaxed);
    readIdx = readIdx & (queueSize-1);
    
    if(data_[readIdx]!=-1){
        val = data_[readIdx];
        data_[readIdx] = -1;
        return true;
    }

    return false;
  }
};

struct ringbuffer q(queueSize);
//-----------------------------------

//-----------------------------------


int main(int argc, char* argv[])
{
  int testnum = 2000;
  if (argc > 1) {
      testnum = atoi(argv[1]);
  }
  uint64_t begin;
  uint64_t end;
  uint64_t mean_w=0,max_w=0,mean_r=0,max_r=0;
  int value = 1;

  for (int i=0;i<testnum;i++){
        begin = rdtsc();

        q.push(i);

        end = rdtsc();
        
        mean_w =mean_w + (end - begin);
        if(max_w<(end - begin)) max_w = (end - begin);
    
  }
  for (int i=0;i<testnum;i++){
        begin = rdtsc();

        q.pop(value);

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
