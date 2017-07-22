#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <sys/prctl.h>

#include "opt.h"
#include "threadpool.h"

char aux_log[100];
volatile int threads_waiting;
volatile int threads_keepalive;

struct threadpool* thpool_init(int num_threads) {
        threads_waiting = 0;
        threads_keepalive = 1;
        if (num_threads < 0) {
                num_threads = 0;
        }
        threadpool* thpool;
        thpool = (struct threadpool*)malloc(sizeof(struct threadpool));
        if (thpool == NULL) {
                std_err("Unable to allocate memory for threadpool.");
                return NULL;
        }
        thpool->num_threads_alive = 0;
        thpool->num_threads_working = 0;

        if (jobqueue_init(&thpool->jobqueue) == -1) {
                std_err("Unable to allocate memory for job queue.");
                free(thpool);
                return NULL;
        }

        thpool->threads = (struct thread**)malloc(num_threads * sizeof(struct thread *));
        if (thpool->threads == NULL) {
                std_err("Unable to allocate memory for threads.");
                jobqueue_destroy(&thpool->jobqueue);
                free(thpool);
                return NULL;
        }

        pthread_mutex_init(&(thpool->thcount_lock), NULL);
        pthread_cond_init(&thpool->threads_all_idle, NULL);

        int i;
        for (i = 0; i<num_threads; i++) {
                thread_init(thpool, &thpool->threads[i], i);
        }

        while (thpool->num_threads_alive != num_threads) ;

        return thpool;
}

int thpool_add_work(threadpool* thpool, void (*function_p)(void*), void* arg_p) {
        job* newjob;

        newjob = (struct job*)malloc(sizeof(struct job));
        if (newjob == NULL) {
                std_err("Unable to allocate memory for new job.");
                return -1;
        }

        newjob->function = function_p;
        newjob->arg = arg_p;
        jobqueue_push(&thpool->jobqueue, newjob);
        return 0;
}

void thpool_wait(threadpool* thpool) {
        pthread_mutex_lock(&thpool->thcount_lock);
        while (thpool->jobqueue.len || thpool->num_threads_working) {
                pthread_cond_wait(&thpool->threads_all_idle, &thpool->thcount_lock);
        }
        pthread_mutex_unlock(&thpool->thcount_lock);
}

void thpool_destroy(threadpool* thpool) {
        if (thpool == NULL) {
                return;
        }

        volatile int threads_total = thpool->num_threads_alive;
        threads_keepalive = 0;
        double TIMEOUT = 1.0;
        time_t start, end;
        double tpassed = 0.0;
        time (&start);
        while (tpassed < TIMEOUT && thpool->num_threads_alive) {
                sem_post_all(thpool->jobqueue.has_jobs);
                time (&end);
                tpassed = difftime(end, start);
        }
        while (thpool->num_threads_alive) {
                sem_post_all(thpool->jobqueue.has_jobs);
                sleep(1);
        }
        jobqueue_destroy(&thpool->jobqueue);
        int i;
        for (i=0; i < threads_total; i++) {
                thread_destroy(thpool->threads[i]);
        }
        free(thpool->threads);
        free(thpool);
}

int thpool_num_threads_working(threadpool* thpool) {
        return thpool->num_threads_working;
}

int thread_init (threadpool* thpool, struct thread** thread_p, int id) {
        *thread_p = (struct thread*)malloc(sizeof(struct thread));
        if (thread_p == NULL) {
                std_err("Unable to allocate memory for thread.");
                return -1;
        }

        (*thread_p)->thpool = thpool;
        (*thread_p)->id = id;

        pthread_create(&(*thread_p)->pthread, NULL, (void *)thread_do, (*thread_p));
        pthread_detach((*thread_p)->pthread);
        return 0;
}

void thread_hold(int sig_id) {
        (void) sig_id;
        threads_waiting = 1;
        while (threads_waiting) {
                sleep(1);
        }
}

void* thread_do(struct thread* thread_p) {
        char thread_name[128] = {0};
        sprintf(thread_name, "thread-pool-%d", thread_p->id);

#if defined(__linux__)
        prctl(PR_SET_NAME, thread_name);
#else
        printf("thread_do(): pthread_setname_np is not supported on this system.\n");
#endif

        threadpool* thpool = thread_p->thpool;
        struct sigaction act;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        act.sa_handler = thread_hold;
        if (sigaction(SIGUSR1, &act, NULL) == -1) {
                std_err("Cannot handle SIGUSR1");
        }
        pthread_mutex_lock(&thpool->thcount_lock);
        thpool->num_threads_alive += 1;
        pthread_mutex_unlock(&thpool->thcount_lock);

        while(threads_keepalive) {
                sem_wait(thpool->jobqueue.has_jobs);
                if (threads_keepalive) {
                        pthread_mutex_lock(&thpool->thcount_lock);
                        thpool->num_threads_working++;
                        pthread_mutex_unlock(&thpool->thcount_lock);
                        void (*func_buff)(void*);
                        void* arg_buff;
                        job* job_p = jobqueue_pull(&thpool->jobqueue);
                        if (job_p) {
                                func_buff = job_p->function;
                                arg_buff = job_p->arg;
                                func_buff(arg_buff);
                                free(job_p);
                        }
                        pthread_mutex_lock(&thpool->thcount_lock);
                        thpool->num_threads_working--;
                        if (!thpool->num_threads_working) {
                                pthread_cond_signal(&thpool->threads_all_idle);
                        }
                        pthread_mutex_unlock(&thpool->thcount_lock);
                }
        }
        pthread_mutex_lock(&thpool->thcount_lock);
        thpool->num_threads_alive--;
        pthread_mutex_unlock(&thpool->thcount_lock);

        return NULL;
}

void thread_destroy (thread* thread_p) {
        free(thread_p);
}

int jobqueue_init(jobqueue* jobqueue_p) {
        jobqueue_p->len = 0;
        jobqueue_p->front = NULL;
        jobqueue_p->rear  = NULL;

        jobqueue_p->has_jobs = (struct sem*)malloc(sizeof(struct sem));
        if (jobqueue_p->has_jobs == NULL) {
                return -1;
        }

        pthread_mutex_init(&(jobqueue_p->rwmutex), NULL);
        sem_init(jobqueue_p->has_jobs, 0);

        return 0;
}

void jobqueue_clear(jobqueue* jobqueue_p) {
        while(jobqueue_p->len) {
                free(jobqueue_pull(jobqueue_p));
        }
        jobqueue_p->front = NULL;
        jobqueue_p->rear  = NULL;
        sem_reset(jobqueue_p->has_jobs);
        jobqueue_p->len = 0;
}

void jobqueue_push(jobqueue* jobqueue_p, struct job* newjob) {
        pthread_mutex_lock(&jobqueue_p->rwmutex);
        newjob->prev = NULL;

        switch(jobqueue_p->len) {

        case 0:
                jobqueue_p->front = newjob;
                jobqueue_p->rear = newjob;
                break;
        default:
                jobqueue_p->rear->prev = newjob;
                jobqueue_p->rear = newjob;

        }
        jobqueue_p->len++;

        sem_post(jobqueue_p->has_jobs);
        pthread_mutex_unlock(&jobqueue_p->rwmutex);
}

struct job* jobqueue_pull(jobqueue* jobqueue_p) {
        pthread_mutex_lock(&jobqueue_p->rwmutex);
        job* job_p = jobqueue_p->front;

        switch(jobqueue_p->len) {

        case 0:
                break;
        case 1:
                jobqueue_p->front = NULL;
                jobqueue_p->rear = NULL;
                jobqueue_p->len = 0;
                break;
        default:
                jobqueue_p->front = job_p->prev;
                jobqueue_p->len--;
                sem_post(jobqueue_p->has_jobs);
        }

        pthread_mutex_unlock(&jobqueue_p->rwmutex);
        return job_p;
}

void jobqueue_destroy(jobqueue* jobqueue_p) {
        jobqueue_clear(jobqueue_p);
        free(jobqueue_p->has_jobs);
}

void sem_init(sem *sem_p, int value) {
        if (value < 0 || value > 1) {
                std_err("Binary semaphore can be only 0 or 1.");
                exit(EXIT_FAILURE);
        }
        pthread_mutex_init(&(sem_p->mutex), NULL);
        pthread_cond_init(&(sem_p->cond), NULL);
        sem_p->v = value;
}

void sem_reset(sem *sem_p) {
        sem_init(sem_p, 0);
}

void sem_post(sem *sem_p) {
        pthread_mutex_lock(&sem_p->mutex);
        sem_p->v = 1;
        pthread_cond_signal(&sem_p->cond);
        pthread_mutex_unlock(&sem_p->mutex);
}

void sem_post_all(sem *sem_p) {
        pthread_mutex_lock(&sem_p->mutex);
        sem_p->v = 1;
        pthread_cond_broadcast(&sem_p->cond);
        pthread_mutex_unlock(&sem_p->mutex);
}

void sem_wait(sem* sem_p) {
        pthread_mutex_lock(&sem_p->mutex);
        while (sem_p->v != 1) {
                pthread_cond_wait(&sem_p->cond, &sem_p->mutex);
        }
        sem_p->v = 0;
        pthread_mutex_unlock(&sem_p->mutex);
}
