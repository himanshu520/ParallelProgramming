#include<bits/stdc++.h>
#include<pthread.h>
#include<semaphore.h>
#include<sys/time.h>
#define MAX_REPEAT 5
#define MAX_FUNCTIONS 3
#define MAX_ARRAY_SIZE 20
#define MAX_ITERATIONS 500
#define COEFFICIENT (1e-2)
using namespace std;

typedef void* (*function_p) (void *);

int array_size, no_of_threads, no_of_iterations, mutex_count;
double *arr_old, *arr_new;
double running_time_avg[MAX_FUNCTIONS];
double running_time_max[MAX_FUNCTIONS];
double running_time_min[MAX_FUNCTIONS];
pthread_mutex_t sum_mutex;
pthread_mutex_t condition_mutex;
pthread_cond_t condition_var;
pthread_barrier_t barrier_var;


void* mutex_busy_wait_barrier(void *arg) {
    
    int my_rank = *((int*) arg);
    
    for(int iter_count = 1; iter_count <= no_of_iterations; iter_count++) {

        if(my_rank - 1 >= 0) {
            arr_new[my_rank] += (arr_old[my_rank - 1] - arr_old[my_rank]) * COEFFICIENT;
        } 
        if(my_rank + 1 < array_size) {
            arr_new[my_rank] += (arr_old[my_rank + 1] - arr_old[my_rank]) * COEFFICIENT;
        }

        pthread_mutex_lock(&sum_mutex);
        mutex_count++;
        if(mutex_count == (no_of_threads * iter_count - 1)) {
            for(int thrd = 0; thrd < no_of_threads; thrd++)
                arr_old[thrd] = arr_new[thrd];
        }
        pthread_mutex_unlock(&sum_mutex);
        while(mutex_count < no_of_threads * iter_count);

    }

    return NULL;
}


void* condition_var_barrier(void *arg) {

    int my_rank = *((int*) arg), my_sum = 0;
    
    for(int iter_count = 1; iter_count <= no_of_iterations; iter_count++) {

        if(my_rank - 1 >= 0) {
            arr_new[my_rank] += (arr_old[my_rank - 1] - arr_old[my_rank]) * COEFFICIENT;
        } 
        if(my_rank + 1 < array_size) {
            arr_new[my_rank] += (arr_old[my_rank + 1] - arr_old[my_rank]) * COEFFICIENT;
        }

        pthread_mutex_lock(&condition_mutex);
        mutex_count++;
        if(mutex_count == no_of_threads) {
            mutex_count = 0;
            for(int thrd = 0; thrd < no_of_threads; thrd++)
                arr_old[thrd] = arr_new[thrd];
            pthread_cond_broadcast(&condition_var);
        } else {
            while(pthread_cond_wait(&condition_var, &condition_mutex));
        }
        pthread_mutex_unlock(&condition_mutex);

    }

    return NULL;
}


void* barrier_barrier(void *arg) {

    int my_rank = *((int*) arg), my_sum = 0;

    for(int iter_count = 1; iter_count <= no_of_iterations; iter_count++) {

        if(my_rank - 1 >= 0) {
            arr_new[my_rank] += (arr_old[my_rank - 1] - arr_old[my_rank]) * COEFFICIENT;
        } 
        if(my_rank + 1 < array_size) {
            arr_new[my_rank] += (arr_old[my_rank + 1] - arr_old[my_rank]) * COEFFICIENT;
        }

        pthread_barrier_wait(&barrier_var);
    }

    return NULL;
}


int main(int argc, char **argv) {


    if(argc == 1) {

        cout << "Enter the size of the array (should be between 2 and " << MAX_ARRAY_SIZE << " inclusive): ";
        cin >> array_size;
        if(array_size < 2 || array_size > MAX_ARRAY_SIZE) {
            cerr << "Invalid array size entered.\nTerminating program.......\n";
            exit(0);
        }

        cout << "Enter the number of iterations to be performed (should be between 1 and " << MAX_ITERATIONS << " inclusive): ";
        cin >> no_of_iterations;
        if(no_of_iterations < 1 || no_of_iterations > MAX_ITERATIONS) {
            cerr << "Invalid input for number of iterations.\nTerminating program.......\n";
            exit(0);
        }

        arr_old = new double[array_size];
        arr_new = new double[array_size];
        cout << "Enter the values of the array: \n";
        for(int i = 0; i < array_size; i++) {
            cout << "\tIndex" << setw(9) << (i + 1) << " :\t";
            cin >> arr_old[i]; 
            arr_new[i] = arr_old[i];
        }

    } else {
        
        ifstream fin(argv[1]);
        if(fin.is_open()) {

            fin >> array_size;
            if(array_size < 2 || array_size > MAX_ARRAY_SIZE) {
                cerr << "Invalid array size entered.\nTerminating program.......\n";
                exit(0);
            }
            
            fin >> no_of_iterations;
            if(no_of_iterations < 1 || no_of_iterations > MAX_ITERATIONS) {
                cerr << "Invalid input for number of iterations.\nTerminating program.......\n";
                exit(0);
            }

            arr_old = new double[array_size];
            arr_new = new double[array_size];
            for(int i = 0; i < array_size; i++) {
                fin >> arr_old[i]; 
                arr_new[i] = arr_old[i];
            }

            fin.close();
        } else {
            cerr << "Error opening input file.\nTerminating program........\n";
            exit(0);
        }

    }


    function_p thread_functions[MAX_FUNCTIONS] = {&mutex_busy_wait_barrier, &condition_var_barrier, &barrier_barrier};
    string thread_functions_name[] = {"MutexBusyWaitBarrier", "ConditionVariableBarrier", "BarrierBarrier"};
    no_of_threads = array_size;

    pthread_mutex_init(&sum_mutex, NULL);
    pthread_mutex_init(&condition_mutex, NULL);
    pthread_cond_init(&condition_var, NULL);
    pthread_barrier_init(&barrier_var, NULL, no_of_threads);

    for(int function_no = 0; function_no < MAX_FUNCTIONS; function_no++) {

    running_time_avg[function_no] = 0;
    running_time_max[function_no] = -DBL_MAX;
    running_time_min[function_no] = DBL_MAX;

        for(int repeat_count = 0; repeat_count < MAX_REPEAT; repeat_count++) {
            struct timeval start_time, end_time; 
            gettimeofday(&start_time, NULL);

            pthread_t threads[no_of_threads];
            int thread_arg[no_of_threads];
            mutex_count = 0;

            for(int thread_no = 0; thread_no < no_of_threads; thread_no++) {
                thread_arg[thread_no] = thread_no;
                int ret = pthread_create(&threads[thread_no], NULL, thread_functions[function_no], (void *) (&thread_arg[thread_no]));
                if(ret != 0) {
                    cerr << "Error occurred during execution.\nTerminating program........\n";
                    for(int thread__no = 0; thread__no < thread_no; thread__no++)
                        pthread_cancel(threads[thread__no]);
                        
                    delete[] arr_old;
                    delete[] arr_new;
                    pthread_mutex_destroy(&sum_mutex);
                    pthread_mutex_destroy(&condition_mutex);
                    pthread_cond_destroy(&condition_var);
                    pthread_barrier_destroy(&barrier_var);
                    exit(0);
                }
            }

            for(int thread_no = 0; thread_no < no_of_threads; thread_no++)
                pthread_join(threads[thread_no], NULL);

            gettimeofday(&end_time, NULL);
            double time_taken;
            time_taken = (end_time.tv_sec - start_time.tv_sec) * 1e6; 
            time_taken = (time_taken + (end_time.tv_usec - start_time.tv_usec)) * 1e-6;
            running_time_avg[function_no] += time_taken;
            if(time_taken > running_time_max[function_no])
                running_time_max[function_no] = time_taken;
            if(time_taken < running_time_min[function_no])
                running_time_min[function_no] = time_taken;
        }

        running_time_avg[function_no] /= MAX_REPEAT;

    }

    if(argc != 1) cout << "For " << argv[1] << "\n";
    cout << "The size of the array is: " << array_size << "\n";
    cout << "The number of iterations is: " << no_of_iterations << "\n\n";
    
    cout << "The time spent for reaching equilibrium is (avg, max, min):\n";
    for(int function_no = 0; function_no < MAX_FUNCTIONS; function_no++) {
        cout << thread_functions_name[function_no] << " : " << fixed << setprecision(5) 
             << running_time_avg[function_no] << " " << running_time_max[function_no] << " " << running_time_min[function_no] << "\n";
    }
    

    ofstream fout("data.txt");
    if(fout.is_open()) {

        if(argc == 1) fout << "Console input\n";
        else fout << argv[1] << "\n";
        fout << array_size << "\n" << no_of_iterations << "\n";
        fout << MAX_FUNCTIONS << "\n";
        for(int function_no = 0; function_no < MAX_FUNCTIONS; function_no++) {
            fout << thread_functions_name[function_no] << "\n";
            fout << setw(10) << setprecision(5) << running_time_avg[function_no] << "\n";
            fout << running_time_max[function_no] << "\n" << running_time_min[function_no] << "\n";
        }
        fout.close();
    } else {
        cerr << "Error writing output to file\n.Terminating program........";
    }


    delete[] arr_old;
    delete[] arr_new;
    pthread_mutex_destroy(&sum_mutex);
    pthread_mutex_destroy(&condition_mutex);
    pthread_cond_destroy(&condition_var);
    pthread_barrier_destroy(&barrier_var);

    return 0;
}