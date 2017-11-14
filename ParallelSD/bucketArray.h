#ifndef _BUCKET_ARRAY_INCLUDED_H
#define _BUCKET_ARRAY_INCLUDED_H

#include "bucket.h"

typedef struct bucketArray {
	// TODO: Buckets is not contiguous in memory, which will probably result in
	// very bad cache performance. Make this contiguous; of course, you won't be able
	// to initialize buckets with init_bucket, as that mallocs the bucket, which
	// you won't need now.
	bucket **buckets;
	int colors;
	int max_degree;
} bucketArray;

int index(bucketArray *bucket_array, int c, int d);
// std::pair<int, int> unpack_index(bucketArray *bucket_array, int index);

bucketArray *init_bucketArray(int colors, int degree);

int get_max_saturation_degree(bucketArray *bucket_array);
void insert_bucketArray(bucketArray *bucket_array, int color, int degree, int vertex_id);

void kill_bucketArray(bucketArray *bucket_array);


#endif // _BUCKETARRAY_INCLUDED_H
