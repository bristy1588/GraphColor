#include "stdlib.h"
#include "bucketArray.h"
#include "bucket.h"
#include "parallel.h"
#include "cilk/cilk.h"
#include <stdio.h>
static const int GRANULARITY = 4096;
bool BA_DEBUG = false;
int index(bucketArray *bucket_array, int c, int d) {
	return d * bucket_array->colors + c;
}

// First degree, then color
/*
std::pair<int, int> unpack_index(bucketArray *bucket_array, int index) {
	return std::make_pair<int, int>(index / bucket_array->colors,
									index % bucket_array->colors);
}
*/

// Find the largest size of a bucket in a given row
static int parallel_find_max_color_index(bucketArray *bucket_array, int degree, 
								int left_index, int right_index) {
	if (right_index - left_index < GRANULARITY) {
		int max_index = -1;  // -1 signifies that the max size is 0
    int max_size = 0;
		for (int i = left_index; i <= right_index; i++) {
      int current_index = index(bucket_array, i, degree);
			bucket* current_bucket = bucket_array->buckets[current_index];
      if (current_bucket->size > 0) {
				max_size = current_bucket->size;
        max_index = current_index;
			}
		}
	  return max_index; 

  } else {
		int max_left = parallel_find_max_color_index(bucket_array, degree, 
									left_index, (left_index + right_index) / 2);
		int max_right = parallel_find_max_color_index(bucket_array, degree,
									(left_index + right_index) / 2 + 1, right_index);

		if (max_left == -1) return max_right;
		if (max_right == -1) 	return max_left;
		
    int left_size = bucket_array->buckets[max_left]->size;
    int right_size = bucket_array->buckets[max_right]->size;
		if (left_size > right_size ) return max_left;
		else return max_right;
		
	}
}

// Find the index of the largest saturation degree bucket
// For multiple colors within same maximum saturation degree,
// we tie-break by the size of the bucket
// TODO: Right now, we are blindly looking at all the buckets
// Many of them will probably be empty, so it would be good if
// we found a better way to do this check
static int parallel_find_index_max_sd(bucketArray *bucket_array, int left_index,
							int right_index) {
	if (right_index - left_index < GRANULARITY) {
		for (int i = right_index; i >= left_index; i--) {
			int current_index= parallel_find_max_color_index(bucket_array, i,	0, bucket_array->colors - 1);
      if (current_index != -1) return current_index;
		}
		return -1;
	} else {
		int max_left = parallel_find_index_max_sd(
									bucket_array, left_index,
									(left_index + right_index) / 2);
		int max_right = parallel_find_index_max_sd(bucket_array,
						(left_index + right_index) / 2 + 1, right_index);

		if (max_right == -1) return max_left;
		else return max_right;
		
	}
}

bucketArray* init_bucketArray(int colors, int max_degree) {
	bucketArray *bucket_array = (bucketArray *) malloc(sizeof(bucketArray));
	bucket_array->buckets = (bucket **) malloc(colors * (max_degree + 1) * 
									sizeof(bucket *));
  bucket_array->colors = colors;
  // TODO: make sure the following (with +1) is correct
  bucket_array->max_degree = max_degree + 1;

	cilk_for (int i = 0; i < colors; i++) {
		cilk_for (int j = 0; j < max_degree + 1; j++) {
	      //printf("Index: %d i:%d j:%d \n", index(bucket_array, i, j), i, j);
       bucket_array->buckets[index(bucket_array, i, j)] = 
				init_bucket();
		}
	}
//  printf("Bucket Array Initialization Ok\n");
	return bucket_array;
} 

// This actually returns the index, Misleading name of sorts
int get_max_saturation_degree(bucketArray *bucket_array) {
	// TODO: Any smart ways to optimize this?


	return parallel_find_index_max_sd(bucket_array, 0,	bucket_array->max_degree - 1);
  /*
  int max_size = 0;
  int max_degree_index = -1;

  for(int deg = bucket_array->max_degree -1 ; deg >= 0; deg--){
    
    for(int col = 0; col < bucket_array->colors; col++){
       int cur_index = index(bucket_array, col, deg);
       if (bucket_array->buckets[cur_index]->size > max_size){
         max_size = bucket_array->buckets[cur_index]->size;
         max_degree_index = cur_index;
       }
     }
     if (max_size > 0){
       break;
     }
  }

  return max_degree_index;
*/
}

// A non-parallel version of inserting a vertex into bucket_array
// TODO: Implement a parallel version of this function
void insert_bucketArray(bucketArray *bucket_array, int color, int degree, int vertex_id) {
  if (BA_DEBUG) printf("Inserting into bucket index:%d vertex num:%d\n", index(bucket_array, color, degree), vertex_id);
  insert_bucket(bucket_array->buckets[index(bucket_array, color, degree)], &vertex_id, 1);
  if (BA_DEBUG) printf("Insertion Completed!");
  return;
}

void kill_bucketArray(bucketArray *bucket_array) {
  cilk_for (int i = 0; i < bucket_array->colors; i++) {
    cilk_for (int j = 0; j < bucket_array->max_degree; j++) {
      kill_bucket(bucket_array->buckets[index(bucket_array, i, j)]);
    }
  }

  free(bucket_array->buckets);
  free(bucket_array);
}
