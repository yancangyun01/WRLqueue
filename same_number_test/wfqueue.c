#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sched.h>
#include "primitives.h"
#include "wfqueue.h"
#include "time.h"


#define N WFQUEUE_NODE_SIZE
#define BOT ((void *)0)
#define TOP ((void *)-1)

#define MAX_GARBAGE(n) (2 * n)

#ifndef MAX_SPIN
#define MAX_SPIN 100
#endif

#ifndef MAX_PATIENCE
#define MAX_PATIENCE 10
#endif

typedef struct _enq_t enq_t;
typedef struct _deq_t deq_t;
typedef struct _cell_t cell_t;
typedef struct _node_t node_t;

static queue_t * q;
static handle_t ** hds;

bool run = false;

int thread_number = 1;
int cpu_num = 1;
long NumDeq=0;
long NumEnq=0;
long num = 0;

static inline void *spin(void *volatile *p) {
    int patience = MAX_SPIN;
    void *v = *p;

    while (!v && patience-- > 0) {
        v = *p;
        PAUSE();
    }

    return v;
}


static inline node_t *new_node() {
    node_t *n = align_malloc(PAGE_SIZE, sizeof(node_t));
    memset(n, 0, sizeof(node_t));
    return n;
}


static node_t *check(unsigned long volatile *p_hzd_node_id, node_t *cur,
                     node_t *old) {
    unsigned long hzd_node_id = ACQUIRE(p_hzd_node_id);

    if (hzd_node_id < cur->id) {
        node_t *tmp = old;
        while (tmp->id < hzd_node_id) {
            tmp = tmp->next;
        }
        cur = tmp;
    }

    return cur;
}


static node_t *update(node_t *volatile *pPn, node_t *cur,
                      unsigned long volatile *p_hzd_node_id, node_t *old) {
    node_t *ptr = ACQUIRE(pPn);

    if (ptr->id < cur->id) {
        if (!CAScs(pPn, &ptr, cur)) {
            if (ptr->id < cur->id) cur = ptr;
        }

        cur = check(p_hzd_node_id, cur, old);
    }

    return cur;
}


static void cleanup(queue_t *q, handle_t *th) {
    long oid = ACQUIRE(&q->Hi);
    node_t *new = th->Dp;

    if (oid == -1) return;
    if (new->id - oid < MAX_GARBAGE(q->nprocs)) return;
    if (!CASa(&q->Hi, &oid, -1)) return;

    long Di = q->Di, Ei = q->Ei;
    while(Ei <= Di && !CAS(&q->Ei, &Ei, Di + 1))
        ;

    node_t *old = q->Hp;
    handle_t *ph = th;
    handle_t *phs[q->nprocs];
    int i = 0;

    do {
        new = check(&ph->hzd_node_id, new, old);
        new = update(&ph->Ep, new, &ph->hzd_node_id, old);
        new = update(&ph->Dp, new, &ph->hzd_node_id, old);

        phs[i++] = ph;
        ph = ph->next;
    } while (new->id > oid && ph != th);

    while (new->id > oid && --i >= 0) {
        new = check(&phs[i]->hzd_node_id, new, old);
    }

    long nid = new->id;

    if (nid <= oid) {
        RELEASE(&q->Hi, oid);
    } else {
        q->Hp = new;
        RELEASE(&q->Hi, nid);

        while (old != new) {
            node_t *tmp = old->next;
            free(old);
            old = tmp;
        }
    }
}

static cell_t *find_cell(node_t *volatile *ptr, long i, handle_t *th) {
    node_t *curr = *ptr;

    long j;
    for (j = curr->id; j < i / N; ++j) {
        node_t *next = curr->next;

        if (next == NULL) {
            node_t *temp = th->spare;

            if (!temp) {
                temp = new_node();
                th->spare = temp;
            }

            temp->id = j + 1;

            if (CASra(&curr->next, &next, temp)) {
                next = temp;
                th->spare = NULL;
            }
        }

        curr = next;
    }

    *ptr = curr;
    return &curr->cells[i % N];
}

static int enq_fast(queue_t *q, handle_t *th, void *v, long *id) {
    long i = FAAcs(&q->Ei, 1);
    cell_t *c = find_cell(&th->Ep, i, th);
    void *cv = BOT;

    if (CAS(&c->val, &cv, v)) {
#ifdef RECORD
        th->fastenq++;
#endif
        return 1;
    } else {
        *id = i;
        return 0;
    }
}

static void enq_slow(queue_t *q, handle_t *th, void *v, long id) {
    enq_t *enq = &th->Er;
    enq->val = v;
    RELEASE(&enq->id, id);

    node_t *tail = th->Ep;
    long i;
    cell_t *c;

    do {
        i = FAA(&q->Ei, 1);
        c = find_cell(&tail, i, th);
        enq_t *ce = BOT;

        if (CAScs(&c->enq, &ce, enq) && c->val != TOP) {
            if (CAS(&enq->id, &id, -i)) id = -i;
            break;
        }
    } while (enq->id > 0);

    id = -enq->id;
    c = find_cell(&th->Ep, id, th);
    if (id > i) {
        long Ei = q->Ei;
        while (Ei <= id && !CAS(&q->Ei, &Ei, id + 1))
            ;
    }
    c->val = v;

#ifdef RECORD
    th->slowenq++;
#endif
}

void enqueue(queue_t *q, handle_t *th, void *v) {
    th->hzd_node_id = th->enq_node_id;

    long id;
    int p = MAX_PATIENCE;
    while (!enq_fast(q, th, v, &id) && p-- > 0)
        ;
    if (p < 0) enq_slow(q, th, v, id);

    th->enq_node_id = th->Ep->id;
    RELEASE(&th->hzd_node_id, -1);
}

static void *help_enq(queue_t *q, handle_t *th, cell_t *c, long i) {
    void *v = spin(&c->val);

    if ((v != TOP && v != BOT) ||
        (v == BOT && !CAScs(&c->val, &v, TOP) && v != TOP)) {
        return v;
    }

    enq_t *e = c->enq;

    if (e == BOT) {
        handle_t *ph;
        enq_t *pe;
        long id;
        ph = th->Eh, pe = &ph->Er, id = pe->id;

        if (th->Ei != 0 && th->Ei != id) {
            th->Ei = 0;
            th->Eh = ph->next;
            ph = th->Eh, pe = &ph->Er, id = pe->id;
        }

        if (id > 0 && id <= i && !CAS(&c->enq, &e, pe) && e != pe)
            th->Ei = id;
        else {
            th->Ei = 0;
            th->Eh = ph->next;
        }

        if (e == BOT && CAS(&c->enq, &e, TOP)) e = TOP;
    }

    if (e == TOP) return (q->Ei <= i ? BOT : TOP);

    long ei = ACQUIRE(&e->id);
    void *ev = ACQUIRE(&e->val);

    if (ei > i) {
        if (c->val == TOP && q->Ei <= i) return BOT;
    } else {
        if ((ei > 0 && CAS(&e->id, &ei, -i)) || (ei == -i && c->val == TOP)) {
            long Ei = q->Ei;
            while (Ei <= i && !CAS(&q->Ei, &Ei, i + 1))
                ;
            c->val = ev;
        }
    }

    return c->val;
}

static void help_deq(queue_t *q, handle_t *th, handle_t *ph) {
    deq_t *deq = &ph->Dr;
    long idx = ACQUIRE(&deq->idx);
    long id = deq->id;

    if (idx < id) return;

    node_t *Dp = ph->Dp;
    th->hzd_node_id = ph->hzd_node_id;
    FENCE();
    idx = deq->idx;

    long i = id + 1, old = id, new = 0;
    while (1) {
        node_t *h = Dp;
        for (; idx == old && new == 0; ++i) {
            cell_t *c = find_cell(&h, i, th);

            long Di = q->Di;
            while (Di <= i && !CAS(&q->Di, &Di, i + 1))
                ;

            void *v = help_enq(q, th, c, i);
            if (v == BOT || (v != TOP && c->deq == BOT))
                new = i;
            else
                idx = ACQUIRE(&deq->idx);
        }

        if (new != 0) {
            if (CASra(&deq->idx, &idx, new)) idx = new;
            if (idx >= new) new = 0;
        }

        if (idx < 0 || deq->id != id) break;

        cell_t *c = find_cell(&Dp, idx, th);
        deq_t *cd = BOT;
        if (c->val == TOP || CAS(&c->deq, &cd, deq) || cd == deq) {
            CAS(&deq->idx, &idx, -idx);
            break;
        }

        old = idx;
        if (idx >= i) i = idx + 1;
    }
}

static void *deq_fast(queue_t *q, handle_t *th, long *id) {
    long i = FAAcs(&q->Di, 1);
    cell_t *c = find_cell(&th->Dp, i, th);
    void *v = help_enq(q, th, c, i);
    deq_t *cd = BOT;

    if (v == BOT) return BOT;
    if (v != TOP && CAS(&c->deq, &cd, TOP)) return v;

    *id = i;
    return TOP;
}

static void *deq_slow(queue_t *q, handle_t *th, long id) {
    deq_t *deq = &th->Dr;
    RELEASE(&deq->id, id);
    RELEASE(&deq->idx, id);

    help_deq(q, th, th);
    long i = -deq->idx;
    cell_t *c = find_cell(&th->Dp, i, th);
    void *val = c->val;

#ifdef RECORD
    th->slowdeq++;
#endif
    return val == TOP ? BOT : val;
}

void *dequeue(queue_t *q, handle_t *th) {
    th->hzd_node_id = th->deq_node_id;

    void *v;
    long id = 0;
    int p = MAX_PATIENCE;

    do
        v = deq_fast(q, th, &id);
    while (v == TOP && p-- > 0);
    if (v == TOP)
        v = deq_slow(q, th, id);
    else {
#ifdef RECORD
        th->fastdeq++;
#endif
    }

    if (v != EMPTY) {
        help_deq(q, th, th->Dh);
        th->Dh = th->Dh->next;
    }

    th->deq_node_id = th->Dp->id;
    RELEASE(&th->hzd_node_id, -1);

    if (th->spare == NULL) {
        cleanup(q, th);
        th->spare = new_node();
    }

#ifdef RECORD
    if (v == EMPTY) th->empty++;
#endif
    return v;
}

void queue_init(queue_t *q, int nprocs) {
    q->Hi = 0;
    q->Hp = new_node();

    q->Ei = 1;
    q->Di = 1;

    q->nprocs = nprocs;

#ifdef RECORD
    q->fastenq = 0;
    q->slowenq = 0;
    q->fastdeq = 0;
    q->slowdeq = 0;
    q->empty = 0;
#endif
}

void queue_free(queue_t *q, handle_t *h) {
#ifdef RECORD
    static int lock = 0;

    FAA(&q->fastenq, h->fastenq);
    FAA(&q->slowenq, h->slowenq);
    FAA(&q->fastdeq, h->fastdeq);
    FAA(&q->slowdeq, h->slowdeq);
    FAA(&q->empty, h->empty);

    pthread_barrier_wait(&barrier);

    if (FAA(&lock, 1) == 0)
        printf("Enq: %f Deq: %f Empty: %f\n",
               q->slowenq * 100.0 / (q->fastenq + q->slowenq),
               q->slowdeq * 100.0 / (q->fastdeq + q->slowdeq),
               q->empty * 100.0 / (q->fastdeq + q->slowdeq));
#endif
}

void queue_register(queue_t *q, handle_t *th, int id) {
    th->next = NULL;
    th->hzd_node_id = -1;
    th->Ep = q->Hp;
    th->enq_node_id = th->Ep->id;
    th->Dp = q->Hp;
    th->deq_node_id = th->Dp->id;

    th->Er.id = 0;
    th->Er.val = BOT;
    th->Dr.id = 0;
    th->Dr.idx = -1;

    th->Ei = 0;
    th->spare = new_node();
#ifdef RECORD
    th->slowenq = 0;
    th->slowdeq = 0;
    th->fastenq = 0;
    th->fastdeq = 0;
    th->empty = 0;
#endif

    static handle_t *volatile _tail;
    handle_t *tail = _tail;

    if (tail == NULL) {
        th->next = th;
        if (CASra(&_tail, &tail, th)) {
            th->Eh = th->next;
            th->Dh = th->next;
            return;
        }
    }

    handle_t *next = tail->next;
    do
        th->next = next;
    while (!CASra(&tail->next, &next, th));

    th->Eh = th->next;
    th->Dh = th->next;
}

void thread_init(int id) {
  hds[id] = align_malloc(PAGE_SIZE, sizeof(handle_t));
  queue_register(q, hds[id], id);
}

void* Producer(void* arg){
    int data = *(int*)arg;
    uint64_t begin=0;
    uint64_t end=0;
    uint64_t mean=0,max=0;
    thread_init(data);
    long numMyOps = 0;
    void * val = (void *) (intptr_t) (data);
    handle_t * th = hds[data];
    //---------------------------------
    //内核绑定
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(val, &set);
    pthread_setaffinity_np(pthread_self(),sizeof(set),&set);
    //---------------------------------
    //设置优先级
    struct sched_param sp;
    bzero((void*)&sp, sizeof(sp));
    int policy = sched_get_priority_max(SCHED_FIFO);
    sp.sched_priority = policy;
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    //---------------------------------
    while (run == false){
        sched_yield();
    }

    while (numMyOps < 3000){
        begin = rdtsc();

        enqueue(q, th, val);

        end = rdtsc();
        mean = mean + (end-begin);
        if(max<(end - begin)) max = (end - begin);

        numMyOps++;
    }

    mean = mean/numMyOps;
    printf("\tProducer ID %d: mean ticks is %ld ,max ticks is %ld \n",data,mean,max);

    __sync_fetch_and_add(&NumEnq, numMyOps);
    printf("Producer ID %d:number of operations is %ld\n",data,numMyOps);
    queue_free(q, hds[data]);
    return NULL;
}

void* Consumer(void* arg){ 
    uint64_t begin=0;
    uint64_t end=0;
    uint64_t mean=0,max=0;
    
    thread_init(0);
    long numMyOps = 0;
    void * val=(void *) (intptr_t) (0);
    int data = 0;
    handle_t * th = hds[0];
    //---------------------------------
    //内核绑定
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(val, &set);
    pthread_setaffinity_np(pthread_self(),sizeof(set),&set);
    //---------------------------------
    //设置优先级
    struct sched_param sp;
    bzero((void*)&sp, sizeof(sp));
    int policy = sched_get_priority_max(SCHED_FIFO);
    sp.sched_priority = policy;
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    //---------------------------------
    while (run == false){
        sched_yield();
    }

    while (numMyOps < 3000){
        begin = rdtsc();

        val = dequeue(q, th);
        data = (int) (intptr_t)val;

        end = rdtsc();
        mean = mean + (end-begin);
        if(max<(end - begin)) max = (end - begin);

        if(data!=0 && data!=-1){
            num++;
        }
        numMyOps++;
    }
    mean = mean/numMyOps;
    printf("\tConsumer: mean ticks is %ld ,max ticks is %ld \n",mean,max);

    NumDeq = numMyOps;
    printf("Consumer:number of data fetched %ld, last dequeue value is %d\n",num,data);
    queue_free(q, hds[0]);
    return NULL;
}

void testMyqueue(int num_threads){
    pthread_t threadPool[num_threads];
    int data[num_threads+1];
    //初始化队列
    q = align_malloc(PAGE_SIZE, sizeof(queue_t));
    queue_init(q, num_threads);
    hds = align_malloc(PAGE_SIZE, sizeof(handle_t * [num_threads]));


    data[0] = 0;

    if (num_threads>1){
        if(pthread_create(&threadPool[0], NULL, Consumer, NULL) != 0){
        printf("ERROR, fail to create consumer\n");
        exit(1);
        }
    }
    else{
        if(pthread_create(&threadPool[0], NULL, Producer, &data[0]) != 0){
        printf("ERROR, fail to create producer\n");
        exit(1);
        }
    }
    for(int i=1;i<num_threads;i++){
        data[i] = i;
        if(pthread_create(&threadPool[i], NULL, Producer, &data[i]) != 0){
        printf("ERROR, fail to create consumer\n");
        exit(1);
        }
    }
    run = true;
    
    for(int i=0;i<num_threads;i++){
        pthread_join(threadPool[i],NULL);
    }
    
    printf("***\ttotal Producer actions are %ld frequency\t***\n",NumEnq);
    printf("***\ttotal Consumer actions are %ld frequency\t***\n",NumDeq);
    printf("***\ttotal actions are %ld frequency\t***\n",NumEnq+NumDeq);
    if(NumEnq>=NumDeq){
        printf("***\tThe write data failed number is %ld\t***\n",NumEnq-num);
    }
}

int main(int argc, char* argv[])
{
    if (argc < 1)
	{
		printf(" **********need to specify the amount of threads and running time************* \n");
		return 0;
	}
    printf("  Benchmark: %s\n", argv[0]);

    if (argc > 1) {
        thread_number = atoi(argv[1]);
    }
    cpu_num = get_nprocs();
    printf("  Number of processors: %d\n", thread_number);
    
    testMyqueue(thread_number);
    
    return 0;
}
