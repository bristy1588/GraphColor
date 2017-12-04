#ifndef _BUCKET_INCLUDED_H
#define _BUCKET_INCLUDED_H

typedef struct bucket {
	int *vertices;
	int size;
	int capacity;
} bucket;

bucket* init_bucket();
void kill_bucket(bucket *bucket);
void insert_bucket(bucket *bucket, int *vertices, int num);

bucket* copy_bucket(bucket* current);
void resize_bucket(bucket *bucket, int new_capacity);
void empty_bucket(bucket *bucket);
#endif // _BUCKET_INCLUDED_H
