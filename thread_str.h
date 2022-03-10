#ifndef THREAD_STR_H
#define THREAD_STR_H

#include <pthread.h>
#include "sack_object.h"
//structura de thread pe care am creat-o cu o singura bariera pentru toate thread-urile
//un singur mutex si nr thread-uri, id si parametrii necesari pentru a putea avea un singur argument
//de tipul void * in functia run_alg
typedef struct threadStr {
    int id;
    pthread_barrier_t *barrier;
    pthread_mutex_t *mutex;
    int P;
    const sack_object *objects;
    int object_count;
    int generations_count;
    int sack_capacity;
    individual *current_generation;
    individual *next_generation;
} threadStr;

#endif