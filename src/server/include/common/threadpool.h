#ifndef THREADPOOL_H
#define THREADPOOL_H

typedef struct sem {
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        int v;
} sem;

typedef struct job {
        struct job* prev;
        void (*function)(void* arg);
        void* arg;
} job;

typedef struct jobqueue {
        pthread_mutex_t rwmutex;
        job *front;
        job *rear;
        sem *has_jobs;
        int len;
} jobqueue;

typedef struct thread {
        int id;
        pthread_t pthread;
        struct threadpool* thpool;
} thread;

typedef struct threadpool {
        thread** threads;
        volatile int num_threads_alive;
        volatile int num_threads_working;
        pthread_mutex_t thcount_lock;
        pthread_cond_t threads_all_idle;
        jobqueue jobqueue;
} threadpool;

threadpool* thpool_init(int num_threads);
int thpool_add_work(threadpool*, void (*function_p)(void*), void* arg_p);
void thpool_wait(threadpool*);
void thpool_destroy(threadpool*);
int thpool_num_threads_working(threadpool*);
int thread_init(threadpool* thpool, struct thread** thread_p, int id);
void* thread_do(struct thread* thread_p);
void thread_hold(int sig_id);
void thread_destroy(struct thread* thread_p);
int jobqueue_init(jobqueue* jobqueue_p);
void jobqueue_clear(jobqueue* jobqueue_p);
void jobqueue_push(jobqueue* jobqueue_p, struct job* newjob_p);
struct job* jobqueue_pull(jobqueue* jobqueue_p);
void jobqueue_destroy(jobqueue* jobqueue_p);
void sem_init(struct sem *sem_p, int value);
void sem_reset(struct sem *sem_p);
void sem_post(struct sem *sem_p);
void sem_post_all(struct sem *sem_p);
void sem_wait(struct sem *sem_p);

#endif // THREADPOOL_H
