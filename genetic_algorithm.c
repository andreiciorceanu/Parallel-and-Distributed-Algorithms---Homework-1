#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "genetic_algorithm.h"
#include "thread_str.h"
#include <math.h>

#define min(a,b)  (((a)<(b))?(a):(b))

int read_input(sack_object **objects, int *object_count, int *sack_capacity, int *generations_count, int argc, char *argv[])
{
	FILE *fp;

	if (argc < 3) {
		fprintf(stderr, "Usage:\n\t./tema1 in_file generations_count\n");
		return 0;
	}

	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		return 0;
	}

	if (fscanf(fp, "%d %d", object_count, sack_capacity) < 2) {
		fclose(fp);
		return 0;
	}

	if (*object_count % 10) {
		fclose(fp);
		return 0;
	}

	sack_object *tmp_objects = (sack_object *) calloc(*object_count, sizeof(sack_object));

	for (int i = 0; i < *object_count; ++i) {
		if (fscanf(fp, "%d %d", &tmp_objects[i].profit, &tmp_objects[i].weight) < 2) {
			free(objects);
			fclose(fp);
			return 0;
		}
	}

	fclose(fp);

	*generations_count = (int) strtol(argv[2], NULL, 10);
	
	if (*generations_count == 0) {
		free(tmp_objects);

		return 0;
	}

	*objects = tmp_objects;

	return 1;
}

void print_objects(const sack_object *objects, int object_count)
{
	for (int i = 0; i < object_count; ++i) {
		printf("%d %d\n", objects[i].weight, objects[i].profit);
	}
}

void print_generation(const individual *generation, int limit)
{
	for (int i = 0; i < limit; ++i) {
		for (int j = 0; j < generation[i].chromosome_length; ++j) {
			printf("%d ", generation[i].chromosomes[j]);
		}

		printf("\n%d - %d\n", i, generation[i].fitness);
	}
}

void print_best_fitness(const individual *generation)
{
	printf("%d\n", generation[0].fitness);
}

void compute_fitness_function(const sack_object *objects, individual *generation, int object_count, int sack_capacity, threadStr thread)
{	
	int start,end;
	int profit;
	int weight;
	//paralelizare functie de fitness si adaugare de bariere
	start = thread.id * ceil((double)thread.object_count/thread.P);
	end = min(thread.object_count,(thread.id + 1) * ceil((double)thread.object_count/thread.P));
	pthread_barrier_wait(thread.barrier);
	
	for (int i = start; i < end; ++i) {
		
		weight = 0;
		profit = 0;
		
		for (int j = 0; j < generation[i].chromosome_length; ++j) {
			if (generation[i].chromosomes[j]) {
				
				weight += objects[j].weight;
				profit += objects[j].profit;
				
			}
		}
	
		generation[i].fitness = (weight <= sack_capacity) ? profit : 0;
	}
}

int cmpfunc(const void *a, const void *b)
{	
	int i;

	individual *first = (individual *) a;
	individual *second = (individual *) b;

	int res = second->fitness - first->fitness; // decreasing by fitness

	if (res == 0) {
		int first_count = 0, second_count = 0;

		for (i = 0; i < first->chromosome_length && i < second->chromosome_length; ++i) {
			first_count += first->chromosomes[i];
		}

		res = first_count - second_count; // increasing by number of objects in the sack
		if (res == 0) {
			return second->index - first->index;
		}
	}

	return res;
}

void mutate_bit_string_1(const individual *ind, int generation_index, threadStr thread)
{	
	int i, mutation_size;
	int step = 1 + generation_index % (ind->chromosome_length - 2);
	pthread_mutex_lock(thread.mutex);
	if (ind->index % 2 == 0) {
		// for even-indexed individuals, mutate the first 40% chromosomes by a given step
		mutation_size = ind->chromosome_length * 4 / 10;
		for (i = 0; i < mutation_size; i += step) {
			ind->chromosomes[i] = 1 - ind->chromosomes[i];
		}
	} else {
		// for even-indexed individuals, mutate the last 80% chromosomes by a given step
		mutation_size = ind->chromosome_length * 8 / 10;
		for (i = ind->chromosome_length - mutation_size; i < ind->chromosome_length; i += step) {
			ind->chromosomes[i] = 1 - ind->chromosomes[i];
		}
	}
	pthread_mutex_unlock(thread.mutex);
}

void mutate_bit_string_2(const individual *ind, int generation_index, threadStr thread)
{	
	//adaugare mutexi in fiecare functie pentru ca variabilele sa nu fie modificate de mai
	//multe threaduri si sa avem race condition
	int step = 1 + generation_index % (ind->chromosome_length - 2);
	pthread_mutex_lock(thread.mutex);
	// mutate all chromosomes by a given step

	for (int i = 0; i < ind->chromosome_length; i += step) {
		ind->chromosomes[i] = 1 - ind->chromosomes[i];
	}
	pthread_mutex_unlock(thread.mutex);
}

void crossover(individual *parent1, individual *child1, int generation_index, threadStr thread)
{	
	//adaugare structura de thread ca parametru pentru a folosi mutexul
	pthread_mutex_lock(thread.mutex);
	individual *parent2 = parent1 + 1;
	individual *child2 = child1 + 1;
	int count = 1 + generation_index % parent1->chromosome_length;

	memcpy(child1->chromosomes, parent1->chromosomes, count * sizeof(int));
	memcpy(child1->chromosomes + count, parent2->chromosomes + count, (parent1->chromosome_length - count) * sizeof(int));

	memcpy(child2->chromosomes, parent2->chromosomes, count * sizeof(int));
	memcpy(child2->chromosomes + count, parent1->chromosomes + count, (parent1->chromosome_length - count) * sizeof(int));
	pthread_mutex_unlock(thread.mutex);
}

void copy_individual(const individual *from, const individual *to, threadStr thread)
{	
	pthread_mutex_lock(thread.mutex);
	memcpy(to->chromosomes, from->chromosomes, from->chromosome_length * sizeof(int));
	pthread_mutex_unlock(thread.mutex);
}

void free_generation(individual *generation)
{
	int i;

	for (i = 0; i < generation->chromosome_length; ++i) {
		free(generation[i].chromosomes);
		generation[i].chromosomes = NULL;
		generation[i].fitness = 0;
	}
}

void* run_genetic_algorithm(void *arg)
{	
	//initializare structura de thread
	threadStr thread = *(threadStr *) arg;
	
	pthread_barrier_wait(thread.barrier);
	int count, cursor;
	int start,end;
	//paralelizarea for-urilor din aceasta functie pentru a obtine un timp si o accleratie mai buna
	//pentru start,end am folosit formulele din laborator
	start = thread.id * ceil((double)thread.object_count/thread.P);
	end = min(thread.object_count,(thread.id + 1) * ceil((double)thread.object_count/thread.P));
	individual *tmp = NULL;
	
	// set initial generation (composed of object_count individuals with a single item in the sack)
	
	for (int i = start; i < end; ++i) {
		
		thread.current_generation[i].fitness = 0;
		thread.current_generation[i].chromosomes = (int*) calloc(thread.object_count, sizeof(int));
		//verificarea alocarii de memorie pentru fiecare vector
		if(thread.current_generation[i].chromosomes == NULL) {
			printf("eroare la calloc\n");
			exit(-1);
		}
		thread.current_generation[i].chromosomes[i] = 1;
		thread.current_generation[i].index = i;
		thread.current_generation[i].chromosome_length = thread.object_count;

		thread.next_generation[i].fitness = 0;
		thread.next_generation[i].chromosomes = (int*) calloc(thread.object_count, sizeof(int));
		if(thread.next_generation[i].chromosomes == NULL) {
			printf("eroare la calloc\n");
			exit(-1);
		}
		thread.next_generation[i].index = i;
		thread.next_generation[i].chromosome_length = thread.object_count;
	}
	
	pthread_barrier_wait(thread.barrier);
	// iterate for each generation
	for (int k = 0; k < thread.generations_count; ++k) {
		cursor = 0;	
		
		// compute fitness and sort by it
		pthread_barrier_wait(thread.barrier);
		compute_fitness_function(thread.objects, thread.current_generation, end - start, thread.sack_capacity, thread);
		pthread_barrier_wait(thread.barrier);

		// if(thread.id == 0) {
		// 	qsort(thread.current_generation, thread.object_count, sizeof(individual), cmpfunc);
		// }
		//pentru a obtine un timp mai bun si o acceleratie am folosit functia de sortare oets paralelizata
		//din laboratorul 3 pe care am adaptat-o in functie de structura de thread
		int start = thread.id * (double)thread.object_count / thread.P;
		int start_even, start_odd;
		if (start % 2 == 0) {
			start_even = start;
			start_odd = start + 1;
		} else {
			start_even = start + 1;
			start_odd = start;
		}
		int end = min((thread.id + 1) * (double)thread.object_count / thread.P, thread.object_count);

		// implementati aici OETS paralel
		for(int i = 0; i < thread.object_count; i++) {
			for(int j = start_odd; (j < end) && (j < thread.object_count-1); j = j + 2) {
				if(thread.current_generation[j].fitness < thread.current_generation[j + 1].fitness) {
					individual aux = thread.current_generation[j];
					thread.current_generation[j] = thread.current_generation[j + 1];
					thread.current_generation[j + 1] = aux; 
				}
			}
		
			pthread_barrier_wait(thread.barrier);

			for(int j = start_even; (j < end) && (j < thread.object_count-1); j = j + 2) {
				if(thread.current_generation[j].fitness < thread.current_generation[j + 1].fitness) {
					individual aux = thread.current_generation[j];
					thread.current_generation[j] = thread.current_generation[j + 1];
					thread.current_generation[j + 1] = aux; 
				}
			}

			pthread_barrier_wait(thread.barrier);
		}

		//print_generation(thread.current_generation, thread.object_count);

		// keep first 30% children (elite children selection)
		
		count = thread.object_count * 3 / 10;
		start = thread.id * ceil((double)count/thread.P);
		end = min(count,(thread.id + 1) * ceil((double)count/thread.P));
		
		pthread_barrier_wait(thread.barrier);
		for (int i = start; i < end; ++i) {
			copy_individual(thread.current_generation + i, thread.next_generation + i,thread);
		}
		
		cursor = count;
		
		// mutate first 20% children with the first version of bit string mutation
		
		count = thread.object_count * 2 / 10;
		start = thread.id * ceil((double)count/thread.P);
		end = min(count,(thread.id + 1) * ceil((double)count/thread.P));
		
		
		pthread_barrier_wait(thread.barrier);
		for (int i = start; i < end; ++i) {
			copy_individual(thread.current_generation + i, thread.next_generation + cursor + i, thread);
			mutate_bit_string_1(thread.next_generation + cursor + i, k, thread);
		}
		
		cursor += count;
		
		// mutate thread.next 20% children with the second version of bit string mutation
		count = thread.object_count * 2 / 10;
		
		start = thread.id * ceil((double)count/thread.P);
		end = min(count,(thread.id + 1) * ceil((double)count/thread.P));
		
		for (int i = start; i < end; ++i) {
			copy_individual(thread.current_generation + i + count, thread.next_generation + cursor + i, thread);
			mutate_bit_string_2(thread.next_generation + cursor + i, k, thread);

		}

		pthread_barrier_wait(thread.barrier);
		cursor += count;
		
		// crossover first 30% parents with one-point crossover
		// (if there is an odd number of parents, the last one is kept as such)
		count = thread.object_count * 3 / 10;
		
		if (count % 2 == 1) {
			
			copy_individual(thread.current_generation + thread.object_count - 1, thread.next_generation + cursor + count - 1, thread);
			count--;
		}
		
		start = thread.id * ceil((double)count/thread.P);
		end = min(count,(thread.id + 1) * ceil((double)count/thread.P));
		
		if (start % 2 == 1) {
			start++;
		}

		for (int i = start; i < end; i += 2) {
			
			crossover(thread.current_generation + i, thread.next_generation + cursor + i, k, thread);
		}
		pthread_barrier_wait(thread.barrier);
		
		// switch to new generation
		tmp = thread.current_generation;
		thread.current_generation = thread.next_generation;
		thread.next_generation = tmp;

		start = thread.id * ceil((double)thread.object_count/thread.P);
		end = min(thread.object_count,(thread.id + 1) * ceil((double)thread.object_count/thread.P));
		
		for (int i = start; i < end; ++i) {
			
			thread.current_generation[i].index = i;
			
		}
		pthread_barrier_wait(thread.barrier);
		
		if (k % 5 == 0) {
			if(thread.id == 0) {
				print_best_fitness(thread.current_generation);
			} 
		}
		
	}
	pthread_barrier_wait(thread.barrier);
	compute_fitness_function(thread.objects, thread.current_generation, thread.object_count, thread.sack_capacity, thread);
	pthread_barrier_wait(thread.barrier);
	

	start = thread.id * (double)thread.object_count / thread.P;
	int start_even, start_odd;
	if (start % 2 == 0) {
		start_even = start;
		start_odd = start + 1;
	} else {
		start_even = start + 1;
		start_odd = start;
	}
	end = min((thread.id + 1) * (double)thread.object_count / thread.P, thread.object_count);

	for(int i = 0; i < thread.object_count; i++) {
		for(int j = start_odd; (j < end) && (j < thread.object_count-1); j = j + 2) {
			if(thread.current_generation[j].fitness < thread.current_generation[j + 1].fitness) {
				individual aux = thread.current_generation[j];
				thread.current_generation[j] = thread.current_generation[j + 1];
				thread.current_generation[j + 1] = aux; 
			}
		}
	
		pthread_barrier_wait(thread.barrier);

		for(int j = start_even; (j < end) && (j < thread.object_count-1); j = j + 2) {
			if(thread.current_generation[j].fitness < thread.current_generation[j + 1].fitness) {
				individual aux = thread.current_generation[j];
				thread.current_generation[j] = thread.current_generation[j + 1];
				thread.current_generation[j + 1] = aux; 
			}
		}

		pthread_barrier_wait(thread.barrier);
	}

	
	//printarea sa fie facuta doar de un thread pentru a nu avea printate de mai multe ori
	//acelasi rezultat
	if(thread.id == 0) {
		print_best_fitness(thread.current_generation);
	}

	pthread_exit(NULL);
}
