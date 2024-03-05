#include <stdio.h>

#include "filter.h"
#include "pipeline.h"
#include <pthread.h>
#include "queue.h"


#define NUM_THREADS 61
#define NUM_THREADS_SCALE_UP 20
#define NUM_THREADS_VERTICAL_FLIP 20
#define NUM_THREADS_SAVE 20

struct thread_args{
    image_dir_t* image_dir;
    queue_t* queue_in;
    queue_t* queue_out;
};

void *load_image(void *arguments){
	struct thread_args *args = arguments;
	while(1){
		image_t* image = image_dir_load_next(args -> image_dir);
		if(image == NULL){
			break;
		}
		queue_push(args -> queue_out, image);
	}
	for(int i = 0; i < NUM_THREADS_SCALE_UP; i++){
		queue_push(args -> queue_out, NULL);
	}
	return 0;
}

void *scale_up_image(void *arguments){
	struct thread_args *args = arguments;
	while(1){
		image_t* image = queue_pop(args -> queue_in);
		if(image == NULL){
			break;
		}
		image = filter_scale_up(image, 3);
		queue_push(args -> queue_out, image);
	}
	queue_push(args -> queue_out, NULL);
	return 0;
}

void *vertical_flip_image(void *arguments){
	struct thread_args *args = arguments;
	while(1){
		image_t* image = queue_pop(args -> queue_in);
		if(image == NULL){
			break;
		}
		image = filter_vertical_flip(image);
		queue_push(args -> queue_out, image);
	}
	queue_push(args -> queue_out, NULL);
	return 0;
}

void *save_image(void *arguments){
	struct thread_args *args = arguments;
	while(1){
		image_t* image = queue_pop(args -> queue_in);
		if(image == NULL){
			break;
		}
		image_dir_save(args -> image_dir, image);
        printf(".");
        fflush(stdout);
        image_destroy(image);
	}

	return 0;
}

int pipeline_pthread(image_dir_t* image_dir) {
	pthread_t threads[NUM_THREADS];
  	int result_code;

	queue_t* queue_1 = queue_create(401);
	struct thread_args args_1;
	args_1.image_dir = image_dir;
	args_1.queue_out = queue_1;

	result_code = pthread_create(&threads[0], NULL, load_image, (void *)&args_1);
	printf("Creating thread %d\n", 0);

	queue_t* queue_2 = queue_create(401);
	struct thread_args args_2;
	args_2.queue_in = queue_1;
	args_2.queue_out = queue_2;

	for (int i = 0; i < NUM_THREADS_SCALE_UP; i++){
		result_code = pthread_create(&threads[i+1], NULL, scale_up_image, (void *)&args_2);
		printf("Creating thread %d\n", i+1);
	}

	queue_t* queue_3 = queue_create(401);
	struct thread_args args_3;
	args_3.queue_in = queue_2;
	args_3.queue_out = queue_3;

	for (int i = 0; i < NUM_THREADS_VERTICAL_FLIP; i++){
		result_code = pthread_create(&threads[i+21], NULL, vertical_flip_image, (void *)&args_3);
		printf("Creating thread %d\n", i + 21);
	}

	struct thread_args args_4;
	args_4.queue_in = queue_3;
	args_4.image_dir = image_dir;

	for (int i = 0; i < NUM_THREADS_SAVE; i++){
		result_code = pthread_create(&threads[i+41], NULL, save_image, (void *)&args_4);
		printf("Creating thread %d\n", i + 41);
	}

	for (int i = 0; i < NUM_THREADS; i++){
		pthread_join(threads[i], NULL);	
		printf("Joinded thread %d\n", i);
	}
	queue_destroy(queue_1);
	queue_destroy(queue_2);
	queue_destroy(queue_3);
}
