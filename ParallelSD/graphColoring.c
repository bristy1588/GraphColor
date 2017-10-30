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

void colorSD(sparseRowMajor<int, int> G, std::string _ordering) {

	// initial coloring given by JP
	intT *init_colors = newA(intT, G.numRows);
	colorGraphJP(&G, init_colors, _ordering);

	intT num_colors = parallelFindMax(init_colors, 0, G.numRows - 1); 
	int max_degree = parallelFindMaxDegree(G, 0, G.numRows - 1);

	bucketArray *buckets = init_bucketArray(num_colors, max_degree);
	cilk_for (int i = 0; i < G.numRows; i++) {
		// How to add nodes to buckets in a paralle fashion without
		// contention?
		// There is a way: find the index for each vertex (bascially
		// find for each vertex how many predecessors of same color it
		// has, and then add them to the appropriate bucket in parallel)
	}

	int *colors = malloc(G.numRows * sizeof(int));
	cilf_for(int i = 0; i < G.numRows; i++) {
		// TODO: Might want to think about making this 0,
		// as checking if something is 0 should be faster
		colors[i] = -1;
	}
	int uncolored = G.numRows;

	while (uncolored > 0) {
		int index = get_max_saturation_degree(bucket_array);
		color_vertices(bucket_array->buckets[i]);

		update_neighbors(bucket_array->buckets[i]);
	}
}