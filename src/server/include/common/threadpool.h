#ifndef THREADPOOL_H
#define THREADPOOL_H

#ifdef __linux__
typedef pthread_t thread_t;
#elif _WIN32
typedef HANDLE thread_t;
#endif

#ifdef __linux__
typedef pthread_mutex_t mutex_t;
#elif _WIN32
typedef SRWLOCK mutex_t;
#endif

#ifdef __linux__
typedef pthread_cond_t condition_variable_t;
#elif _WIN32
typedef CONDITION_VARIABLE condition_variable_t;
#endif

typedef struct job_t {
        struct job_t *next;
        void (*function)(void *);
        void *params;
} job_t;

typedef struct {
        int size_jobs;
        int size_threads;
        thread_t *threads;
        job_t *top;
        job_t *bottom;
        mutex_t mutex;
        condition_variable_t condition_variable;
        int bye;
} threadpool_t;

int threadpool_init(threadpool_t *threadpool, int max);
int threadpool_add_job(threadpool_t *threadpool, void (*function)(void *), void *params);
int threadpool_bye(threadpool_t *threadpool);

#endif // THREADPOOL_H
