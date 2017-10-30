#include "ligra.h";
#include "graphColorJP.h"

const int GRANULARITY = 4096;

intT max(intT val1, intT val2) {
	if (val1 < val2) {
		return val2;
	} else {
		return val1;
	}
}

// Finds maximum value in given range
// Spawns left half and right half, then return the maximum for this
// range
intT paralleFindMax(intT *colors, int left_index, int right_index) {
	if (right_index - left_index < GRANULARITY) {
		intT max = -1;
		for (int i = left_index; i <= right_index) {
			if (colors[i] > max) {
				max = colors[i];
			}
		}
		return max;
	} else {
		cilk_spawn intT max_left = parallelFindMax(colors, left_index,
										(left_index + right_index) / 2);
		intT max_right = parallelFindMax(colors,
						(left_index + right_index) / 2 + 1, right_index);
		cilk_sync;

		return max(max_left, max_right);
	}
}

int paralleFindMaxDegree(sparseRowMajor<int, int> *_graphs, 
							int left_index, int right_index) {
	if (right_index - left_index < GRANULARITY) {
		int max = -1;
		for (int i = left_index; i <= right_index) {
			int degree = _graph->Starts[i + 1] - _graph->Starts[i];
			if (degree > max) {
				max = degree;
			}
		}
		return max;
	} else {
		cilk_spawn int max_left = parallelFindMax(_graphs, left_index,
										(left_index + right_index) / 2);
		int max_right = parallelFindMax(_graphs,
						(left_index + right_index) / 2 + 1, right_index);
		cilk_sync;

		return max(max_left, max_right);
	}
}

int count(char *counters, int left_index, int right_index) {
  if (right_index - left_index < GRANULARITY) {
    int res = 0;
    for (int i = left_index; i <= right_index; i++) {
      res += counters[i];
    }
    return res;
  } else {
    cilk_spawn int res1 = count(counters, left_index, (left_index + right_index) / 2);
    int res2 = count(counters, (left_index + right_index) / 2 + 1, right_index);
    cilk_sync;

    return res1 + res2;
  }
}

void color_vertices(sparseRowMajor<int, int> *G, bucket *bucket, int *colors,
                      int num_colors, palette **palettes, int &uncolored) {
  // TODO: Make this a cilk_for; beware of races on uncolored
  char *colored = malloc(bucket->size * sizeof(char));
  cilk_for(int i = 0; i < bucket->size; i++) {
    int vertex_id = bucket->vertices[i];
    if (colors[vertex_id] == -1) {
      colors[vertex_id] = find_min(palettes[vertex_id]);
      colored[i] = 1;
    } else {
      colored[i] = 0;
    }
  }

  uncolored -= count(colored, 0, bucket->size - 1);
}

void update_neighbours(sparseRowMajor<int, int> *G, bucket *bucket, int *colors,
                        int num_colors, palette **palettes, int *saturation_degrees,
                        int *init_colors, bucketArray *bucket_array) {
  // Would like cilk_for, but that would result in a race
  // Use tournament for better results
  for(int i = 0; i < bucket->size; i++) {
    vertex_id = bucket->vertices[i];
    int *neighbours = G->ColIds[G->Starts[vertex_id]];
    int degree = G->Starts[vertex_id + 1] - G->Starts[vertex_id];
    // TODO: Learn to use tournament
    // TODO: We are probably inserting into the bucket too many times
    // TODO: The following has a race when inserting in bucket_array
    for(int j = 0; j < degree; j++) {
      int n_id = neighbours[j];
      if (set_color(palettes[n_id], colors[vertex_id])) {
        saturation_degrees[n_id]++;
        insert_bucketArray(bucket_array, init_colors[n_id], saturation_degrees[n_id],
                            n_id);
      }
    }
  }
}

void colorSD(sparseRowMajor<int, int> G, std::string _ordering) {

	// initial coloring given by JP
	intT *init_colors = newA(intT, G.numRows);
	colorGraphJP(&G, init_colors, _ordering);

	intT num_colors = parallelFindMax(init_colors, 0, G.numRows - 1); 
	int max_degree = parallelFindMaxDegree(G, 0, G.numRows - 1);

	bucketArray *buckets = init_bucketArray(num_colors, max_degree);
	for (int i = 0; i < G.numRows; i++) {
		// How to add nodes to buckets in a paralle fashion without
		// contention?
		// There is a way: find the index for each vertex (bascially
		// find for each vertex how many predecessors of same color it
		// has, and then add them to the appropriate bucket in parallel)
    //
    // For now this is serial, as we first want to test how well SD performs
    // In terms of number of colors used
    insert_bucketArray(buckets, init_colors[i], 0, i);  
	}

  palette **palettes = malloc(G.numRows * sizeof(pallete *));
  cilk_for (int i = 0; i < G.numRows; i++) {
    // TODO: Maybe increase this a bit
    palettes[i] = init_palette(num_colors);
  }

  
	int *colors = malloc(G.numRows * sizeof(int));
	cilk_for(int i = 0; i < G.numRows; i++) {
		// TODO: Might want to think about making this 0,
		// as checking if something is 0 should be faster
		colors[i] = -1;
	}

  int *saturation_degrees = malloc(G.numRows * sizeof(int));
  cilk_for(int i = 0; i < G.numRows; i++) {
    saturation_degrees[i] = 0;
  }

	int uncolored = G.numRows;

	while (uncolored > 0) {
		int index = get_max_saturation_degree(bucket_array);
		color_vertices(&G, bucket_array->buckets[i], colors, num_colors, palettes, uncolored);

		update_neighbors(&G, bucket_array->buckets[i], colors, num_colors, palettes, 
                      saturation_degrees, init_colors, bucket_array);
	}
}
