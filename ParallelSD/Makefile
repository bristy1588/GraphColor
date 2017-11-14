include parallelDefs

# required files
# deleted speculative_for.h
GLOBAL_REQUIRE = utils.h parallel.h sequence.h graph.h graphUtils.h blockRadixSort.h
BENCH_REQUIRE = graphColorOrdering.h graphColorOrdering.C graphColor.h
JP_REQUIRE = graphColorJP.h
LOCAL_REQUIRE = graphColoringSD.h bucket.h bucketArray.h palette.h
OBJS = Coloring.o graphColorOrdering.o graphColorJP.o graphColoringSD.o palette.o bucket.o bucketArray.o

include ../common/timeRequiredFiles
include MakeBench

