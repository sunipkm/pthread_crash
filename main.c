#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>

static pthread_key_t  thread_SIG_handler_key;

static void process_SIG_handler(int signum, siginfo_t *info, void *context)
{
    void (*func)(int, siginfo_t *, void *);

    *((void **)&func) = pthread_getspecific(thread_SIG_handler_key);
    if (func)
        func(signum, info, context);
}

static int set_thread_SIG_handler(void (*func)(int, siginfo_t *, void *), int SIG)
{
    sigset_t block, old;
    int result;

    sigemptyset(&block);
    sigaddset(&block, SIG); /* Use signal number instead of SIG! */
    result = pthread_sigmask(SIG_BLOCK, &block, &old);
    if (result)
        return errno = result;

    result = pthread_setspecific(thread_SIG_handler_key, (void *)func);
    if (result) {
        pthread_sigmask(SIG_SETMASK, &old, NULL);
        return errno = result;
    }

    result = pthread_sigmask(SIG_SETMASK, &old, NULL);
    if (result)
        return errno = result;

    return 0;
}

int install_SIG_handlers(const int signum)
{
    struct sigaction act;
    int result;

    result = pthread_key_create(&thread_SIG_handler_key, NULL);
    if (result)
        return errno = result;

    sigemptyset(&act.sa_mask);
    act.sa_sigaction = process_SIG_handler;
    act.sa_flags = SA_SIGINFO;
    if (sigaction(signum, &act, NULL))
        return errno;

    return 0;
}

void thread_sighandler(int sig, siginfo_t *info, void *ctx)
{
    pthread_t thr = pthread_self();
    printf("%d received on %p, exiting thread\n", sig, thr);
    fflush(stdout);
    pthread_exit(NULL);
}

void *thr_fcn(void *_arg)
{
    set_thread_SIG_handler(&thread_sighandler, SIGBUS);
    int *a = _arg;
    printf("a[0] = 0x%x\n", a[0]);
    fflush(stdout);
    for (int i = 1; i < 1000; i++)
    {
        printf("a[%d] = 0x%x\n", i, a[i]);
        fflush(stdout);
    }
    pthread_exit((void *)1);
}

int main()
{
    install_SIG_handlers(SIGBUS);
    pthread_t t;
    int a[1] = {0xdeadbeef}, rc;
    void *ret;
spawn:
    pthread_create(&t, NULL, &thr_fcn, a);
    printf("Spawned: %p\n", t);
    fflush(stdout);
    struct timespec no_time = {0, 0};
    printf("Join -> Ret = %d, ", pthread_join(t, &ret));
    printf("retval = %p\n", ret);
    return 0;
}