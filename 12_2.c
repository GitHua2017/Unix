#include "apue.h"
#include<pthread.h>
#include<time.h>
#include<sys/time.h>

int makethread(void *(*fn) (void *), void *arg)
{
	int err;
	pthread_t tid;
	pthread_attr_t attr;

	if ((err = pthread_mutexattr_init(&attr)) != 0)
		return err;

	if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) == 0)
		err = pthread_create(&tid, &attr, fn, arg);

	pthread_attr_destroy(&attr);
	return err;
}

struct to_info
{
	void (*to_fn)(void*);	// function
	void *to_arg;			// argement
	struct timespec to_wait;// time to wait
};

#define SECTONSEC 1000000000
#define USECTONXEC 1000

void timeout_helper(void *arg)
{
	struct to_info *tip;

	tip = (struct to_info *)arg;
	nanosleep(&tip->to_wait, NULL);
	(*tip->to_fn)(tip->to_arg);
	return(0);
}

void timeout(const struct timespec *when, void (*func)(void *), void *arg)
{
	struct timespec now;
	struct timeval tv;
	struct to_info *tip;
	int err;

	gettimeofday(&tv, NULL);
	printf("gettimeofday: %d\n", tv.tv_sec);
	now.tv_sec =  tv.tv_sec;
	now.tv_nsec = tv.tv_usec * USECTONXEC;
	if ((when->tv_sec > now.tv_sec) || (when->tv_sec > now.tv_sec && when->tv_nsec > now.tv_nsec)) {
		tip = malloc(sizeof(struct to_info));
		if (tip != NULL) {
			tip->to_fn = func;
			tip->to_arg = arg;
			tip->to_wait.tv_sec = when->tv_sec - now.tv_sec;
			if (when->tv_nsec >= now.tv_nsec) {
				tip->to_wait.tv_nsec = when->tv_nsec - now.tv_nsec;
			} else {
				tip->to_wait.tv_sec--;
				tip->to_wait.tv_nsec = SECTONSEC - now.tv_nsec + when->tv_nsec;
			}
			err = makethread(timeout_helper, (void *)tip);
			if (err == 0)
				return;
		}
	}
	(*func)(arg);
	
}

pthread_mutexattr_t attr;
pthread_mutex_t mutex;

void retry(void *arg)
{
	pthread_mutex_lock(&mutex);
	// perform retry steps.
	pthread_mutex_unlock(&mutex);
}

int main(void)
{
	int err, condition, arg;
	struct timespec when;
	//*
	condition = 1;
	printf("condition: %d\n", condition);
	if ((err = pthread_mutexattr_init(&attr)) != 0)
		err_exit(err, "pthread_mutexattr_init failed!");

	
	if ((err = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE)) != 0)
		err_exit(err, "set recrusive type failed!");

	if ((err = pthread_mutex_init(&mutex, &attr)) != 0)
		err_exit(err, "pthread_mutex_init failed!");
	
	pthread_mutex_lock(&mutex);

	printf("condition1: %d\n", condition);
	if (condition) {
		// calculate target time "when"
		printf("condition2: %d\n", condition);
		timeout(&when, retry, (void *)arg);
	}
	pthread_mutex_unlock(&mutex);

	exit(0);

}