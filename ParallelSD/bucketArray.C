#include "stdlib.h"
#include "bucketArray.h"
#include "bucket.h"
#include "parallel.h"

static const int GRANULARITY = 4096;

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
static int parallel_find_index_max_size(bucket **buckets,
								int left_index, int right_index) {
	if (right_index - left_index < GRANULARITY) {
		int max = left_index;
		for (int i = left_index + 1; i <= right_index; i++) {
			if (buckets[i]->size > buckets[max]->size) {
				max = i;
			}
		}
		if (buckets[max]->size == 0) {
			return -1;
		} else {
			return max;
		}
	} else {
		cilk_spawn int max_left = parallel_find_index_max_size(buckets, 
									left_index, 
									(left_index + right_index) / 2);
		int max_right = parallel_find_index_max_size(buckets,
									(left_index + right_index) / 2 + 1, 
									right_index);
		cilk_sync;

		if (max_left == -1) {
			return max_right;
		}

		if (max_right == -1) {
			return max_left;
		}

		if (buckets[max_left]->size > buckets[max_right]->size) {
			return max_left;
		} else {
			return max_right;
		}
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
			int res = parallel_find_index_max_size(
					bucket_array->buckets + i * bucket_array->colors,
					0, bucket_array->colors - 1);
			if (res != -1) {
				return res;
			}
		}
		return -1;
	} else {
		cilk_spawn int max_left = parallel_find_index_max_sd(
									bucket_array, left_index,
									(left_index + right_index) / 2);
		int max_right = parallel_find_index_max_sd(bucket_array,
						(left_index + right_index) / 2 + 1, right_index);
		cilk_sync;

		if (max_right == -1) {
			return max_left;
		} else {
			return max_right;
		}
	}
}

bucketArray* init_bucketArray(int colors, int max_degree) {
	bucketArray *bucket_array = (bucketArray *) malloc(sizeof(bucketArray *));
	bucket_array->buckets = (bucket **) malloc(colors * (max_degree + 1) * 
									sizeof(bucket *));
	cilk_for (int i = 0; i < colors; i++) {
		cilk_for (int j = 0; j < max_degree; j++) {
			bucket_array->buckets[index(bucket_array, i, j)] = 
				init_bucket();
		}
	}
	return bucket_array;
} 

int get_max_saturation_degree(bucketArray *bucket_array) {
	// TODO: Any smart ways to optimize this?
	return parallel_find_index_max_sd(bucket_array, 0,
								bucket_array->max_degree);
}

// A non-parallel version of inserting a vertex into bucket_array
// TODO: Implement a parallel version of this function
void insert_bucketArray(bucketArray *bucket_array, int color, int degree, int vertex_id) {
  insert_bucket(bucket_array->buckets[index(bucket_array, color, degree)], &vertex_id, 1);
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
