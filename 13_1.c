#include "apue.h"
#include <syslog.h>
#include <fcntl.h>
#include <sys/resource.h>

#include <pthread.h>

#include <unistd.h>
#include <stdlib.h>



void daemonize(const char *cmd)
{
	int i, fd0, fd1, fd2;
	pid_t pid;
	struct rlimit rl;
	struct sigaction sa;

	// clear file creation mask
	umask(0);

	// get maximum number of file descriptors.
	if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
		err_quit("%s: can't get file limit", cmd);

	// become a session leader to lose controlling TTY.
	if ((pid = fork()) < 0)
		err_quit("%s: can't fork", cmd);
	else if (pid != 0)
		exit(0);
	setsid();

	// ensure future opens won't allocate controlling TTYS.
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags =0;

	if (sigaction(SIGHUP, &sa, NULL) < 0)
		err_quit("%s: can't ignore SIGHUP");
	else if (pid != 0)
		exit(0);
	
	// Change the current working directory to the root so we won't prevent file systems from being unmounted.

	if (chdir("/") < 0)
		err_quit("%s: can't change directory to /");

	//close all open file descriptors

	if (rl.rlim_max == RLIM_INFINITY)
		rl.rlim_max = 1024;

	for(i = 0; i < rl.rlim_max; i++)
		close(i);

	// attach file descriptors 0, 1, and 2 to /dev/null.
	fd0 = open("/dev/null", O_RDWR);
	fd1 = dup(0);
	fd2 = dup(0);

	// initialize the log file
	openlog(cmd, LOG_CONS, LOG_DAEMON);
	if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
		syslog(LOG_ERR, "unexpected file descriptors %d, %d, %d.", fd0, fd1, fd2);
		exit(1);
	}
}


// #define lockfile(fd) write_lock((fd), 0, SEEK_SET, 0)

int lockfile(int fd)
{
	struct flock fl;
	fl.l_type = F_WRLCK;
	fl.l_start = 0;
	fl.l_whence = SEEK_SET;
	fl.l_len = 0;
	return(fcntl(fd, F_SETLK, &fl));
}

#define LOCKFILE "/var/run/daemon.pid"
#define LOCKMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)
int already_running(void)
{
	int fd;
	char buf[16];

	fd = open(LOCKFILE, O_RDWR | O_CREAT, LOCKMODE);
	if (fd < 0){
		syslog(LOG_ERR, "can't open %s: %s", LOCKFILE, strerror(errno));
		exit(1);
	}

	if (lockfile(fd) < 0)
	{
		if (errno == EACCES || errno ==  EAGAIN){
			close(fd);
			return (1);
		}
		syslog(LOG_ERR, "can't lock %s: %s", LOCKFILE, strerror(errno));
		exit(1);
	}

	ftruncate(fd, 0);
	sprintf(buf, "%ld", (long)getpid());
	write(fd, buf, strlen(buf)+1);
	return(0);
}


sigset_t mask;

void reread(void)
{
	/*....*/
}

void *thr_fn(void *arg)
{
	int err, signo;

	for(;;)
	{
		err = sigwait(&mask, &signo);
		if (err != 0) {
			syslog(LOG_ERR, "sigwait failed!");
			exit(1);
		}
	}

	switch(signo){
		case SIGHUP:
			syslog(LOG_INFO, "Re-reading configuration file");
			reread();
			break;

		case SIGTERM:
			syslog(LOG_INFO, "got SIGTEMR; exiting");
			exit(0);

		default:
			syslog(LOG_INFO, "unexpected signal %d\n", signo);
	}

	return(0);
}


int main(int argc, char *argv[])
{
	int err;
	pthread_t tid;
	char *cmd;
	struct sigaction sa;

	if ((cmd = strrchr(argv[0], '/')) == NULL)
		cmd = argv[0];
	else
		cmd++;

	// become a daemon
	daemonize(cmd);

	// make sure only one copy of the deamon is running.

	if (already_running()){
		syslog(LOG_INFO, "deamon already_running");
		exit(1);
	}

	//restore SIGHUP default and block all signals.
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGHUP, &sa, NULL) < 0)
		err_quit("%s: can't restore SIGHUP default");

	sigfillset(&mask);
	if ((err = pthread_sigmask(SIG_BLOCK, &mask, NULL)) != 0)
		err_exit(err, "SIG_BLOCK error");

	// create a thread to handle SIGHUP and SIGTERM
	err = pthread_create(&tid, NULL, thr_fn, 0);
	if (err != 0)
		err_exit(err, "can't create thread");

	// proceed  with the rest of the daemon

	exit(0);
}