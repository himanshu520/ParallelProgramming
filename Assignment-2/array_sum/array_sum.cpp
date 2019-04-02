#include<bits/stdc++.h>
#include<pthread.h>
#include<semaphore.h>
#include<sys/time.h>
#define MAX_THREADS 10
#define MAX_REPEAT 10
#define MAX_FUNCTIONS 4
#define MAX_ARRAY_SIZE (int)(1e9)
using namespace std;

typedef void* (*function_p) (void *);

int array_size, no_of_threads, global_sum, global_rank;
int *arr;
double running_time_avg[MAX_FUNCTIONS][MAX_THREADS];
double running_time_max[MAX_FUNCTIONS][MAX_THREADS];
double running_time_min[MAX_FUNCTIONS][MAX_THREADS];
pthread_mutex_t sum_mutex;
sem_t sum_semaphore;
pthread_rwlock_t sum_rwlock;


void* busy_wait_sum(void *arg) {
    
    int my_rank = *((int*) arg), my_sum = 0;
    int my_low = (array_size + no_of_threads - 1) / no_of_threads * my_rank;
    int my_high = min(my_low + (array_size + no_of_threads - 1) / no_of_threads, array_size);

    for(int i = my_low; i < my_high; i++)
        my_sum += arr[i];

    while(global_rank != my_rank); {
        global_sum += my_sum;
        global_rank++;
    }

    return NULL;
}


void* mutex_sum(void *arg) {

    int my_rank = *((int*) arg), my_sum = 0;
    int my_low = (array_size + no_of_threads - 1) / no_of_threads * my_rank;
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
    int my_low = (array_size + no_of_threads - 1) / no_of_threads * my_rank;
    int my_high = min(my_low + (array_size + no_of_threads - 1) / no_of_threads, array_size);
    
    for(int i = my_low; i < my_high; i++)
        my_sum += arr[i];

    sem_wait(&sum_semaphore);
    global_sum += my_sum;
    sem_post(&sum_semaphore);

    return NULL;
}

void *rwlock_sum(void *arg) {

    int my_rank = *((int*) arg), my_sum = 0;
    int my_low = (array_size + no_of_threads - 1) / no_of_threads * my_rank;
    int my_high = min(my_low + (array_size + no_of_threads - 1) / no_of_threads, array_size);
    
    for(int i = my_low; i < my_high; i++)
        my_sum += arr[i];

    pthread_rwlock_wrlock(&sum_rwlock);
    global_sum += my_sum;
    pthread_rwlock_unlock(&sum_rwlock);

    return NULL;
}

int main(int argc, char **argv) {


    if(argc == 1) {

        cout << "Enter the size of the array (should be between " << MAX_THREADS << " and " 
             << MAX_ARRAY_SIZE << " inclusive): ";
        cin >> array_size;
        if(array_size < MAX_THREADS || array_size > MAX_ARRAY_SIZE) {
            cerr << "Invalid array size entered.\nTerminating program.......\n";
            exit(0);
        }

        arr = new int[array_size];
        cout << "Enter the elements of the array: \n";
        for(int i = 0; i < array_size; i++) {
            cout << "\tIndex" << setw(9) << (i + 1) << " :\t";
            cin >> arr[i]; 
        }

    } else {
        
        ifstream fin(argv[1]);
        if(fin.is_open()) {

            fin >> array_size;
            if(array_size < MAX_THREADS || array_size > MAX_ARRAY_SIZE) {
                cerr << "Invalid array size entered.\nTerminating program.......\n";
                exit(0);
            }
            
            arr = new int[array_size];
            for(int i = 0; i < array_size; i++)
                fin >> arr[i]; 

            fin.close();
        } else {
            cerr << "Error opening input file.\nTerminating program........\n";
            exit(0);
        }

    }


    function_p thread_functions[4] = {&busy_wait_sum, &mutex_sum, &semaphore_sum, &rwlock_sum};
    string thread_functions_name[] = {"BusyWaiting", "Mutex", "Semaphore", "ReadWriteLock"};

    pthread_mutex_init(&sum_mutex, NULL);
    sem_init(&sum_semaphore, 0, 1);

    for(int function_no = 0; function_no < MAX_FUNCTIONS; function_no++) {

        for(no_of_threads = 1; no_of_threads <= MAX_THREADS; no_of_threads++) {

            running_time_avg[function_no][no_of_threads - 1] = 0;
            running_time_max[function_no][no_of_threads - 1] = - DBL_MAX;
            running_time_min[function_no][no_of_threads - 1] = DBL_MAX;

            for(int repeat_count = 0; repeat_count < MAX_REPEAT; repeat_count++) {
                
                global_sum = global_rank = 0;
                struct timeval start_time, end_time; 
                gettimeofday(&start_time, NULL);

                pthread_t threads[no_of_threads];
                int thread_arg[no_of_threads];

                for(int thread_no = 0; thread_no < no_of_threads; thread_no++) {
                    thread_arg[thread_no] = thread_no;
                    int ret = pthread_create(&threads[thread_no], NULL, thread_functions[function_no], (void *) (&thread_arg[thread_no]));
                    if(ret != 0) {
                        cerr << "Error occurred during execution.\nTerminating program........\n";
                        for(int thread__no = 0; thread__no < thread_no; thread__no++)
                            pthread_cancel(threads[thread__no]);
                            
                        delete[] arr;
                        pthread_mutex_destroy(&sum_mutex);
                        sem_destroy(&sum_semaphore);
                        exit(0);
                    }
                }

                for(int thread_no = 0; thread_no < no_of_threads; thread_no++)
                    pthread_join(threads[thread_no], NULL);

                gettimeofday(&end_time, NULL);
                double time_taken;
                time_taken = (end_time.tv_sec - start_time.tv_sec) * 1e6; 
                time_taken = (time_taken + (end_time.tv_usec - start_time.tv_usec)) * 1e-6;
                running_time_avg[function_no][no_of_threads - 1] += time_taken;
                if(time_taken > running_time_max[function_no][no_of_threads - 1])
                    running_time_max[function_no][no_of_threads - 1] = time_taken;
                if(time_taken < running_time_min[function_no][no_of_threads - 1])
                    running_time_min[function_no][no_of_threads - 1] = time_taken;
            }

            running_time_avg[function_no][no_of_threads - 1] /= MAX_REPEAT;

        }
    }

    if(argc != 1) cout << "For " << argv[1] << "\n";
    cout << "\nThe sum of the array is: " << global_sum << "\n";
    cout << "The size of the array is: " << array_size << "\n\n";
    
    double (*running_time[3])[10] = { running_time_avg, running_time_max, running_time_min };
    string str[3] = { "average", "maximum", "minimum"};
    for(int i = 0; i < 3; i++) {
        cout << "The " << str[i] << " time spent for computing the array sum (in order of thread no from 1 to " << MAX_THREADS << "):\n";
        for(int function_no = 0; function_no < MAX_FUNCTIONS; function_no++) {
            cout << thread_functions_name[function_no] << " :\n\t";
            for(int no_of_threads = 0; no_of_threads < MAX_THREADS; no_of_threads++)
                cout << setw(10) << fixed << setprecision(5) << running_time[i][function_no][no_of_threads] << "\t";
            cout << "\n\n";
        }
    }
    

    ofstream fout("data.txt");
    if(fout.is_open()) {

        if(argc == 1) fout << "Console input\n";
        else fout << argv[1] << "\n";
        fout << global_sum << "\n" << array_size << "\n";
        fout << MAX_FUNCTIONS << "\n" << MAX_THREADS << "\n";
        for(int function_no = 0; function_no < MAX_FUNCTIONS; function_no++) {
            fout << thread_functions_name[function_no] << "\n";
            for(int ty = 0; ty < 3; ty++) {
                for(int no_of_threads = 0; no_of_threads < MAX_THREADS; no_of_threads++)
                    fout << setw(10) << setprecision(5) << running_time[ty][function_no][no_of_threads] << "\n";
            }
        }
        fout.close();
    } else {
        cerr << "Error writing output to file\n.Terminating program........";
    }


    delete[] arr;
    pthread_mutex_destroy(&sum_mutex);
    sem_destroy(&sum_semaphore);

    return 0;
}