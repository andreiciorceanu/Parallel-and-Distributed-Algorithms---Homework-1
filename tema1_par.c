#include <stdlib.h>
#include "genetic_algorithm.h"
#include "thread_str.h"
#include "individual.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
	// array with all the objects that can be placed in the sack
	sack_object *objects = NULL;
	//void *status;
	pthread_barrier_t barrier;
	pthread_mutex_t mutex;
	threadStr *v;
	// number of objects
	int object_count = 0;
	// maximum weight that can be carried in the sack
	int sack_capacity = 0;
	
	// number of generations
	int generations_count = 0;
	
	
	if (!read_input(&objects, &object_count, &sack_capacity, &generations_count, argc, argv)) {
		return 0;
	}
	//initializare vector de id-uri si de thread-uri 
	//cu verificarile necesare de alocare
	int K = atoi(argv[3]);
	pthread_t tid[K];
	int errCode[K];
	v = malloc(K * sizeof(struct threadStr));
	if(!v) {
		printf("Eroare la malloc\n");
		exit(1);
	}
	//initializare mutex/bariera
	pthread_barrier_init(&barrier,NULL, K);
	
	
	pthread_mutex_init(&mutex, NULL);
	
	individual *current_generation = (individual*) calloc(object_count, sizeof(individual));
	if(current_generation == NULL) {
		printf("Eroare la calloc\n");
		exit(1);
	}
	individual *next_generation = (individual*) calloc(object_count, sizeof(individual));
	if(next_generation == NULL) {
		printf("Eroare la calloc\n");
		exit(1);
	}
	//initializarea thread-urilor pt care am facut pthread_create

	for(int i = 0; i < K; i++) {
		
		v[i].id = i;
		v[i].barrier = &barrier;
		v[i].mutex = &mutex;
		v[i].P = K;
		v[i].current_generation = current_generation;
		v[i].next_generation = next_generation;
		v[i].object_count = object_count;
		v[i].generations_count = generations_count;
		v[i].sack_capacity = sack_capacity;
		v[i].objects = objects;
	
		errCode[i] = pthread_create(&(tid[i]), NULL, run_genetic_algorithm,(void *) &v[i]);
		if(errCode[i]) {
			printf("error creating thread %d\n", i);
			exit(-1);
		}
			
	}
	//facem join pe toate thread-urile si verificam aceasta operatie
	for(int i = 0;i < K;i++) {
		errCode[i] = pthread_join(tid[i], NULL);
		if(errCode[i]) {
			printf("error joining thread %d\n", i);
			exit(-1);
		}
			
	}
	//run_genetic_algorithm(objects, object_count, generations_count, sack_capacity);
	free(objects);

	pthread_barrier_destroy(&barrier);
	

	free(current_generation);
	free(next_generation);
	free(v);

	return 0;
}
