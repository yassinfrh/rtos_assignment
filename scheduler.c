// compile with: g++ -lpthread <sourcename> -o <executablename>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>

// code of periodic tasks
int task1_code();
int task2_code();
int task3_code();

// code of aperiodic task
int task4_code();

// characteristic function of the thread, only for timing and synchronization
// periodic tasks
void *task1(void *);
void *task2(void *);
void *task3(void *);

// aperiodic task
void *task4(void *);

// initialization of mutexes and conditions (only for aperiodic scheduling)
pthread_mutex_t mutex_task_4 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_task_4 = PTHREAD_COND_INITIALIZER;

// IMPORTANT: uncomment the following lines for using priority ceiling mutex
/*
// declaration of mutex
pthread_mutex_t mutex_sem = PTHREAD_MUTEX_INITIALIZER;

// mutex attribute
pthread_mutexattr_t mutexattr_sem;
*/

#define INNERLOOP 1000
#define OUTERLOOP 2000

#define NPERIODICTASKS 3
#define NAPERIODICTASKS 1
#define NTASKS NPERIODICTASKS + NAPERIODICTASKS

long int periods[NTASKS];
struct timespec next_arrival_time[NTASKS];
double WCET[NTASKS];
pthread_attr_t attributes[NTASKS];
pthread_t thread_id[NTASKS];
struct sched_param parameters[NTASKS];

int main()
{
    // set task periods in nanoseconds
    // the first task has period 300 millisecond
    // the second task has period 500 millisecond
    // the third task has period 800 millisecond
    // you can already order them according to their priority;
    // if not, you will need to sort them
    periods[0] = 300000000; // in nanoseconds
    periods[1] = 500000000; // in nanoseconds
    periods[2] = 800000000; // in nanoseconds

    // for aperiodic task we set the period equals to 0
    periods[3] = 0;

    // this is not strictly necessary, but it is convenient to
    // assign a name to the maximum and the minimum priotity in the
    // system. We call them priomin and priomax.

    struct sched_param priomax;
    priomax.sched_priority = sched_get_priority_max(SCHED_FIFO);
    struct sched_param priomin;
    priomin.sched_priority = sched_get_priority_min(SCHED_FIFO);

    // set the maximum priority to the current thread

    if (getuid() == 0)
        pthread_setschedparam(pthread_self(), SCHED_FIFO, &priomax);

    // open the special file
    int fd;
    if ((fd = open("/dev/my", O_RDWR)) == -1)
    {
        perror("Error opening file");
        return -1;
    }

    // IMPORTANT: uncomment the following lines for using priority ceiling mutex
    /*
    // initialize mutex attributes
    pthread_mutexattr_init(&mutexattr_sem);

    // set mutex protocol to priority ceiling
    pthread_mutexattr_setprotocol(&mutexattr_sem, PTHREAD_PRIO_PROTECT);

    // set the priority ceiling of the mutex to the maximum priority
    pthread_mutexattr_setprioceiling(&mutexattr_sem, priomin.sched_priority + NTASKS);

    // initialize the mutex
    pthread_mutex_init(&mutex_sem, &mutexattr_sem);
    */

    // string to be written to the file
    char str[100];

    // execute all tasks in standalone modality in order to measure execution times
    // Use the computed values to update the worst case execution time of each task.
    int i;
    for (i = 0; i < NTASKS; i++)
    {

        // initialize time_1 and time_2 required to read the clock
        struct timespec time_1, time_2;
        clock_gettime(CLOCK_REALTIME, &time_1);

        // we should execute each task more than one for computing the WCET
        // periodic tasks
        if (i == 0)
            task1_code();
        if (i == 1)
            task2_code();
        if (i == 2)
            task3_code();

        // aperiodic task
        if (i == 3)
            task4_code();

        clock_gettime(CLOCK_REALTIME, &time_2);

        // compute the Worst Case Execution Time (in a real case, we should repeat this many times under
        // different conditions, in order to have reliable values
        WCET[i] = 1000000000 * (time_2.tv_sec - time_1.tv_sec) + (time_2.tv_nsec - time_1.tv_nsec);

        // write the WCET in the special file
        sprintf(str, "Worst Case Execution Time %d=%f", i, WCET[i]);
        if (write(fd, str, strlen(str) + 1) != strlen(str) + 1)
        {
            perror("Error writing file");
            return -1;
        }
    }

    // compute U
    double U = WCET[0] / periods[0] + WCET[1] / periods[1] + WCET[2] / periods[2];

    // compute Ulub
    double Ulub = NPERIODICTASKS * (pow(2.0, (1.0 / NPERIODICTASKS)) - 1);

    // check the sufficient conditions: if they are not satisfied, exit
    if (U > Ulub)
    {
        sprintf(str, "U=%lf Ulub=%lf Non schedulable Task Set", U, Ulub);
        if (write(fd, str, strlen(str) + 1) != strlen(str) + 1)
        {
            perror("Error writing file");
            return -1;
        }
        return (-1);
    }
    sprintf(str, "U=%lf Ulub=%lf Schedulable Task Set", U, Ulub);
    if (write(fd, str, strlen(str) + 1) != strlen(str) + 1)
    {
        perror("Error writing file");
        return -1;
    }

    // close the special file
    close(fd);

    sleep(5);

    // set the minimum priority to the current thread: this is now required because
    // we will assign higher priorities to periodic threads to be soon created
    // pthread_setschedparam

    if (getuid() == 0)
        pthread_setschedparam(pthread_self(), SCHED_FIFO, &priomin);

    // set the attributes of each task, including scheduling policy and priority
    for (i = 0; i < NPERIODICTASKS; i++)
    {
        // initialize the attribute structure of task i
        pthread_attr_init(&(attributes[i]));

        // set the attributes to tell the kernel that the priorities and policies are explicitly chosen,
        // not inherited from the main thread (pthread_attr_setinheritsched)
        pthread_attr_setinheritsched(&(attributes[i]), PTHREAD_EXPLICIT_SCHED);

        // set the attributes to set the SCHED_FIFO policy (pthread_attr_setschedpolicy)
        pthread_attr_setschedpolicy(&(attributes[i]), SCHED_FIFO);

        // properly set the parameters to assign the priority inversely proportional
        // to the period
        parameters[i].sched_priority = priomin.sched_priority + NTASKS - i;

        // set the attributes and the parameters of the current thread (pthread_attr_setschedparam)
        pthread_attr_setschedparam(&(attributes[i]), &(parameters[i]));
    }

    // aperiodic tasks
    for (int i = NPERIODICTASKS; i < NTASKS; i++)
    {
        pthread_attr_init(&(attributes[i]));
        pthread_attr_setschedpolicy(&(attributes[i]), SCHED_FIFO);

        // set minimum priority (background scheduling)
        parameters[i].sched_priority = 0;
        pthread_attr_setschedparam(&(attributes[i]), &(parameters[i]));
    }

    // declare the variable to contain the return values of pthread_create
    int iret[NTASKS];

    // declare variables to read the current time
    struct timespec time_1;
    clock_gettime(CLOCK_REALTIME, &time_1);

    // set the next arrival time for each task. This is not the beginning of the first
    // period, but the end of the first period and beginning of the next one.
    for (i = 0; i < NPERIODICTASKS; i++)
    {
        // first we encode the current time in nanoseconds and add the period
        long int next_arrival_nanoseconds = time_1.tv_nsec + periods[i];

        // then we compute the end of the first period and beginning of the next one
        next_arrival_time[i].tv_nsec = next_arrival_nanoseconds % 1000000000;
        next_arrival_time[i].tv_sec = time_1.tv_sec + next_arrival_nanoseconds / 1000000000;
    }

    printf("Starting scheduler\n");
    fflush(stdout);

    // create all threads(pthread_create)
    iret[0] = pthread_create(&(thread_id[0]), &(attributes[0]), task1, NULL);
    iret[1] = pthread_create(&(thread_id[1]), &(attributes[1]), task2, NULL);
    iret[2] = pthread_create(&(thread_id[2]), &(attributes[2]), task3, NULL);
    iret[3] = pthread_create(&(thread_id[3]), &(attributes[3]), task4, NULL);

    // join all threads (pthread_join)
    pthread_join(thread_id[0], NULL);
    pthread_join(thread_id[1], NULL);
    pthread_join(thread_id[2], NULL);

    printf("End of the program\n");
    fflush(stdout);

    // IMPORTANT: uncomment the following lines to use priority ceiling mutex
    /*
    // destroy the mutex
    pthread_mutex_destroy(&mutex_sem);
    */
    exit(0);
}

// application specific task_1 code
int task1_code()
{
    // strings to write
    const char *str_i;
    const char *str_f;

    // strings length
    int len_i, len_f;

    // file descriptor
    int fd;

    // open the special file
    if ((fd = open("/dev/my", O_RDWR)) == -1)
    {
        perror("open failed");
        return -1;
    }
    // write on the special file the id of the current task
    str_i = " 1[ ";
    len_i = strlen(str_i) + 1;
    if (write(fd, str_i, len_i) != len_i)
    {
        perror("write failed");
        return -1;
    }
    // close the special file
    close(fd);

    // this double loop with random computation is only required to waste time
    int i, j;
    double uno;
    for (i = 0; i < OUTERLOOP; i++)
    {
        for (j = 0; j < INNERLOOP; j++)
        {
            uno = rand() * rand() % 10;
        }
    }

    /// open the special file
    if ((fd = open("/dev/my", O_RDWR)) == -1)
    {
        perror("open failed");
        return -1;
    }
    // write on the special file the id of the current task
    str_f = " ]1 ";
    len_f = strlen(str_f) + 1;
    if (write(fd, str_f, len_f) != len_f)
    {
        perror("write failed");
        return -1;
    }
    // close the special file
    close(fd);

    return 0;
}

// thread code for task_1 (used only for temporization)
void *task1(void *ptr)
{
    // set thread affinity, that is the processor on which threads shall run
    cpu_set_t cset;
    CPU_ZERO(&cset);
    CPU_SET(0, &cset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cset);

    // execute the task one hundred times... it should be an infinite loop (too dangerous)
    int i = 0;
    for (i = 0; i < 100; i++)
    {
        // IMPORTANT: uncomment the following lines to use priority ceiling mutex
        /*
        // take the mutex
        pthread_mutex_lock(&mutex_sem);
        */

        // execute application specific code
        if (task1_code())
        {
            printf("task1_code failed\n");
            fflush(stdout);
            // release the mutex
            //pthread_mutex_unlock(&mutex_sem);
            return NULL;
        }

        // IMPORTANT: uncomment the following lines to use priority ceiling mutex
        /*
        // release the mutex
        pthread_mutex_unlock(&mutex_sem);
        */

        // sleep until the end of the current period (which is also the start of the
        // new one
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_arrival_time[0], NULL);

        // the thread is ready and can compute the end of the current period for
        // the next iteration
        long int next_arrival_nanoseconds = next_arrival_time[0].tv_nsec + periods[0];
        next_arrival_time[0].tv_nsec = next_arrival_nanoseconds % 1000000000;
        next_arrival_time[0].tv_sec = next_arrival_time[0].tv_sec + next_arrival_nanoseconds / 1000000000;
    }
}

int task2_code()
{
    // strings to write
    const char *str_i;
    const char *str_f;

    // strings length
    int len_i, len_f;

    // file descriptor
    int fd;

    // open the special file
    if ((fd = open("/dev/my", O_RDWR)) == -1)
    {
        perror("open failed");
        return -1;
    }
    // write on the special file the id of the current task
    str_i = " 2[ ";
    len_i = strlen(str_i) + 1;
    if (write(fd, str_i, len_i) != len_i)
    {
        perror("write failed");
        return -1;
    }
    // close the special file
    close(fd);

    int i, j;
    double uno;
    for (i = 0; i < OUTERLOOP * 2; i++)
    {
        for (j = 0; j < INNERLOOP; j++)
        {
            uno = rand() * rand() % 10;
        }
    }

    /// open the special file
    if ((fd = open("/dev/my", O_RDWR)) == -1)
    {
        perror("open failed");
        return -1;
    }
    // when the random variable uno=0, then aperiodic task 4 must
    // be executed
    if (uno == 0)
    {
        const char *str_4 = ":ex(4)";
        int len_4 = strlen(str_4) + 1;
        if (write(fd, str_4, len_4) != len_4)
        {
            perror("write failed");
            return -1;
        }
        // In theory, we should protect conditions using mutexes. However, in a real-time application, something undesirable may happen.
        // Indeed, when task2 takes the mutex and sends the condition, task4 is executed and is given the mutex by the kernel. Which means
        // that task2 (higher priority) would be blocked waiting for task4 to finish (lower priority). This is of course unacceptable,
        // as it would produced a priority inversion. For this reason, we are not putting mutexes here.

        //      		pthread_mutex_lock(&mutex_task_4);
        pthread_cond_signal(&cond_task_4);
        //      		pthread_mutex_unlock(&mutex_task_4);
    }

    // write on the special file the id of the current task
    str_f = " ]2 ";
    len_f = strlen(str_f) + 1;
    if (write(fd, str_f, len_f) != len_f)
    {
        perror("write failed");
        return -1;
    }
    // close the special file
    close(fd);

    return 0;
}

void *task2(void *ptr)
{
    // set thread affinity, that is the processor on which threads shall run
    cpu_set_t cset;
    CPU_ZERO(&cset);
    CPU_SET(0, &cset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cset);

    int i = 0;
    for (i = 0; i < 60; i++)
    {
        // IMPORTANT: uncomment the following lines to use priority ceiling mutex
        /*
        // take the mutex
        pthread_mutex_lock(&mutex_sem);
        */

        // see task 1
        if (task2_code())
        {
            printf("task2_code failed\n");
            fflush(stdout);
            // release the mutex
            //pthread_mutex_unlock(&mutex_sem);
            return NULL;
        }

        // IMPORTANT: uncomment the following lines to use priority ceiling mutex
        /*
        // release the mutex
        pthread_mutex_unlock(&mutex_sem);
        */

        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_arrival_time[1], NULL);
        long int next_arrival_nanoseconds = next_arrival_time[1].tv_nsec + periods[1];
        next_arrival_time[1].tv_nsec = next_arrival_nanoseconds % 1000000000;
        next_arrival_time[1].tv_sec = next_arrival_time[1].tv_sec + next_arrival_nanoseconds / 1000000000;
    }
}

int task3_code()
{
    // strings to write
    const char *str_i;
    const char *str_f;

    // strings length
    int len_i, len_f;

    // file descriptor
    int fd;

    // open the special file
    if ((fd = open("/dev/my", O_RDWR)) == -1)
    {
        perror("open failed");
        return -1;
    }
    // write on the special file the id of the current task
    str_i = " 3[ ";
    len_i = strlen(str_i) + 1;
    if (write(fd, str_i, len_i) != len_i)
    {
        perror("write failed");
        return -1;
    }
    // close the special file
    close(fd);

    int i, j;
    double uno;
    for (i = 0; i < OUTERLOOP * 4; i++)
    {
        for (j = 0; j < INNERLOOP; j++)
        {
            uno = rand() * rand() % 10;
        }
    }

    // open the special file
    if ((fd = open("/dev/my", O_RDWR)) == -1)
    {
        perror("open failed");
        return -1;
    }
    // write on the special file the id of the current task
    str_f = " ]3 ";
    len_f = strlen(str_f) + 1;
    if (write(fd, str_f, len_f) != len_f)
    {
        perror("write failed");
        return -1;
    }
    // close the special file
    close(fd);

    return 0;
}

void *task3(void *ptr)
{
    // set thread affinity, that is the processor on which threads shall run
    cpu_set_t cset;
    CPU_ZERO(&cset);
    CPU_SET(0, &cset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cset);

    int i = 0;
    for (i = 0; i < 40; i++)
    {
        // IMPORTANT: uncomment the following lines to use priority ceiling mutex
        /*
        // take the mutex
        pthread_mutex_lock(&mutex_sem);
        */

        // see task 1
        if (task3_code())
        {
            printf("task3_code failed\n");
            fflush(stdout);
            // release the mutex
            //pthread_mutex_unlock(&mutex_sem);
            return NULL;
        }

        // IMPORTANT: uncomment the following lines to use priority ceiling mutex
        /*
        // release the mutex
        pthread_mutex_unlock(&mutex_sem);
        */

        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_arrival_time[2], NULL);
        long int next_arrival_nanoseconds = next_arrival_time[2].tv_nsec + periods[2];
        next_arrival_time[2].tv_nsec = next_arrival_nanoseconds % 1000000000;
        next_arrival_time[2].tv_sec = next_arrival_time[2].tv_sec + next_arrival_nanoseconds / 1000000000;
    }
}

int task4_code()
{
    // strings to write
    const char *str_i;
    const char *str_f;

    // strings length
    int len_i, len_f;

    // file descriptor
    int fd;

    // open the special file
    if ((fd = open("/dev/my", O_RDWR)) == -1)
    {
        perror("open failed");
        return -1;
    }
    // write on the special file the id of the current task
    str_i = " 4[ ";
    len_i = strlen(str_i) + 1;
    if (write(fd, str_i, len_i) != len_i)
    {
        perror("write failed");
        return -1;
    }
    // close the special file
    close(fd);

    double uno;
    for (int i = 0; i < OUTERLOOP; i++)
    {
        for (int j = 0; j < INNERLOOP; j++)
        {
            uno = rand() * rand();
        }
    }

    // open the special file
    if ((fd = open("/dev/my", O_RDWR)) == -1)
    {
        perror("open failed");
        return -1;
    }
    // write on the special file the id of the current task
    str_f = " ]4 ";
    len_f = strlen(str_f) + 1;
    if (write(fd, str_f, len_f) != len_f)
    {
        perror("write failed");
        return -1;
    }
    // close the special file
    close(fd);

    return 0;
}

void *task4(void *ptr)
{
    // set thread affinity, that is the processor on which threads shall run
    cpu_set_t cset;
    CPU_ZERO(&cset);
    CPU_SET(0, &cset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cset);

    // add an infinite loop
    while (1)
    {
        // wait for the proper condition to be signaled
        pthread_cond_wait(&cond_task_4, &mutex_task_4);

        // IMPORTANT: uncomment the following lines to use priority ceiling mutex
        /*
        // take the mutex
        pthread_mutex_lock(&mutex_sem);
        */

        // execute the task code
        if (task4_code())
        {
            printf("task4_code failed\n");
            fflush(stdout);
            // release the mutex
            //pthread_mutex_unlock(&mutex_sem);
            return NULL;
        }

        // IMPORTANT: uncomment the following lines to use priority ceiling mutex
        /*
        // release the mutex
        pthread_mutex_unlock(&mutex_sem);
        */
    }
}