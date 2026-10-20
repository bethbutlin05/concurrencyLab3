#include <pthread.h>
#include <stdio.h>

void *hello_world(void *args) { //receives a general pointer
    int *n = args; //interprets the pointer as the address of an integer
    printf("Hello from thread %d\n", *n);
    pthread_exit(NULL);
}

int main(int argc, char const *argv[]) {
    pthread_t threads[5];//stores identifiers of the five threads
    int numbers[5]; //creates 5 separate integers to pass to the threads

    //start all 5 threads
    for (int n = 0; n < 5; n++){
        numbers[n] = n + 1????  //sets the integers to 1, 2, 3, 4 and 5
        if (pthread_create(&threads[n], NULL, hello_world, &numbers[n])) {
            printf("Error creating thread\n");
        }
        // &threads[i]: store the new thread's identifier here
        // NULL: use default thread attributes
        // hello_world: execute this function
        // &numbers[i]: pass the address of this thread's number
    }

    //wait for all 5 to finish
    for (int n = 0; n < 5; n++){
            if (pthread_join(threads[n], NULL)) {
                printf("Error joining thread\n");
            }
    }

    return 0;
}

/* Thread: a sequence of instructions being executed within a program.
This program initially has one thread: main, which executes main().
When you create another thread, there are two sequences of instructions that make progress independently:
    - main continues executing main()
    - the new thread executes the function you gave it.
Both threads belong to the same program and can access shared memory.
The extra threads in this function execute hello_world

 Here, we are working with C pthreads, which are different from Goroutines:
       - Go's runtime schedules goroutines as threads
       - OS threads are scheduled by the operating system
       - Goroutines are relatively lightweight, Pthreads generally require more resources.

A new pthread starts by executing the function passed to pthread_create. The output order is unpredictable.
Starting thread 1 doesn't guarantee that it prints first. */
