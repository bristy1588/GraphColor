#include "stdlib.h"
#include "bucket.h"
#include "parallel.h"
#include <stdio.h>

bool B_DEBUG = false;
bucket* init_bucket() {
	bucket *new_bucket = (bucket *) malloc(sizeof(bucket *));
	new_bucket->vertices = (int *) malloc(sizeof(int));
	new_bucket->size = 0;
	new_bucket->capacity = 1;
	return new_bucket;
}

static void free_bucket(bucket *bucket) {
	free(bucket->vertices);
}

void kill_bucket(bucket *bucket) {
	free_bucket(bucket);
	free(bucket);
}


bucket* copy_bucket(bucket* current){
  bucket *new_bucket = init_bucket();
  insert_bucket(new_bucket, current->vertices, current->size);
  return new_bucket;
}

void resize_bucket(bucket *bucket, int new_capacity) {
	if (B_DEBUG) printf(" Resizing to capacity: %d\n", bucket->capacity);
  bucket->capacity = new_capacity;
  if (bucket->size > bucket->capacity) 
    printf("Resizing Error Size %d is bigger than Capacity %d\n", bucket->size, bucket->capacity);
  
  int *new_vertices = (int *) malloc(new_capacity * sizeof(int));
	for (int i = 0; i < bucket->size; i++) {
		new_vertices[i] = bucket->vertices[i];
	}
  if (B_DEBUG) printf(" Done making copies\n");
	free(bucket->vertices);

  if (B_DEBUG) printf("Freed the old vertices\n");
	bucket->vertices = new_vertices;
  return;
}

void empty_bucket(bucket *bucket) {
  bucket->size = 0;
  resize_bucket(bucket, 1);
}

void insert_bucket(bucket *bucket, int *vertices, int num) {

  if (bucket->size + num > bucket->capacity) {
	 if (B_DEBUG)	printf("Resizing bucket for insertion Bucket Size %d Bucket Capacity %d\n", bucket->size, bucket->capacity);
    resize_bucket(bucket, 2 * (bucket->size + num));
	
  }
  if (B_DEBUG) printf ("Bucket Size: %d Bucket Capacity %d Num %d\n", bucket->size, bucket->capacity, num); 
	for (int i = 0; i < num; i++) {
	  if (B_DEBUG) printf("Inserting %d: Vertex %d\n", i, vertices[i]);
    bucket->vertices[bucket->size + i] = vertices[i];
    if (B_DEBUG) printf("..Last step of inserting %d: Vertex %d\n Done\n", i, bucket->vertices[bucket->size +i]);
	}
	bucket->size += num;
 if (B_DEBUG) printf("Exiting out of insert_bucket\n");
}

// Doesn't use resize in case of overflow
// void insert_optimized(bucket *bucket, int *vertices, int num);
