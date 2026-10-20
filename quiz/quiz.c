#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    const char *question;
    const char *answer;
} question_t;

static question_t questions[] = {
    {"3*2", "6"},
    {"50/10", "5"},
    {"2+1+1+1", "5"},
    {"3^3", "27"},
    {"3+3", "6"},
    {"4/2", "2"}
};

//initialise mutex and condition variable.
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; //mutex allows for several threads to access same variables

//'changed' = 'some relevant quiz state may have changed. check variables again'. Both timer and question thread can signal this.
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER; //condition variable prevents busy waiting (repeatedly checking whether something has changed, which uses lots of CPU time). Instead, main should sleep when there is nothing to do

//these are the shared variables that describe the quiz's state. static gives these variables global scope in the program.
static bool timed_out = false; //has the timer expired?
static bool answered = false; //has the current question's input been processed?
static int score = 0; //how many answers were corrrect?

static bool input_ended = false;

static void *timer(void *unused) { //defines a function suitable for a pthread
    (void)unused;

    //blocks only the timer thread, other threads keep running
    sleep(5);

    //lock before accessing shared state
    pthread_mutex_lock(&mutex);

    //records the timeout and wakes main if it is waiting
    timed_out = true;
    //wakes a thread waiting on changed, if there is one. in this design, that is main
    pthread_cond_signal(&changed);

    //allow main to acquire mutex and inspect updated state.
    pthread_mutex_unlock(&mutex);

    return NULL;
}

static void *ask(void *arg) {
    question_t *q = arg; //thread received address of a question struct, which contains a question and an answer (above)
    char input[128]; //allocates space for up to 128 chars

    printf("%s? ", q->question); //accesses the question through the pointer
    fflush(stdout);

    //question worker reads input before locking the mutex.
    //if it held the mutex while waiting for input, the timer could not acquire the mutex to record the timeout.
    bool got_input = fgets(input, sizeof input, stdin) != NULL;

    if (got_input) { //if got_input was able to read a line from standard input,
        input[strcspn(input, "\n")] = '\0'; //remove the newline and replace with string terminator
    }

    //lock mutex before checking timeout or changing score
    pthread_mutex_lock(&mutex);

    if (!timed_out) { //only process the answer if no timeout has been recorded
        if (!got_input) {
            input_ended = true; //program stops if no more input available
        } else if (strcmp(input, q->answer) == 0) { //check whether strings are equal
            score++;
            printf("Correct!\n");
        } else {
            printf("Incorrect :-(\n");
        }
    }

    //records that the input attempt has completed and wakes main
    answered = true;
    pthread_cond_signal(&changed);

    pthread_mutex_unlock(&mutex);

    return NULL;
}

static void start_detached(
    void *(*function)(void *),
    void *argument
) {
    pthread_t thread;

    int error = pthread_create(
        &thread, NULL, function, argument
    );

    if (error != 0) {
        fprintf(
            stderr, "pthread_create: %s\n", strerror(error)
        );
        exit(EXIT_FAILURE);
    }

    error = pthread_detach(thread);

    if (error != 0) {
        fprintf(
            stderr, "pthread_detach: %s\n", strerror(error)
        );
        exit(EXIT_FAILURE);
    }
}

int main(void) {
    start_detached(timer, NULL);

    size_t count = sizeof questions / sizeof questions[0];

    for (size_t i = 0; i < count; i++) {
        pthread_mutex_lock(&mutex);

        if (timed_out) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        answered = false;

        pthread_mutex_unlock(&mutex);

        start_detached(ask, &questions[i]);

        pthread_mutex_lock(&mutex);

        while (!answered && !timed_out) { //while the question has not been answered and the timer has not expired (neither event has happened), keep waiting
            pthread_cond_wait(&changed, &mutex); //release mutex & begin waiting in one atomic operation, suspend main, reacquire the mutex before running
        } //main doesn't keep the mutex locked while it is asleep.

        bool stop = timed_out || input_ended;

        pthread_mutex_unlock(&mutex);

        if (stop) {
            break;
        }
    }

    pthread_mutex_lock(&mutex);

    printf(
        "\n%s, final score %d\n",
        timed_out ? "Timed out" : "Quiz finished",
        score
    );

    pthread_mutex_unlock(&mutex);

    fflush(stdout);
    _Exit(EXIT_SUCCESS);
}

/*need to coordinate 3 threads: main, question thread, timer
in the original program, if the user never presses Enter:
    - the question thread stays inside fgets (which waits for keyboard input)
    - main stays inside pthread_join (as it makes main wait until the current thread finishes)
    - the timer prints "5s have passed", but neither of the waiting threads is made to stop.
Printing a timeout message doesn't automatically interrupt another thread!
Main therefore needs a different instruction: wait until either the question has been answered or the timer has expired.
This why you have to stop using pthread_join

In this lab, we want to create a similar behaviour to Go's select statement:
select {
case result := <-answerChannel:
    // Process the answer.

case <-timerChannel:
    // Stop the quiz.
}
Here, main waits for whichever event becomes available.

To do this in C, we must use:
    - shared variables to record what happened
    - a mutex to protect those variables
    - a condition variable to let main sleep until something changes.
*/