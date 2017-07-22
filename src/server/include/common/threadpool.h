#ifndef THREADPOOL_H
#define THREADPOOL_H

typedef struct thpool_* threadpool;

typedef struct bsem {
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        int v;
} bsem;

typedef struct job {
        struct job* prev;
        void (*function)(void* arg);
        void* arg;
} job;

typedef struct jobqueue {
        pthread_mutex_t rwmutex;
        job *front;
        job *rear;
        bsem *has_jobs;
        int len;
} jobqueue;

typedef struct thread {
        int id;
        pthread_t pthread;
        struct thpool_* thpool_p;
} thread;

typedef struct thpool_ {
        thread** threads;
        volatile int num_threads_alive;
        volatile int num_threads_working;
        pthread_mutex_t thcount_lock;
        pthread_cond_t threads_all_idle;
        jobqueue jobqueue;
} thpool_;

threadpool thpool_init(int num_threads);
int thpool_add_work(threadpool, void (*function_p)(void*), void* arg_p);
void thpool_wait(threadpool);
void thpool_pause(threadpool);
void thpool_resume(threadpool);
void thpool_destroy(threadpool);
int thpool_num_threads_working(threadpool);
static int thread_init(thpool_* thpool_p, struct thread** thread_p, int id);
static void* thread_do(struct thread* thread_p);
static void thread_hold(int sig_id);
static void thread_destroy(struct thread* thread_p);
static int jobqueue_init(jobqueue* jobqueue_p);
static void jobqueue_clear(jobqueue* jobqueue_p);
static void jobqueue_push(jobqueue* jobqueue_p, struct job* newjob_p);
static struct job* jobqueue_pull(jobqueue* jobqueue_p);
static void jobqueue_destroy(jobqueue* jobqueue_p);
static void bsem_init(struct bsem *bsem_p, int value);
static void bsem_reset(struct bsem *bsem_p);
static void bsem_post(struct bsem *bsem_p);
static void bsem_post_all(struct bsem *bsem_p);
static void bsem_wait(struct bsem *bsem_p);

#endif // THREADPOOL_H
