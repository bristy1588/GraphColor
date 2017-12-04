#include "graphColorJP.h"
#include "bucket.h"
#include "bucketArray.h"
#include "graphColoringSD.h"
#include "palette.h"
#include <iostream>
#include "sequence.h"
#include "graph.h"
#include "parallel.h"
#include "gettime.h"

using namespace std;
bool DEBUG = false;
const int GRANULARITY = 4096;

static int max(int val1, int val2) {
	if (val1 < val2) {
		return val2;
	} else {
		return val1;
	}
}

// Finds maximum value in given range
// Spawns left half and right half, then return the maximum for this
// range
static int parallelFindMax(int *colors, int left_index, int right_index) {
	if (right_index - left_index < GRANULARITY) {
		int max_current = -1;
		for (int i = left_index; i <= right_index; i++) {
			if (colors[i] > max_current) {
				max_current = colors[i];
			}
		}
		return max_current;
	} else {
		int max_left = cilk_spawn parallelFindMax(colors, left_index,
										(left_index + right_index) / 2);
		int max_right = parallelFindMax(colors,
						(left_index + right_index) / 2 + 1, right_index);
		cilk_sync;
    int  max_current = max(max_left, max_right);
		return max(max_left, max_right);
	}
}

static int parallelFindMaxDegree(sparseRowMajor<int, int> *_graph, 
							int left_index, int right_index) {
	if (right_index - left_index < GRANULARITY) {
		int max = -1;
		for (int i = left_index; i <= right_index; i++) {
			int degree = _graph->Starts[i + 1] - _graph->Starts[i];
			if (degree > max) {
				max = degree;
			}
		}
		return max;
	} else {
		int max_left = cilk_spawn parallelFindMaxDegree(_graph, left_index,
										(left_index + right_index) / 2);
		int max_right = parallelFindMaxDegree(_graph,
						(left_index + right_index) / 2 + 1, right_index);
		cilk_sync;

		return max(max_left, max_right);
	}
}

static int count(char *counters, int left_index, int right_index) {
  if (right_index - left_index < GRANULARITY) {
    int res = 0;
    for (int i = left_index; i <= right_index; i++) {
      res += counters[i];
    }
    return res;
  } else {
    int res1 = cilk_spawn count(counters, left_index, (left_index + right_index) / 2);
    int res2 = count(counters, (left_index + right_index) / 2 + 1, right_index);
    cilk_sync;

    return res1 + res2;
  }
}

static void color_vertices(sparseRowMajor<int, int> *G, bucket *bucket, int *colors,
                      int num_colors, palette **palettes, int &uncolored) {
  // TODO: Make this a cilk_for; beware of races on uncolored
  char *colored = (char *) malloc(bucket->size * sizeof(char));
  for(int i = 0; i < bucket->size; i++) {
    int vertex_id = bucket->vertices[i];
    if (colors[vertex_id] == -1) {
      colors[vertex_id] = find_min(palettes[vertex_id]);
      
      colored[i] = 1;
    } else {
      colored[i] = 0;
    }
  }
  // Should be able to cilkify easily
  uncolored -= count(colored, 0, bucket->size - 1);
}

static void update_neighbours(sparseRowMajor<int, int> *G, bucket *bucket, int *colors,
                        int num_colors, palette **palettes, int *saturation_degrees,
                        int *init_colors, bucketArray *bucket_array) {
  // Would like cilk_for, but that would result in a race
  // Use tournament for better results
  for(int i = 0; i < bucket->size; i++) {
    int vertex_id = bucket->vertices[i];
    int *neighbours = &G->ColIds[G->Starts[vertex_id]];
    int degree = G->Starts[vertex_id + 1] - G->Starts[vertex_id];
    // TODO: Learn to use tournament
    // TODO: We are probably inserting into the bucket too many times
    // TODO: The following has a race when inserting in bucket_array
    //printf("#############   Vertex Id: %d i: %d Degree %d\n", vertex_id,i, degree);
    for(int j = 0; j < degree; j++) {
      int n_id = neighbours[j];
     //  printf("/// Current j: %d Vertex %d\n", j, n_id);       
       if (set_color(palettes[n_id], colors[vertex_id])) {
        saturation_degrees[n_id]++;
      //  printf("\nProcessing Vertex %d j:%d  Degree:%d \n", n_id, j, degree); 
        insert_bucketArray(bucket_array, init_colors[n_id], saturation_degrees[n_id],
                            n_id);
        //printf("SD Bucket inserted Vertex %d\n", n_id);
     }
    }
  }
 // printf("Exiting Update Neighbours\n");
 return;
}

intT colorSD(sparseRowMajor<int, int> *G, intT *init_colors, std::string _ordering) {

  // initial coloring given by JP
	int num_colors; int max_degree;
  num_colors = 2; 
  max_degree = 2;
  if (DEBUG) cout << "Yoo! Coloring SD Begins !!"<<endl;
  intT m = colorGraphJP(G, init_colors, _ordering);
  int x;
  if (DEBUG) cout << "JP Colors = " << m <<endl;
  
  num_colors = parallelFindMax(init_colors, 0, G->numRows - 1) + 1; 
	max_degree = parallelFindMaxDegree(G, 0, G->numRows - 1);

  printf("JP Num Colors: %d  , Max Degree %d\n ", num_colors, max_degree);
 
	bucketArray *bucket_array = init_bucketArray(num_colors, max_degree);
	
  for (int i = 0; i < G->numRows; i++) {
		// How to add nodes to buckets in a parallel fashion without
		// contention?
		// There is a way: find the index for each vertex (bascially
		// find for each vertex how many predecessors of same color it
		// has, and then add them to the appropriate bucket in parallel)
    //
    // For now this is serial, as we first want to test how well SD performs
    // In terms of number of colors used
    insert_bucketArray(bucket_array, init_colors[i], 0, i);  
	}

  if (DEBUG) printf("Bucket Array Initialized\n");
  palette **palettes = (palette **) malloc(G->numRows * sizeof(palette *));
  cilk_for (int i = 0; i < G->numRows; i++) {
    // NOTE: Palette initialized to max degree + 1 (might take a lot of space)
    palettes[i] = init_palette(num_colors * 5);
  }
 
 if (DEBUG) printf("Palette Initialized\n");
  
	int *colors = (int *) malloc(G->numRows * sizeof(int));
	cilk_for(int i = 0; i < G->numRows; i++) {
		// TODO: Might want to think about making this 0,
		// as checking if something is 0 should be faster
		colors[i] = -1;
	}

  int *saturation_degrees = (int *) malloc(G->numRows * sizeof(int));
  cilk_for(int i = 0; i < G->numRows; i++) {
    saturation_degrees[i] = 0;
  }

  if (DEBUG) printf("Saturation Degree initialized\n");
	int uncolored = G->numRows;

 // printf("........ Total Vertices %d\n", G->numRows);
	while (uncolored > 0) {
    //
     int index = get_max_saturation_degree(bucket_array);
    if (index < 0) {
      printf("$$$$$$  Max SD:%d  Uncolored Verices: %d\n", index, uncolored);
      printf(" Current Bucket Sizes Color: %d Max Degree %d:\n", bucket_array->colors, bucket_array->max_degree);
      for(int i = 0; i < bucket_array->colors * bucket_array->max_degree; i++){
          int b_col = i % bucket_array->colors;
          int b_degree = i/ bucket_array->colors;
          printf("Bucket Color:%d Degree %d Size :%d\n", b_col, b_degree, bucket_array->buckets[i]->size); 

      }
    }

    bucket* current_bucket = copy_bucket(bucket_array->buckets[index]);
    if (DEBUG) printf("......... Current Bucket index:%d Num Uncolored:%d Bucket Size:%d\n", index, uncolored, current_bucket->size);
    
    empty_bucket(bucket_array->buckets[index]);
    if (DEBUG) printf("Bucket Emptied!");
    color_vertices(G, current_bucket, colors, num_colors, palettes, uncolored);
    if (DEBUG) printf("Vertices Colored!");
    update_neighbours(G, current_bucket, colors, num_colors, palettes, 
                      saturation_degrees, init_colors, bucket_array);
	  if (DEBUG) printf("!!!!!!!Neighbours Updated!");
    kill_bucket(current_bucket); 
    if (DEBUG) printf("Buckets Killed\n");
  }

  int minCol = 100000;
  int prevColors[max_degree], newColors[max_degree];
  for(int i = 0; i < max_degree; i++){
    prevColors[i] = 0;
    newColors[i] = 0;
  }
  for(int i = 0; i < G->numRows; i++){
    prevColors[init_colors[i]]++;
    newColors[colors[i]]++;
    init_colors[i] =  colors[i];
    if (colors[i] < minCol) minCol = colors[i];
  }

   if (DEBUG){
     for(int i = 0; i < max_degree; i++){
       cout << "i: " <<i << " Prev: " << prevColors[i]<< " New "<< newColors[i] <<endl;
     }
  }
	printf("Returning from SD\n");
  return parallelFindMax(colors, 0, G->numRows - 1) + 1;

}
