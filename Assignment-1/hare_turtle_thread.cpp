#include<bits/stdc++.h>
#include<pthread.h>
using namespace std;

//declaring the global variable specific to hare and tortoise simulation
int start_pos = 0, finish_pos = 1000;
int new_pos_hare = 0, old_pos_hare = 0, new_pos_tort = 0, old_pos_tort = 0;
int god_prob = 1, tim = 1, hare_sleep_prob = 5, max_sleep_time = 100, sleep_cond = 100;

//declaring global variables used in implementation
int counter = 0;
pthread_mutex_t cond_mutex, var_mutex;
pthread_cond_t cond_var;

//the function that the threads execute
//the argument passed to the thread is an integer using which each thread will choose to run the code
//which is spcific to them
void* thread_fun(void *thread_arg) {
    int my_rank = *((int *) thread_arg);
    int sleep_time = 0;
    while(true) {

        //god modifying the positions of tortoise and hare
        if(my_rank == 4) {
            if(rand() % 100 <= god_prob) {
                cout << "God in action :-\n";
                new_pos_hare = rand() % 1000;
                new_pos_tort = rand() % 1000;
                cout << "New position of hare : " << new_pos_hare << "\n";
                cout << "New position of tortoise : " << new_pos_tort << "\n";
            }
        }

        //synchronising all the four threads using condition variable
        pthread_mutex_lock(&cond_mutex);
        counter++;
        if(counter == 4) {
            counter = 0;
            old_pos_tort = new_pos_tort;
            old_pos_hare = new_pos_hare;
            pthread_cond_broadcast(&cond_var);
        } else {
            while(pthread_cond_wait(&cond_var, &cond_mutex) != 0);
        }
        pthread_mutex_unlock(&cond_mutex);

        if(my_rank == 0) {
            //tortoise moving
            new_pos_tort += 1;
        } else if(my_rank == 1) {
            //if hare is sleeping, then update its sleep time
            if(sleep_time > 0) sleep_time--;
            else {
                //if hare is not sleeping and is well ahead of tortoise, choose to sleep or not
                if(old_pos_hare - old_pos_tort >= sleep_cond && rand() <= hare_sleep_prob) {
                    sleep_time = rand() % max_sleep_time;
                    cout << "Hare has decided to sleep for " << sleep_time + 1 << " seconds\n";
                } else {
                    //if hare has decided not to sleep, then it will move
                    new_pos_hare += 3;
                }
            }
        }

        //synchronising all the threads using condition variable
        pthread_mutex_lock(&cond_mutex);
        counter++;
        if(counter == 4) {
            counter = 0;
            old_pos_hare = new_pos_hare;
            old_pos_tort = new_pos_tort;
            pthread_cond_broadcast(&cond_var);
        } else {
            while(pthread_cond_wait(&cond_var, &cond_mutex) != 0);
        }
        pthread_mutex_unlock(&cond_mutex);

        //reporter printing the positions of tortoise and god
        if(my_rank == 3) {
            cout << "At time " << tim++ << "seconds :\t Hare is at " << old_pos_hare << ",\t Tortoise is at " << old_pos_tort << ".\n";
        }

        //checking loop breaking condition - either tortoise or hare wins
        if(old_pos_hare >= finish_pos || old_pos_tort >= finish_pos) break;
    }

    //nothing to return, hence NULL
    return NULL;
}

int main() {
    //seeding the random time generator
    srand(time(0));

    //initialising the mutex and condition variable
    pthread_mutex_init(&cond_mutex, NULL);
    pthread_cond_init(&cond_var, NULL);

    //initialising the threads
    pthread_t threads[4];
    for(int i = 0; i < 4; i++)
        pthread_create(&threads[i], NULL, &thread_fun, (void *)(&i));
    
    //waiting for the threads to complete
    for(int i = 0; i < 4; i++)
        pthread_join(&threads[i], NULL);

    //printing the final result
    print_result();

    return 0;
}