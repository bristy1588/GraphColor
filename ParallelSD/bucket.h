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

#endif // _BUCKET_INCLUDED_H
