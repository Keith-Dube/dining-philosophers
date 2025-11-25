#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

// Time quantum for round robin scheduling
#define TIME_QUANTUM 2

// Fork states
/**
 * The `CLEAN` and `DIRTY` constants represent the possible states of a fork in the dining philosophers problem.
 * - `CLEAN`: The fork is clean and available for use.
 * - `DIRTY`: The fork is dirty and / or  currently in use by a philosopher.
 */
enum {
    CLEAN,
    DIRTY
};

/**
 * The `HUNGRY`, `EATING`, and `THINKING` constants represent the possible states of a philosopher in the dining philosophers problem.
 * - `HUNGRY`: The philosopher is hungry and waiting to eat.
 * - `EATING`: The philosopher is currently eating.
 * - `THINKING`: The philosopher is currently thinking.
 */
// Philosopher states
enum {
    HUNGRY,
    EATING,
    THINKING
};

// Shared memory structure
/**
 * The `SharedData` struct represents the shared data used by the philosophers in the dining philosophers problem.
 * It contains the following fields:
 *
 * - `forks`: An array of `pthread_mutex_t` objects representing the mutexes for the forks.
 * - `fork_state`: An array of integers representing the state of each fork (clean or dirty).
 * - `state`: An array of integers representing the state of each philosopher (hungry, eating, or thinking).
 * - `eat_count`: An array of integers representing the number of times each philosopher has eaten.
 * - `mutex`: A global mutex for accessing the shared data.
 * - `cond_vars`: An array of `pthread_cond_t` objects representing the condition variables for each philosopher.
 * - `scheduled_phil`: The index of the currently scheduled philosopher.
 * - `all_have_eaten`: A flag indicating whether all philosophers have eaten.
 */
typedef struct {
    pthread_mutex_t *forks; // Mutexes for forks
    int *fork_state; // Fork states (clean/dirty)
    int *state; // Philosopher states (hungry/eating/thinking)
    int *eat_count; // Count of times each philosopher has eaten
    pthread_mutex_t mutex; // Global mutex for shared access
    pthread_cond_t *cond_vars; // Condition variables for philosophers
    int scheduled_phil; // Currently scheduled philosopher
    int all_have_eaten; // Flag indicating all philosophers have eaten
} SharedData;

SharedData *shared_data; // Pointer to shared memory
int N; // Number of philosophers

/**
 * The `test()` function is responsible for checking if a philosopher can acquire the forks needed to eat.
 * It checks if the philosopher is hungry and if the forks on either side are clean. If these conditions are met,
 * the philosopher's state is set to eating and a condition variable is signaled to wake up the philosopher.
 *
 * @param phil The index of the philosopher (0 to N-1) whose forks are being tested.
 */
void test(int phil) {
    if (shared_data->state[phil] == HUNGRY &&
        shared_data->fork_state[phil] == CLEAN &&
        shared_data->fork_state[(phil + 1) % N] == CLEAN) {
        shared_data->state[phil] = EATING;
        printf("Philosopher %d is eating.\n", phil);
        pthread_cond_signal(&shared_data->cond_vars[phil]);
    }
}

/**
 * The `take_forks()` function is responsible for a philosopher attempting to acquire the forks needed to eat.
 * It first marks the philosopher as hungry, and then checks if the forks on either side are clean.
 * If a fork is dirty, it is cleaned. The function then tries to acquire the forks by calling the `test()` function.
 * If the forks are available, the philosopher's state is set to eating and the forks are marked as dirty.
 * If the forks are not available, the philosopher waits on a condition variable until the forks become available.
 *
 * @param phil The index of the philosopher (0 to N-1) who is attempting to acquire the forks.
 */
void take_forks(int phil) {
    pthread_mutex_lock(&shared_data->mutex);
    shared_data->state[phil] = HUNGRY;
    printf("Philosopher %d is hungry.\n", phil);
    // Clean the fork on the left
    if (shared_data->fork_state[phil] == DIRTY) {
        shared_data->fork_state[phil] = CLEAN;
        printf("Philosopher %d cleaned fork %d.\n", phil, phil);
    }
    // Clean the fork on the right
    if (shared_data->fork_state[(phil + 1) % N] == DIRTY) {
        shared_data->fork_state[(phil + 1) % N] = CLEAN;
        printf("Philosopher %d cleaned fork %d.\n", phil, (phil + 1) % N);
    }
    test(phil); // Try to get forks
    while (shared_data->state[phil] != EATING) {
        pthread_cond_wait(&shared_data->cond_vars[phil], &shared_data->mutex); // Wait until forks are available
    }
    shared_data->fork_state[phil] = DIRTY;
    shared_data->fork_state[(phil + 1) % N] = DIRTY;
    pthread_mutex_unlock(&shared_data->mutex);
}

/**
 * The `put_forks()` function is responsible for releasing the forks used by a philosopher after they have finished eating.
 * It marks the forks as clean, increments the eat count for the philosopher, and tests if the neighboring philosophers can now eat.
 * This function is called by a philosopher after they have finished their meal.
 *
 * @param phil The index of the philosopher (0 to N-1) who is releasing the forks.
 */
void put_forks(int phil) {
    pthread_mutex_lock(&shared_data->mutex);
    shared_data->state[phil] = THINKING;
    printf("Philosopher %d is thinking.\n", phil);
    // Mark forks as clean
    shared_data->fork_state[phil] = CLEAN;
    shared_data->fork_state[(phil + 1) % N] = CLEAN;
    shared_data->eat_count[phil]++;
    // Test if neighbors can eat
    test((phil + N - 1) % N);
    test((phil + 1) % N);
    pthread_mutex_unlock(&shared_data->mutex);
}

/**
 * The `philosopher()` function represents the behavior of a single philosopher in the dining philosophers problem.
 * Each philosopher will repeatedly think, then attempt to acquire the forks to eat, and then release the forks to think again.
 * The function will continue to execute until all philosophers have eaten the required number of times.
 *
 * @param phil The index of the philosopher (0 to N-1).
 */
void philosopher(int phil) {
    while (1) {
        printf("Philosopher %d is thinking.\n", phil);
        usleep(rand() % 1000 + 1000); // Thinking time
        take_forks(phil);
        usleep(rand() % 1000 + 1000); // Eating time
        put_forks(phil);
        pthread_mutex_lock(&shared_data->mutex);
        if (shared_data->all_have_eaten) {
            pthread_mutex_unlock(&shared_data->mutex);
            break;
        }
        pthread_mutex_unlock(&shared_data->mutex);
    }
}

/**
 * The `scheduler()` function is responsible for scheduling the philosophers to take turns eating.
 * It runs in a separate process and coordinates the access to the shared resources (forks) among the philosophers.
 * The scheduler iterates through the philosophers, signaling each one to take their turn to eat.
 * It also checks if all philosophers have eaten the required number of times and exits the loop if so.
 */
void scheduler() {
    int i = 0;
    while (1) {
        pthread_mutex_lock(&shared_data->mutex);
        shared_data->scheduled_phil = i;
        printf("Scheduler: Philosopher %d's turn.\n", i);
        pthread_cond_signal(&shared_data->cond_vars[i]);
        pthread_mutex_unlock(&shared_data->mutex);
        sleep(TIME_QUANTUM);
        pthread_mutex_lock(&shared_data->mutex);
        int all_have_eaten = 1;
        for (int j = 0; j < N; j++) {
            if (shared_data->eat_count[j] == 0) {
                all_have_eaten = 0;
                break;
            }
        }
        if (all_have_eaten) {
            shared_data->all_have_eaten = 1;
            pthread_mutex_unlock(&shared_data->mutex);
            break;
        }
        pthread_mutex_unlock(&shared_data->mutex);
        i = (i + 1) % N;
    }
}

int main() {
    printf("Enter the number of philosophers: ");
    scanf("%d", &N);

    // Create shared memory
    int shm_fd = shm_open("/dining_philosophers", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(SharedData) + 3 * N * sizeof(int) + (N * sizeof(pthread_mutex_t)) + (N * sizeof(pthread_cond_t)));
    shared_data = mmap(0, sizeof(SharedData) + 3 * N * sizeof(int) + (N * sizeof(pthread_mutex_t)) + (N * sizeof(pthread_cond_t)), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // Initialize shared data
    pthread_mutexattr_t mutex_attr;
    pthread_condattr_t cond_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);

    // Allocate memory for dynamic arrays
    shared_data->forks = (pthread_mutex_t *) (shared_data + 1);
    shared_data->fork_state = (int *) (shared_data->forks + N);
    shared_data->state = (int *) (shared_data->fork_state + N);
    shared_data->eat_count = (int *) (shared_data->state + N);
    shared_data->cond_vars = (pthread_cond_t *) (shared_data->eat_count + N);
    shared_data->all_have_eaten = 0;

    pthread_mutex_init(&shared_data->mutex, &mutex_attr);
    for (int i = 0; i < N; i++) {
        pthread_mutex_init(&shared_data->forks[i], &mutex_attr);
        pthread_cond_init(&shared_data->cond_vars[i], &cond_attr);
        shared_data->fork_state[i] = DIRTY; // All forks start as DIRTY
        shared_data->state[i] = THINKING;
        shared_data->eat_count[i] = 0;
    }

    // Create philosopher processes
    for (int i = 0; i < N; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            philosopher(i);
            exit(0);
        }
    }

    // Create scheduler process
    pid_t sched_pid = fork();
    if (sched_pid == 0) {
        scheduler();
        exit(0);
    }

    // Wait for philosopher processes to finish
    for (int i = 0; i < N; i++) {
        wait(NULL);
    }

    // Wait for scheduler process to finish
    wait(NULL);

    // Cleanup shared memory
    munmap(shared_data, sizeof(SharedData) + 3 * N * sizeof(int) + (N * sizeof(pthread_mutex_t)) + (N * sizeof(pthread_cond_t)));
    shm_unlink("/dining_philosophers");

    return 0;
}
