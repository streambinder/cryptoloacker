#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__
#include <pthread.h>
#elif _WIN32
#include <windows.h>
#endif

#include "threadpool.h"
#include "opt.h"

static char aux_log[100];

#ifdef __linux__
static void* thread_boot(void *args) {
#elif _WIN32
static DWORD WINAPI thread_boot(void *args) {
#endif
        threadpool_t *threadpool = args;
        for(;; ) {
#ifdef __linux__
                pthread_mutex_lock(&threadpool->mutex);
#elif _WIN32
                AcquireSRWLockExclusive(&threadpool->mutex);
#endif
                while(threadpool->size_jobs == 0 && !threadpool->bye) {
#ifdef __linux__
                        pthread_cond_wait(&threadpool->condition_variable, &threadpool->mutex);
#elif _WIN32
                        SleepConditionVariableSRW(&threadpool->condition_variable, &threadpool->mutex, INFINITE, 0);
#endif
                }

                if (threadpool->bye) {
#ifdef __linux__
                        pthread_mutex_unlock(&threadpool->mutex);
#elif _WIN32
                        ReleaseSRWLockExclusive(&threadpool->mutex);
#endif
                        break;
                }

                job_t *job = threadpool->top;
                threadpool->top = threadpool->top->next;
                if (!threadpool->top) {
                        threadpool->bottom = NULL;
                }
                threadpool->size_jobs--;
#ifdef __linux__
                pthread_mutex_unlock(&threadpool->mutex);
#elif _WIN32
                ReleaseSRWLockExclusive(&threadpool->mutex);
#endif

                (*job->function)(job->params);
                free(job);
        }
}

int threadpool_init(threadpool_t *threadpool, int max) {
#ifdef __linux__
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&threadpool->mutex, &attr);
#elif _WIN32
        InitializeSRWLock(&threadpool->mutex);
#endif
#ifdef __linux__
        pthread_cond_init(&threadpool->condition_variable, NULL);
#elif _WIN32
        InitializeConditionVariable(&threadpool->condition_variable);
#endif
        threadpool->size_jobs = 0;
        threadpool->size_threads = max;
        threadpool->top = NULL;
        threadpool->bottom = NULL;
        threadpool->threads = malloc(max * sizeof(thread_t));

#ifdef __linux__
        pthread_mutex_lock(&threadpool->mutex);
#elif _WIN32
        AcquireSRWLockExclusive(&threadpool->mutex);
#endif
        for (int i = 0; i < max; i++) {
#ifdef __linux__
                int status = pthread_create(&threadpool->threads[i], NULL, &thread_boot, threadpool);
                if (status != 0) {
#elif _WIN32
                DWORD t_id;
                threadpool->threads[i] = CreateThread(NULL, 0, &thread_boot, threadpool, 0, &t_id);
                if (threadpool->threads[i] == INVALID_HANDLE_VALUE) {
#endif
                        sprintf(aux_log, "Unable to initialize thread %d.", i);
                        std_err(aux_log);
                        return -1;
                }
        }

#ifdef __linux__
        pthread_mutex_unlock(&threadpool->mutex);
#elif _WIN32
        ReleaseSRWLockExclusive(&threadpool->mutex);
#endif

        return 0;
}

int threadpool_add_job(threadpool_t *threadpool, void (*function)(void *), void *params) {
        job_t *job = malloc(sizeof(job_t));
        job->next = NULL;
        job->function = function;
        job->params = params;

#ifdef __linux__
        pthread_mutex_lock(&threadpool->mutex);
#elif _WIN32
        AcquireSRWLockExclusive(&threadpool->mutex);
#endif
        if (threadpool->bye) {
#ifdef __linux__
                pthread_mutex_unlock(&threadpool->mutex);
#elif _WIN32
                ReleaseSRWLockExclusive(&threadpool->mutex);
#endif
                return -1;
        }
        if (threadpool->size_jobs == 0) {
                threadpool->top = job;
        } else {
                threadpool->bottom->next = job;
        }
        threadpool->bottom = job;
        threadpool->size_jobs++;

#ifdef __linux__
        pthread_cond_broadcast(&threadpool->condition_variable);
        pthread_mutex_unlock(&threadpool->mutex);
#elif _WIN32
        WakeAllConditionVariable(&threadpool->condition_variable);
        ReleaseSRWLockExclusive(&threadpool->mutex);
#endif

        return 0;
}

int threadpool_bye(threadpool_t *threadpool) {
#ifdef __linux__
        pthread_mutex_lock(&threadpool->mutex);
#elif _WIN32
        AcquireSRWLockExclusive(&threadpool->mutex);
#endif
        threadpool->bye = 1;
#ifdef __linux__
        pthread_cond_broadcast(&threadpool->condition_variable);
        pthread_mutex_unlock(&threadpool->mutex);
#elif _WIN32
        WakeAllConditionVariable(&threadpool->condition_variable);
        ReleaseSRWLockExclusive(&threadpool->mutex);
#endif

        for (int i = 0; i < threadpool->size_threads; i++) {
#ifdef __linux__
                pthread_join(threadpool->threads[i], NULL);
#elif _WIN32
                WaitForSingleObject(threadpool->threads[i], INFINITE);
#endif
        }

        job_t *job;
        while ((job = threadpool->top)) {
                threadpool->top = threadpool->top->next;
                free(job);
        }

        return 0;
}
