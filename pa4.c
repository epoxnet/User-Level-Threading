#include "thread.h"
#include <ucontext.h>
#include <stdio.h>
#include <ucontext.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>

thread_t *current;
thread_t *front = NULL;
thread_t *last;
int proc_count = 1, first_time = 0;

static void start_timer(int time_first, int time_repeat)
{
    struct itimerval interval;
    interval.it_interval.tv_sec = time_repeat;//it_interval is a struct timeval field containing the value to which the timer will be reset after it expires. If this is 0, the timer will be disabled after it expires. If this is nonzero, the timer is set to expire repeatedly after this interval.
    interval.it_interval.tv_usec = 0;
    interval.it_value.tv_sec = time_first;//it_value is a struct timeval field that contains the time until the timer next expires and a signal is sent. If this is 0, the timer is disabled.
    interval.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &interval, 0);// The timer code is ITIMER_REAL, the process is sent a SIGALRM signal after the specified wall-clock time has elapsed.
}

void insert_into_queue(thread_t *thread_node_ctrl) // Thread Control Block
{
    
    // if there is nothing in the list make thread_node_ctrl the front
    if (front == NULL)
    {
        // front and last are equal to thread_node_ctrl because its the only item in list
        front = thread_node_ctrl;
        last = thread_node_ctrl;
        
        // thread_node_ctrl's next and prev point to thread_node_ctrl because they are the only item in list
        thread_node_ctrl->next = thread_node_ctrl;
        thread_node_ctrl->prev = thread_node_ctrl;
        
        return;
    }
    
    // reorder list
    
    //insert at end of list
    last->next = thread_node_ctrl;
    thread_node_ctrl->prev = last;
    thread_node_ctrl->next = front;
    front->prev = thread_node_ctrl;
    last = thread_node_ctrl;
}

static void timer_handler() //
{
    if (current->constant == 1)
    { //checks flag to see if safe to swap
        
        thread_t *prev = current;
        current = current->next;
        
        start_timer(3, 0);
        
        if (current != prev) //this checks that there is more than one thread first
        {
            printf("swapping from thread %d to thread %d\n", prev->count, current->count);
            swapcontext(&prev->context, &current->context);
        }
    }
    else
    {
        //process is not consistent lets check back in 3 seconds
        start_timer(3, 0);
    }
}

/************************************************************************************/
// BEGIN thread_create() support functions
/************************************************************************************/

void *init_sig(struct sigaction action)//The sigaction() function allows the calling process to examine and/or specify the action to be associated with a specific signal. The argument sig specifies the signal; acceptable values are defined in <signal.h>.

//The structure sigaction, used to describe an action to be taken, is defined in the <signal.h> header to include at least the following members:

{
    action.sa_flags = SA_SIGINFO | SA_RESTART;// sa_flags:Special flags to affect behavior of signal.
    action.sa_sigaction = timer_handler; //Additional set of signals to be blocked during execution of signal-catching function.
    sigemptyset(&action.sa_mask);//an emptyset
    sigaction(SIGALRM, &action, 0); //catching the SIGALRM
}

void *init_queue(thread_t *thread_main)
{
    thread_main->prev = NULL;
    thread_main->next = NULL;
    thread_main->constant = 1;
    thread_main->count = 0;
    
    current = thread_main;
    front = thread_main;
    last = thread_main;
    first_time = 1;
}

void *manage_context(thread_t *thread,  void (*func)(void*), void * arg)
{
    char* funcl_stack = (char*)malloc(16123);
    thread->context.uc_stack.ss_sp = funcl_stack;
    thread->context.uc_stack.ss_size = 16123;
    thread->context.uc_link = 0;
    
    getcontext(&thread->context);
    makecontext(&thread->context, (void (*)(void))func, 1, arg);
}

void *init_thread(thread_t *thread)
{
    thread->prev = NULL;
    thread->next = NULL;
    thread->constant = 1;
    thread->count = proc_count;
}
/************************************************************************************/
// END thread_create() support functions
/************************************************************************************/

void* thread_create(thread_t* thread, void (*func)(void*), void * arg) //gets called at least once. maybe more than once. every thread counts increment by 1 .
//Thread_Create {1-GetContext 2-MAkeContext}
{
    
    //initialize first main thread
    if (first_time == 0)
    {
        // init sig stuff
        struct sigaction action;
        init_sig(action);
        
        // init queue
        thread_t *thread_main = (thread_t *)malloc(sizeof(thread_t));
        init_queue(thread_main);
    }
    
    char *funcl_stack = (char *)malloc(16123); //Function Location Size - Malloc()
    
    // manage context
    manage_context(thread, func, arg);
    init_thread(thread);
    
    proc_count++;
    insert_into_queue(thread);
    start_timer(0, 0);
    timer_handler();
    return thread;
}

void *thread_yield() // allows user code to cause a context switch
//Thread_Yield {SwapContext}
{
    printf("Yielding the current thread with thread ID %d. [<<<Switching threads>>>]\n", current->count);
    start_timer(0, 0);
    timer_handler();
}

/************************************************************************************/
// BEGIN thread_exit() support functions
/************************************************************************************/
void cleanup_queue()
{
    current->constant = 0;
    thread_t *prev = (thread_t *)current->prev;
    thread_t *next = (thread_t *)current->next;
    prev->next = current->next;
    next->prev = current->prev;
    free(current->context.uc_stack.ss_sp);
    if (current == front)
    {
        front = current->next;
    }
    if (current == last)
    {
        last = current->prev;
    }
    if (current == current->next)
    {
        exit(0);
    }
    current = current->next;
}

/************************************************************************************/
// END thread_exit() support functions
/************************************************************************************/

void thread_exit() // exiting thread decrement the count by 1.
{
    printf("Exiting the current thread with thread ID %d. [<<<Exiting thread>>>]\n", current->count);
    
    cleanup_queue();
    
    if (setcontext(&current->context) != 0)
    {
        fprintf(stderr, "\x1b[3;33mError in thread_exit line %d file %s\x1b[0m\n", __LINE__, __FILE__);
    }
}

void * thread_yield(){
  printf("current thread: thread %d is yielding, moving on to next thread..\n", current->count);
  start_timer(0,0);
  timerHandler();
}

void thread_exit() {
    printf("current thread: thread %d\n is exiting", current->count);
    current->isConsistent = 0;
    thread_t* prev = (thread_t*) current->prev;
    thread_t* next = (thread_t*) current->next;
    prev->next = current->next;
    next->prev = current->prev;
    free(current->context.uc_stack.ss_sp);
    if (current == head) {
        head = current->next;
    }
    if (current == tail) {
        tail = current->prev;
    }
    if (current == current->next) {
        exit(0);
    }
    current = current->next;
    if (setcontext(&current->context) != 0) {
        fprintf(stderr, "\x1b[3;33mError in thread_exit line %d file %s\x1b[0m\n", __LINE__, __FILE__);
    }
}
