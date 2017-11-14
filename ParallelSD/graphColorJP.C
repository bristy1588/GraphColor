#include "graphColorJP.h"

inline unsigned long LeafClass::updateLeafCounter(unsigned long _bitColor)
{
  while( true ) {
    if( __sync_lock_test_and_set(&mutex,1) == 0 ) {
      bitColor |= _bitColor;
      if( counter == 1 ) {
        return bitColor;
      } else {
        counter--;
        mutex = 0;
        return 0L;
      }
    } else {
      while( mutex != 0 );
    }
  }

}

inline unsigned long Vertex::updateLocalCounter(unsigned long _bitColor)
{
  while( true ) {
    if( __sync_lock_test_and_set(&mutex,1) == 0 ) {
      finalColor |= _bitColor;
      if( counter == 1 ) {
        return finalColor;
      } else {
        counter--;
        mutex = 0;
        return 0L;
      }
    } else {
      while( mutex != 0 );      
    }
  }
}

inline unsigned long Vertex::competeInTournament(
  unsigned int _hash, // of incoming gladiator 
  unsigned int _color,
  unsigned char *_tournamentArray)
{
  unsigned long bitColor = OVERFLOW_BIT_COLOR;
  if( _color <= MAX_BIT_COLOR ) {
    bitColor = 1L << (unsigned long) _color;
  }
  if( numPredecessors <= COUNTER_THRESHOLD ) { // just use the local counter
    return updateLocalCounter(bitColor);
  } else { //full tournament
    unsigned int size = 1 << (unsigned int) logSize;
    unsigned char *tournament = &_tournamentArray[(edgeIndex+7)&(~0x07)];
    unsigned long *bitColors = (unsigned long *) tournament;
    unsigned int index = (_hash & (size - 1));
    LeafClass *leaf = (LeafClass *) &(bitColors[size]);
    bitColor = leaf[index].updateLeafCounter(bitColor);
    if( bitColor > 0 ) { // compete in tournament
      index += size;
      while( index > 1 ) {
        index = (index >> 1);
        if( bitColors[index] != 0 ) {
          bitColor |= bitColors[index];
        } else {
          unsigned long getAndSetResult = __sync_lock_test_and_set(&bitColors[index],bitColor);
          if( getAndSetResult != 0 ) { // I'm last
            bitColor |= getAndSetResult;
          } else {
            return 0L;
          }
        }
      }
      return bitColor;
    }
    else
      return 0L;
  }
}


void Vertex::colorVertex(
  Vertex *_vertices, 
  unsigned int _vertexID,
  unsigned char *_tournamentArray, 
  unsigned int *_neighbors, 
  unsigned long _bitColor) 
{
  if( numSuccessors > 0 )
  {
    unsigned int *successors = &(_neighbors[edgeIndex]);
    prefetchSuccessors(successors,numSuccessors,_vertices,firstSuccessor,secondSuccessor);
  }
    unsigned char *tournament = &_tournamentArray[(edgeIndex+7)&(~0x07)];
  
  if (numPredecessors == 0) 
  {
    finalColor = 0;
  }
  else 
  {
    if ((_bitColor & NO_BIT_COLOR_AVAILABLE ) == NO_BIT_COLOR_AVAILABLE) 
    {
      unsigned long *bitColors = (unsigned long *)tournament;
      for( unsigned int i = 0; i < (numPredecessors - MAX_BIT_COLOR + 63) >> 6; i++ )
        bitColors[i] = 0;
      unsigned int *predecessors = &_neighbors[edgeIndex+numSuccessors];
      for( unsigned int i = 0; i < numPredecessors; i++ ) {
        unsigned int color = (unsigned int) _vertices[predecessors[i]].finalColor;
        if( ( color > MAX_BIT_COLOR ) && ( color <= numPredecessors ) )
        {
          setBit(bitColors, color - MAX_BIT_COLOR - 1);
          //colors[color-MAX_BIT_COLOR-1] = 1;
        }
      }
      finalColor = getLowestUnsetBit(bitColors) + MAX_BIT_COLOR + 1;
    }
    else {
      finalColor = findFirstBitSet(_bitColor ^ (_bitColor+1));    
    }
  }
  if (numSuccessors > 0)
  {
    unsigned int *successors = &(_neighbors[edgeIndex]);
    processSuccessors(successors,numSuccessors,_vertices,_vertexID,finalColor,_tournamentArray, _neighbors);
  }
}

inline void Vertex::scalarInit(unsigned int _neighborIndex)
{
  numPredecessors = 0;
  numSuccessors = 0;
  firstSuccessor = -1;
  secondSuccessor = -1;
  mutex = 0;
  finalColor = 0L;
  edgeIndex = _neighborIndex;
}

inline void Vertex::tournamentInit(
  unsigned char *_tournamentArray, 
  unsigned int _neighborIndex,
  unsigned int *_neighbors)
{
  if( numSuccessors >= 1 )
    firstSuccessor = _neighbors[_neighborIndex];
  if( numSuccessors >= 2 )
    secondSuccessor = _neighbors[_neighborIndex+1];  
    
  if( numPredecessors <= COUNTER_THRESHOLD ) {
    counter = (unsigned short) numPredecessors;
    mutex = 0;
    return;
  } else {
    logSize = logBaseTwoFloor(numPredecessors/NUM_LEAF_MEMBERS);
    unsigned int size = (1 << logSize);
    unsigned int mask = size - 1;
    // we get 1 byte per neighbor in the tournament array...
    unsigned char *tournament = &(_tournamentArray[(_neighborIndex+7)&(~0x07)]);
    unsigned long *bitColors = (unsigned long *) tournament;    

    if( logSize >= 3 ) { // So, tournament is multiple of 8 bytes...
      for( unsigned int i = 0; i < (size*(sizeof(unsigned long) + sizeof(LeafClass)))>>3; i++ )
        bitColors[i] = 0;
    } else {
      for( unsigned int i = 0; i < size*(sizeof(unsigned long) + sizeof(LeafClass)); i++ )
        tournament[i] = 0;
    }

    unsigned int *predecessor = &_neighbors[_neighborIndex + numSuccessors];
    LeafClass *leaf = (LeafClass *) &(bitColors[size]);
    for( unsigned int i = 0; i < numPredecessors; i++ ) {
      leaf[hashVertexID(predecessor[i]) & mask].counter++;
    }
    for( unsigned int i = 0; i < size; i++ ) {
      if( leaf[i].counter == 0 ) { // we need to pre-populate the tournament in this case
        unsigned int j = (size + i) >> 1;
        while( bitColors[j] > 0 ) 
          j = j >> 1;
        bitColors[j] = OVERFLOW_BIT_COLOR;
      }
    }
  }
}

void Vertex::vertexInitR(
  unsigned char *_tournamentArray, 
  unsigned int _vertexID, 
  unsigned int *_neighbors,
  unsigned int _neighborIndex, 
  unsigned int _numNeighbors,
  unsigned int _mask,
  unsigned int _randVal = SEED_VALUE)
{
  scalarInit(_neighborIndex);
  if(_numNeighbors == 0) return;
  
  // Partition predecessors and successors
  unsigned int *successor = &_neighbors[_neighborIndex];
  unsigned int *predecessor = &_neighbors[_neighborIndex + _numNeighbors - 1];

  unsigned int myOrder = hashR(_vertexID, _mask, _randVal);
  for(unsigned int i = 0; i < _numNeighbors; i++) {
    unsigned int nbr = *successor;
    unsigned int hisOrder = hashR(nbr, _mask, _randVal);
    if( isSuccessor(myOrder, hisOrder, _vertexID, nbr) ) {
      successor++;
      numSuccessors++;
    } else {
      *successor = *predecessor;
      *predecessor = nbr;
      predecessor--;
      numPredecessors++;
    }
  }
  
  tournamentInit(_tournamentArray, _neighborIndex, _neighbors);
}

void Vertex::vertexInitFF(
  unsigned char *_tournamentArray, 
  unsigned int _vertexID, 
  unsigned int *_neighbors,
  unsigned int _neighborIndex, 
  unsigned int _numNeighbors)
{
  scalarInit(_neighborIndex);
  if(_numNeighbors == 0) return;
  
  // Partition predecessors and successors
  unsigned int *successor = &_neighbors[_neighborIndex];
  unsigned int *predecessor = &_neighbors[_neighborIndex + _numNeighbors - 1];

  unsigned int myOrder = _vertexID;
  for(unsigned int i = 0; i < _numNeighbors; i++) {
    unsigned int nbr = *successor;
    unsigned int hisOrder = nbr;
    if( isSuccessor(myOrder, hisOrder, _vertexID, nbr) ) {
      successor++;
      numSuccessors++;
    } else {
      *successor = *predecessor;
      *predecessor = nbr;
      predecessor--;
      numPredecessors++;
    }
  }
  
  tournamentInit(_tournamentArray, _neighborIndex, _neighbors);
}

void Vertex::vertexInitLF(
  sparseRowMajor<int,int>* _graph,
  unsigned char *_tournamentArray, 
  unsigned int _vertexID, 
  unsigned int *_neighbors,
  unsigned int _neighborIndex, 
  unsigned int _numNeighbors,
  unsigned int _mask,
  unsigned int _randVal = SEED_VALUE)
{
  scalarInit(_neighborIndex);
  if(_numNeighbors == 0) return;
  
  // Partition predecessors and successors
  unsigned int *successor = &_neighbors[_neighborIndex];
  unsigned int *predecessor = &_neighbors[_neighborIndex + _numNeighbors - 1];

  unsigned int myOrder = hashLF(_graph, _vertexID);
  //unsigned int myHash = reversible_hash_forward(_vertexID, _graph->numRows, _randVal);
  unsigned int myHash = hashR(_vertexID, _mask, _randVal);
  for(unsigned int i = 0; i < _numNeighbors; i++) {
    unsigned int nbr = *successor;
    unsigned int hisOrder = hashLF(_graph, nbr);
    unsigned int hisHash = hashR(nbr, _mask, _randVal);
    //unsigned int hisHash = reversible_hash_forward(nbr, _graph->numRows, _randVal);
    bool heIsSuccessor = (myOrder != hisOrder) ? myOrder > hisOrder : isSuccessor(myHash, hisHash, _vertexID, nbr);
    if( heIsSuccessor ) {
      successor++;
      numSuccessors++;
    } else {
      *successor = *predecessor;
      *predecessor = nbr;
      predecessor--;
      numPredecessors++;
    }
  }
  
  tournamentInit(_tournamentArray, _neighborIndex, _neighbors);
}

void Vertex::vertexInitLLF(
  sparseRowMajor<int,int>* _graph,
  unsigned char *_tournamentArray, 
  unsigned int _vertexID, 
  unsigned int *_neighbors,
  unsigned int _neighborIndex, 
  unsigned int _numNeighbors,
  unsigned int _shiftAmount,
  unsigned int _randVal = SEED_VALUE)
{
  scalarInit(_neighborIndex);
  if(_numNeighbors == 0) return;
  
  // Partition predecessors and successors
  unsigned int *successor = &_neighbors[_neighborIndex];
  unsigned int *predecessor = &_neighbors[_neighborIndex + _numNeighbors - 1];

  unsigned int myOrder = hashLLF(_graph, _vertexID, _shiftAmount, _randVal);
  for(unsigned int i = 0; i < _numNeighbors; i++) {
    unsigned int nbr = *successor;
    unsigned int hisOrder = hashLLF(_graph, nbr, _shiftAmount, _randVal);
    if( isSuccessor(myOrder, hisOrder, _vertexID, nbr) ) {
      successor++;
      numSuccessors++;
    } else {
      *successor = *predecessor;
      *predecessor = nbr;
      predecessor--;
      numPredecessors++;
    }
  }
  
  tournamentInit(_tournamentArray, _neighborIndex, _neighbors);
}

void Vertex::vertexInitSLL(
  unsigned char *_tournamentArray, 
  unsigned int _vertexID, 
  unsigned int *_neighbors,
  unsigned int _neighborIndex, 
  unsigned int _numNeighbors,
  unsigned int *_priority,
  unsigned int _mask,
  unsigned int _randVal = SEED_VALUE)
{
  scalarInit(_neighborIndex);
  if(_numNeighbors == 0) return;
  
  // Partition predecessors and successors
  unsigned int *successor = &_neighbors[_neighborIndex];
  unsigned int *predecessor = &_neighbors[_neighborIndex + _numNeighbors - 1];

  unsigned int myOrder = hashSLL(_vertexID,_priority);
  //unsigned int myHash = reversible_hash_forward(_vertexID, _maxVertexID, _randVal);
  unsigned int myHash = hashR(_vertexID, _mask, _randVal);
  for(unsigned int i = 0; i < _numNeighbors; i++) {
    unsigned int nbr = *successor;
    unsigned int hisOrder = hashSLL(nbr,_priority);
    //bool heIsSuccessor = (myOrder != hisOrder) ? myOrder > hisOrder : myHash < reversible_hash_forward(nbr, _maxVertexID, _randVal);
    bool heIsSuccessor = (myOrder != hisOrder) ? myOrder > hisOrder : isSuccessor(myHash, hashR(nbr, _mask, _randVal), _vertexID, nbr);
    if( heIsSuccessor ) {
      successor++;
      numSuccessors++;
    } else {
      *successor = *predecessor;
      *predecessor = nbr;
      predecessor--;
      numPredecessors++;
    }
  }
  
  tournamentInit(_tournamentArray, _neighborIndex, _neighbors);
}

void Vertex::vertexInitSL(
  unsigned char *_tournamentArray, 
  unsigned int _vertexID, 
  unsigned int *_neighbors,
  unsigned int _neighborIndex, 
  unsigned int _numNeighbors,
  unsigned int *_priority,
  unsigned int _mask,
  unsigned int _randVal = SEED_VALUE)
{
  scalarInit(_neighborIndex);
  if(_numNeighbors == 0) return;
  
  // Partition predecessors and successors
  unsigned int *successor = &_neighbors[_neighborIndex];
  unsigned int *predecessor = &_neighbors[_neighborIndex + _numNeighbors - 1];

  unsigned int myOrder = hashSL(_vertexID,_priority);
  //unsigned int myHash = reversible_hash_forward(_vertexID, _maxVertexID, _randVal);
  unsigned int myHash = hashR(_vertexID, _mask, _randVal);
  for(unsigned int i = 0; i < _numNeighbors; i++) {
    unsigned int nbr = *successor;
    unsigned int hisOrder = hashSL(nbr,_priority);
    //bool heIsSuccessor = (myOrder != hisOrder) ? myOrder > hisOrder : myHash < reversible_hash_forward(nbr, _maxVertexID, _randVal);
    bool heIsSuccessor = (myOrder != hisOrder) ? myOrder > hisOrder : isSuccessor(myHash, hashR(nbr, _mask, _randVal), _vertexID, nbr);
    if( heIsSuccessor ) {
      successor++;
      numSuccessors++;
    } else {
      *successor = *predecessor;
      *predecessor = nbr;
      predecessor--;
      numPredecessors++;
    }
  }
  
  tournamentInit(_tournamentArray, _neighborIndex, _neighbors);
}

inline void prefetchVertexInit(
  sparseRowMajor<int,int>* _graph, 
  Vertex *_vertices, 
  unsigned char *_tournamentArray, 
  unsigned int _index, 
  bool _fetchDegree)
{
  if( SOFTWARE_PREFETCHING_FLAG1 ) {
    if( _index + PREFETCH_DISTANCE < _graph->numRows )
    {
      if( (_index & 1) == 0 )
        __builtin_prefetch(&_vertices[_index], 1);
      if( (_index & 15) == 0 )
        __builtin_prefetch(&_graph->Starts[_index]);
    }
    unsigned int degree = _graph->Starts[_index + HALF_PREFETCH_DISTANCE] - _graph->Starts[_index + HALF_PREFETCH_DISTANCE - 1]; 
    if( _index + HALF_PREFETCH_DISTANCE < _graph->numRows )
    {
      __builtin_prefetch(&_graph->ColIds[_graph->Starts[_index + HALF_PREFETCH_DISTANCE - 1]],0);
      if( degree >= COUNTER_THRESHOLD )
        __builtin_prefetch(&_tournamentArray[_graph->Starts[_index + HALF_PREFETCH_DISTANCE - 1]], 1);
    }
    if( _fetchDegree ) {
      unsigned int degree = _graph->Starts[_index + 1] - _graph->Starts[_index];
      for( unsigned int j = 0; j < degree; j++ )
      {
        __builtin_prefetch(&_graph->Starts[_graph->ColIds[_graph->Starts[_index]+j]],0);
      }
    }
  }
}

void prefetchSuccessors(
  unsigned int *_successors,
  unsigned int _numSuccessors,
  Vertex *_vertices,
  int firstSuccessor,
  int secondSuccessor)
{
  if( SOFTWARE_PREFETCHING_FLAG1 ) {
    if( firstSuccessor >= 0 )
      __builtin_prefetch(&_vertices[firstSuccessor],1);
    if( secondSuccessor >= 0 )
      __builtin_prefetch(&_vertices[secondSuccessor],1);
    __builtin_prefetch(_successors,0);
  }  
}

void processSuccessors(
  unsigned int *_successors,
  unsigned int _numSuccessors,
  Vertex *_vertices,
  unsigned int _vertexID,
  unsigned long _finalColor,
  unsigned char *_tournamentArray, 
  unsigned int *_neighbors)
{
  unsigned int hashValue = hashVertexID(_vertexID);
  Vertex *vertex = &_vertices[_vertexID];
  unsigned long bitColor = _vertices[vertex->firstSuccessor].competeInTournament(hashValue, _finalColor, _tournamentArray);
  if( bitColor != 0 ) {
    cilk_spawn _vertices[vertex->firstSuccessor].colorVertex(_vertices,vertex->firstSuccessor,_tournamentArray,_neighbors, bitColor);
  }
  if( SOFTWARE_PREFETCHING_FLAG1 ) {
    for( unsigned int i = 2; i < MIN(_numSuccessors,PREFETCH_DISTANCE+2); i++ ) {
      __builtin_prefetch(&_vertices[_successors[i]], 1);
    }  
  }
  if( vertex->secondSuccessor >= 0 ) 
  {
    bitColor = _vertices[vertex->secondSuccessor].competeInTournament(hashValue, _finalColor, _tournamentArray);
    if( bitColor != 0 ) {
      cilk_spawn _vertices[vertex->secondSuccessor].colorVertex(_vertices,vertex->secondSuccessor,_tournamentArray,_neighbors, bitColor);
    }
  }
  
  if( _numSuccessors < CILK_FOR_THRESHOLD ) {
    for( int i = 2; i < _numSuccessors; i++ ) {
      if( SOFTWARE_PREFETCHING_FLAG1 ) {
        if( i + PREFETCH_DISTANCE < _numSuccessors )
        {
          __builtin_prefetch(&_vertices[_successors[i+PREFETCH_DISTANCE]], 1);
        }
        if( ( i + HALF_PREFETCH_DISTANCE < _numSuccessors ) && ( _vertices[_successors[i+HALF_PREFETCH_DISTANCE]].numPredecessors >= COUNTER_THRESHOLD) )
        {
          __builtin_prefetch(&_tournamentArray[_vertices[_successors[i+HALF_PREFETCH_DISTANCE]].edgeIndex], 1);
        }
      }
      unsigned long bitColor = _vertices[_successors[i]].competeInTournament(hashValue, _finalColor, _tournamentArray);
      if( bitColor != 0 ) {
        cilk_spawn _vertices[_successors[i]].colorVertex(_vertices,_successors[i],_tournamentArray,_neighbors, bitColor);
      }
    }
  }
  else {
    cilk_for( int i = 2; i < _numSuccessors; i++ ) {
      if( SOFTWARE_PREFETCHING_FLAG1 ) {
        if( i + PREFETCH_DISTANCE < _numSuccessors )
        {
          __builtin_prefetch(&_vertices[_successors[i+PREFETCH_DISTANCE]], 1);
        }
        if( ( i + HALF_PREFETCH_DISTANCE < _numSuccessors ) && ( _vertices[_successors[i+HALF_PREFETCH_DISTANCE]].numPredecessors >= COUNTER_THRESHOLD) )
        {
          __builtin_prefetch(&_tournamentArray[_vertices[_successors[i+HALF_PREFETCH_DISTANCE]].edgeIndex], 1);
        }
      }
      unsigned long bitColor = _vertices[_successors[i]].competeInTournament(hashValue, _finalColor, _tournamentArray);
      if( bitColor != 0 ) {
        _vertices[_successors[i]].colorVertex(_vertices,_successors[i],_tournamentArray,_neighbors,bitColor);
      }
    }    
  }
}

void colorGraphJPInit(
  sparseRowMajor<int,int>* _graph, 
  Vertex *_vertices, 
  unsigned char *_tournamentArray, 
  std::string _ordering, 
  unsigned int seed)
{
  unsigned int numEdges = (unsigned int) _graph->Starts[_graph->numRows];
  unsigned int logV = logBaseTwoFloor(_graph->numRows);
  unsigned int mask = (1 << logV) - 1;
  unsigned int shiftAmount = logV - logBaseTwoFloor(logFloor(_graph->numRows + 1) + 1) - 1;

  unsigned int N = _graph->numRows;
  
  if ( _ordering.compare("R") == 0 ) {
    cilk_for( unsigned int i = 0; i < N; i++ ) { // cilk_for
      prefetchVertexInit(_graph, _vertices, _tournamentArray, i, false);    
      _vertices[i].vertexInitR(_tournamentArray, i, (unsigned int *)_graph->ColIds, _graph->Starts[i], _graph->Starts[i+1]-_graph->Starts[i],mask,seed);
    }
  }
  else if( _ordering.compare("FF") == 0 ) {
    cilk_for( unsigned int i = 0; i < N; i++ ) { // cilk_for
      prefetchVertexInit(_graph, _vertices, _tournamentArray, i, false);    
      _vertices[i].vertexInitFF(_tournamentArray, i, (unsigned int *)_graph->ColIds, _graph->Starts[i], _graph->Starts[i+1]-_graph->Starts[i]);
    }
  }
  else if( _ordering.compare("LF") == 0 ) {
    cilk_for( unsigned int i = 0; i < N; i++ ) { // cilk_for
      prefetchVertexInit(_graph, _vertices, _tournamentArray, i, true);    
      _vertices[i].vertexInitLF(_graph, _tournamentArray, i, (unsigned int *)_graph->ColIds, _graph->Starts[i], _graph->Starts[i+1]-_graph->Starts[i],mask, seed);
    }
  }
  else if( _ordering.compare("LLF") == 0 ) {
    cilk_for( unsigned int i = 0; i < N; i++ ) { // cilk_for
      prefetchVertexInit(_graph, _vertices, _tournamentArray, i, true);    
      _vertices[i].vertexInitLLF(_graph, _tournamentArray, i, (unsigned int *)_graph->ColIds, _graph->Starts[i], _graph->Starts[i+1]-_graph->Starts[i],shiftAmount,seed);
    }
  }
  else if( _ordering.compare("SLL") == 0 ) {
    const unsigned int logJobs = logBaseTwoFloor(1 << ((logV+1)>>1));
    //const unsigned int logJobs = 8;//logFloor(1 << ((logV+1)>>1));
    unsigned int verticesPerJob = N >> logJobs;
    unsigned int maxLogDegree = logFloor(N + 1);
    int *degrees = (int *) malloc(sizeof(int)*N);
    unsigned int *priorities = (unsigned int *) malloc(sizeof(unsigned int)*N);
    unsigned int **queuedUpSize = (unsigned int **) malloc(sizeof(unsigned int *)*(1 << logJobs));
    unsigned int **queuedUpIndex = (unsigned int **) malloc(sizeof(unsigned int *)*(1 << logJobs));
    unsigned int ***queuedUp = (unsigned int ***) malloc(sizeof(unsigned int **)*(1 << logJobs));
    unsigned int **overflow = (unsigned int **) malloc(sizeof(unsigned int *)*(1 << logJobs));
    unsigned int *overflowSize = (unsigned int *) malloc(sizeof(unsigned int)*(1 << logJobs));
    unsigned int *overflowIndex = (unsigned int *) malloc(sizeof(unsigned int)*(1 << logJobs));
    for( unsigned int i = 0; i < (1 << logJobs); i++ ) {
      unsigned int totalVertices = verticesPerJob;
      if( i == ((1 << logJobs) - 1) ) totalVertices = N - verticesPerJob*i;  
      queuedUp[i] = (unsigned int **) malloc(sizeof(unsigned int **)*(maxLogDegree+1));
      queuedUpSize[i] = (unsigned int *) malloc(sizeof(unsigned int *)*(maxLogDegree+1));
      queuedUpIndex[i] = (unsigned int *) malloc(sizeof(unsigned int *)*(maxLogDegree+1));
      overflow[i] = (unsigned int *) malloc(sizeof(unsigned int)*totalVertices);
      overflowSize[i] = totalVertices;
      overflowIndex[i] = 0;
      for( unsigned int j = 0; j < (maxLogDegree+1); j++ ) {
        queuedUp[i][j] = (unsigned int *) malloc(sizeof(unsigned int *)*totalVertices);
        queuedUpSize[i][j] = totalVertices;
        queuedUpIndex[i][j] = 0;
      }
    }
    cilk_for( int i = 0; i < (1 << logJobs); i++ ) {
      unsigned int totalVertices = verticesPerJob;
      if( i == (1 << logJobs) - 1 ) totalVertices = N - verticesPerJob*i;  
      for( int j = 0; j < totalVertices; j++ ) {
        int vertexID = i*verticesPerJob + j;
        degrees[vertexID] = getDegree(_graph, vertexID);
        priorities[vertexID] = 0;
        if( degrees[vertexID] == 0 ) priorities[vertexID] = 1;
        else {
          int logDegree = logFloor(degrees[vertexID] + 1);
          queuedUp[i][logDegree][queuedUpIndex[i][logDegree]] = vertexID;
          queuedUpIndex[i][logDegree]++;
        }
      }
    }
    unsigned int priority = 0;
    unsigned int iterations = 0;
    for( unsigned int round = 0; round <= maxLogDegree; round++ ) {
      bool moreVerticesToProcess = true;
      iterations = 0;
      const int MAX_ROUND_ITERATIONS = 64;
      while( moreVerticesToProcess && ( iterations < MAX_ROUND_ITERATIONS ) ) {
        priority++;
        iterations++;
        unsigned int nextRound = (iterations == MAX_ROUND_ITERATIONS) ? round+1 : round;
        moreVerticesToProcess = false;
        cilk_for( int i = 0; i < (1 << logJobs); i++ ) {
          unsigned int *tmpOverflow = overflow[i];
          unsigned int tmpOverflowSize = overflowSize[i];
          overflow[i] = queuedUp[i][round];
          overflowSize[i] = queuedUpSize[i][round];
          overflowIndex[i] = queuedUpIndex[i][round];
          queuedUp[i][round] = tmpOverflow;
          queuedUpSize[i][round] = tmpOverflowSize;
          queuedUpIndex[i][round] = 0;
          const bool PRIORITY_PREFETCH = true;
          const unsigned int PRIORITY_PREFETCH_DISTANCE = 8;
          const unsigned int PRIORITY_HALF_PREFETCH_DISTANCE = 4;
          const unsigned int PRIORITY_QUARTER_PREFETCH_DISTANCE = 2;
          if( PRIORITY_PREFETCH ) {
            for( unsigned int j = 0; j < MIN(PRIORITY_PREFETCH_DISTANCE,overflowIndex[i]); j++ ) {
              __builtin_prefetch(&priorities[overflow[i][j]],0,0);
            }
          }
          for( unsigned int j = 0; j < overflowIndex[i]; j++ ) {
            if( PRIORITY_PREFETCH ) {
              if( j + PRIORITY_PREFETCH_DISTANCE < overflowIndex[i] ) {
                unsigned int prefetchVertexID = overflow[i][j+PRIORITY_PREFETCH_DISTANCE];
                __builtin_prefetch(&priorities[prefetchVertexID],0,0);
                __builtin_prefetch(&_graph->Starts[prefetchVertexID],0,0);
              }
              if( j + PRIORITY_HALF_PREFETCH_DISTANCE < overflowIndex[i] ) {
                unsigned int prefetchVertexID = overflow[i][j+PRIORITY_HALF_PREFETCH_DISTANCE];
                if( priorities[prefetchVertexID] == 0 ) {
                  __builtin_prefetch(&_graph->ColIds[_graph->Starts[prefetchVertexID]],0,0);
                }
              }
              if( j + PRIORITY_QUARTER_PREFETCH_DISTANCE < overflowIndex[i] ) {
                unsigned int prefetchVertexID = overflow[i][j+PRIORITY_QUARTER_PREFETCH_DISTANCE];
                if( priorities[prefetchVertexID] == 0 ) {
                  int *nbrs = &_graph->ColIds[_graph->Starts[prefetchVertexID]];
                  for( unsigned int k = 0; k < MIN(PRIORITY_PREFETCH_DISTANCE,getDegree(_graph, prefetchVertexID)); k++ ) {
                    __builtin_prefetch(&degrees[nbrs[k]],1,1);
                    __builtin_prefetch(&priorities[nbrs[k]],0,0);
                  }
                }                
              }
            }
            unsigned int vertexID = overflow[i][j];
            if( priorities[vertexID] == 0 ) {
              priorities[vertexID] = priority;
              int *nbrs = &_graph->ColIds[_graph->Starts[vertexID]];
              unsigned int vertexDegree = getDegree(_graph, vertexID);
              for( unsigned int k = 0; k < vertexDegree; k++ ) {
                if( PRIORITY_PREFETCH ) {
                  if( k + PRIORITY_PREFETCH_DISTANCE < vertexDegree ) {
                    __builtin_prefetch(&degrees[nbrs[k+PRIORITY_PREFETCH_DISTANCE]],1,1);
                    __builtin_prefetch(&priorities[nbrs[k+PRIORITY_PREFETCH_DISTANCE]],0,0);
                  }
                }
                if( priorities[nbrs[k]] == 0 ) {
                  int newDegree = __sync_sub_and_fetch(&degrees[nbrs[k]],1);
                  if( (logFloor(newDegree+1) != logFloor(newDegree)) 
                      && priorities[nbrs[k]] == 0 ) {
                    int logDegree = logFloor(newDegree);
                    if( newDegree == 0 ) logDegree = 0; 
                    if( logDegree >= nextRound ) {
                      if( logDegree == nextRound ) {
                        //degrees[nbrs[k]] = 0;
                        moreVerticesToProcess = true;
                      }
                      if( queuedUpIndex[i][logDegree] >= queuedUpSize[i][logDegree] ) {
                        //printf("resize: %u/%u/%u/%u\n", i,logDegree,queuedUpIndex[i][logDegree],queuedUpSize[i][logDegree]);
                        unsigned int *tmpBuffer = (unsigned int *) malloc(sizeof(unsigned int)*queuedUpSize[i][logDegree]*2);
                        for( unsigned int tmp_i = 0; tmp_i < queuedUpIndex[i][logDegree]; tmp_i++ ) {
                          tmpBuffer[tmp_i] = queuedUp[i][logDegree][tmp_i];
                        }
                        free(queuedUp[i][logDegree]);
                        queuedUp[i][logDegree] = tmpBuffer;
                        queuedUpSize[i][logDegree] = 2*queuedUpSize[i][logDegree];
                      }
                      queuedUp[i][logDegree][queuedUpIndex[i][logDegree]] = nbrs[k];
                      queuedUpIndex[i][logDegree]++;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
    cilk_for( unsigned int i = 0; i < N; i++ ) { // cilk_for
      prefetchVertexInit(_graph, _vertices, _tournamentArray, i, true);    
      _vertices[i].vertexInitSLL(_tournamentArray, i, (unsigned int *)_graph->ColIds, _graph->Starts[i], _graph->Starts[i+1]-_graph->Starts[i],priorities,mask, seed);
    }
    for( unsigned int i = 0; i < (1 << logJobs); i++ ) {
      for( unsigned int j = 0; j < (maxLogDegree+1); j++ ) {
        free(queuedUp[i][j]);
      }
      free(queuedUp[i]);
      free(queuedUpSize[i]);
      free(queuedUpIndex[i]);
      free(overflow[i]);
    }
    free(queuedUp);
    free(overflowSize);
    free(overflowIndex);
  } 
  else if( _ordering.compare("SL") == 0 ) {
    unsigned int *priorities = (unsigned int *) malloc(sizeof(unsigned int)*N);
    calculateSLPriorities(_graph, priorities);
    cilk_for( unsigned int i = 0; i < N; i++ ) { // cilk_for
      prefetchVertexInit(_graph, _vertices, _tournamentArray, i, true);    
      _vertices[i].vertexInitSL(_tournamentArray, i, (unsigned int *)_graph->ColIds, _graph->Starts[i], _graph->Starts[i+1]-_graph->Starts[i],priorities,mask,seed);
    }
  }
  else {
    printf("no such ordering\n");
  }
}

void JP(
  sparseRowMajor<int,int>* _graph, 
  Vertex *_vertices, 
  unsigned char *_tournamentArray)
{
  cilk_for( int i = 0; i < _graph->numRows; i++ ) { // cilk_for
    if( _vertices[i].numPredecessors == 0 ) {
      int degree = _graph->Starts[i+1] - _graph->Starts[i];
      _vertices[i].colorVertex(_vertices, i, _tournamentArray, (unsigned int *)_graph->ColIds, 1L);
    }
  }
}

// this version of colorGraphJP doesn't internally allocate memory for the Vertex array or the tournament array
// this is the version expected to be used for benchmarking (where one may prefer to account
// memory allocation separately) or for research considering multiple different colorings of the
// same graph
void colorGraphJP(
  sparseRowMajor<int,int>* _graph, 
  Vertex *_vertices, 
  unsigned char *_tournamentArray, 
  std::string _ordering, 
  unsigned int _seed)
{
  colorGraphJPInit(_graph, _vertices, _tournamentArray, _ordering, _seed);
  JP(_graph, _vertices, _tournamentArray);
}
