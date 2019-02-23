#include<bits/stdc++.h>
#include<pthread.h>
#include<semaphore.h>
#define MAX_THREADS 10
#define MAX_REPEAT 5
#define MAX_FUNCTIONS 3
using namespace std;

typedef void* (*function_p) (void *);

int array_size, no_of_threads, global_sum, global_rank;
int *arr;
double running_time[3][MAX_THREADS];
pthread_mutex_t sum_mutex;
sem_t sum_semaphore;


void* busy_wait_sum(void *arg) {
    
    int my_rank = *((int*) arg), my_sum = 0;
    int my_low = (array_size + no_of_threads - 1) / no_of_threads;
    int my_high = min(my_low + (array_size + no_of_threads - 1) / no_of_threads, array_size);
    
    for(int i = my_low; i < my_high; i++)
        my_sum += arr[i];

    while(global_rank != my_rank); {
        global_sum += my_sum;
        global_sum++;
    }

    return NULL;
}

void* mutex_sum(void *arg) {

    int my_rank = *((int*) arg), my_sum = 0;
    int my_low = (array_size + no_of_threads - 1) / no_of_threads;
    int my_high = min(my_low + (array_size + no_of_threads - 1) / no_of_threads, array_size);
    
    for(int i = my_low; i < my_high; i++)
        my_sum += arr[i];

    pthread_mutex_lock(&sum_mutex);
    global_sum += my_sum;
    pthread_mutex_unlock(&sum_mutex);

    return NULL;
}

void *semaphore_sum(void *arg) {

    int my_rank = *((int*) arg), my_sum = 0;
    int my_low = (array_size + no_of_threads - 1) / no_of_threads;
    int my_high = min(my_low + (array_size + no_of_threads - 1) / no_of_threads, array_size);
    
    for(int i = my_low; i < my_high; i++)
        my_sum += arr[i];

    sem_wait(&sum_semaphore);
    global_sum += my_sum;
    sem_post(&sum_semaphore);

    return NULL;
}

int main(int argc, char **argv) {

    if(argc == 1) {

        cout << "Enter the size of the array (should be between 10 and 10^8 inclusive): ";
        cin >> array_size;
        if(array_size < 10 || array_size > 1e8) {
            cerr << "Invalid array size entered.\n Terminating program.......\n";
            exit(0);
        }
        arr = new int[array_size];

        cout << "Enter the elements of the array: \n";
        for(int i = 0; i < array_size; i++) {
            cout << "\tIndex" << setw(9) << (i + 1) << " :\t";
            cin >> arr[i]; 
        }

    } else {
        
        cin >> array_size;
        if(array_size < 1 || array_size > 1e8) {
            cerr << "Invalid array size entered.\n Terminating program.......\n";
            exit(0);
        }
        
        for(int i = 0; i < array_size; i++)
            cin >> arr[i]; 

    }

    function_p thread_functions[MAX_FUNCTIONS] = {&busy_wait_sum, &mutex_sum, &semaphore_sum};
    string thread_functions_name[] = {"BusyWaiting", "Mutex", "Semaphore"};

    pthread_mutex_init(&sum_mutex, NULL);
    sem_init(&sum_semaphore, 0, 1);

    for(int function_no = 0; function_no < MAX_FUNCTIONS; function_no++) {
        for(no_of_threads = 1; no_of_threads <= MAX_THREADS; no_of_threads++) {
            
            running_time[function_no][no_of_threads - 1] = 0;

            for(int repeat_count = 0; repeat_count < MAX_REPEAT; repeat_count++) {
                global_sum = global_rank = 0;
                clock_t start_time = clock();

                pthread_t threads[no_of_threads];

                for(int thread_no = 0; thread_no < no_of_threads; thread_no++) {
                    int ret = pthread_create(&threads[thread_no], NULL, &busy_wait_sum, (void *) (&thread_no));
                    if(ret != 0) {
                        cerr << "Error occurred during execution.\nTerminating program........\n";
                        for(int thread__no = 0; thread__no < thread_no; thread__no++)
                            pthread_cancel(threads[thread__no]);
                        exit(0);
                    }
                }

                for(int i = 0; i < no_of_threads; i++)
                    pthread_join(threads[i], NULL);

                clock_t end_time = clock();
                running_time[function_no][no_of_threads - 1] += (double)(end_time - start_time) / CLOCKS_PER_SEC;
            }

            running_time[function_no][no_of_threads - 1] /= MAX_REPEAT;

        }
    }

    if(argc == 1) {
        
        cout << "The sum of the array is:" << global_sum << "\n";
        cout << "The size of the array is: " << array_size << "\n";
        
        cout << "The time spent for computing the array sum (in order of thread no from 1 to " << MAX_THREADS << "):\n";
        for(int function_no = 0; function_no < MAX_THREADS; function_no++) {
            cout << thread_functions_name[function_no] << " :\n\t";
            for(int no_of_threads = 0; no_of_threads < MAX_THREADS; no_of_threads++)
                cout << setw(10) << setprecision(5) << running_time[function_no][no_of_threads] << "\t";
            cout << "\n\n";
        }
    }
    
    cout << global_sum << " " << array_size << "\n";

    cout << MAX_FUNCTIONS << " " << MAX_THREADS << "\n";

    for(int function_no = 0; function_no < MAX_THREADS; function_no++) {
        cout << thread_functions_name[function_no] << "\n\t";
        for(int no_of_threads = 0; no_of_threads < MAX_THREADS; no_of_threads++)
            cout << setw(10) << setprecision(5) << running_time[function_no][no_of_threads] << "\t";
        cout << "\n\n";
    }

    delete[] arr;

    return 0;
}