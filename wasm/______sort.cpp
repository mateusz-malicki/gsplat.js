#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <wasm_simd128.h>
#include <smmintrin.h> // SSE
#include<pthread.h>
#include<thread>
#include <algorithm>


/**
 * Generic multithreaded Merge sort.
 * @tparam T - Generic type argument.
 * @param arr - Array of elements.
 * @param size - Size of array.
 * @param depth - 2^depth determines number of threads
 *                (For example if depth = 3, then there will be 8 threads working on sorting.).
 */
template<class T>
void mergeSortMT(T arr[], int size, int max_depth = 2);

//Auxiliary function
template<class T>
void mergeSortAuxMT(T* arr, T* temp, int size, int cur_depth, int max_depth);


//Definitions

template<class T>
static inline void fillTheRest(T* dst, int dstIndex,T* src, int srcIndex, int size) {
  while (srcIndex < size){
    dst[dstIndex] = src[srcIndex];
    dstIndex++;
    srcIndex++;
  }
}

template<class T>
void static merge(T a[], int sizeA, T b[], int sizeB, T c[]){
  int ia = 0, ib = 0, ic = 0;
  while (ia < sizeA && ib < sizeB){
    if (a[ia] <= b[ib]){
      c[ic] = a[ia];
      ia++;
    }
    else{
      c[ic] = b[ib];
      ib++;
    }
    ic++;
  }
  fillTheRest(c,ic,a,ia,sizeA);
  fillTheRest(c,ic,b,ib,sizeB);
}

template<class T>
void mergeSortAuxMT(T* arr, T* temp, int size, int cur_depth, int max_depth){
  if (size < 2) {
    return;
  }
  int left = size/2;
  if (cur_depth <= max_depth) {
    std::thread t(mergeSortAuxMT<T>, arr, temp, left, cur_depth+1, max_depth);
    std::thread t1(mergeSortAuxMT<T>, (arr + left), temp+left, (size - left), cur_depth+1, max_depth);
    //wait for threads to finish
    t.join();
    t1.join();
  } else {
    mergeSortAuxMT(arr, temp, left, cur_depth+1, max_depth);
    mergeSortAuxMT(arr + left, temp+left, (size - left), cur_depth+1, max_depth);
  }
  merge(arr,left,arr+left,size-left,temp);
  memcpy(arr, temp,size*sizeof(T));
}

template<class T>
void mergeSortMT(T arr[], int size, int max_depth){
  T *temp = new T[size];
  //in case of allocation error will throw bad_alloc
  mergeSortAuxMT(arr, temp, size, 1, max_depth);
  delete[] temp;
}

extern "C"
{

    const int xOffset = 0;
    const int yOffset = 1;
    const int zOffset = 2; //
    const int wOffset = 3; //
    const float depthToLarge = 0x7fffffff / 4096 - 1;

    void sortIntegers(
        float *viewProj,
        float *transforms,
        uint32_t *transformIndices,
        uint32_t vertexCount,
        int *positions,
        uint8_t *chunks,
        uint32_t *depthBuffer,
        uint32_t *depthIndex,
        uint32_t *starts,
        uint32_t *counts,
        uint32_t *outs)
    {
        int32_t minDepth = 0x7fffffff;
        int32_t maxDepth = 0x80000000;
        int32_t previousTransformIndex = -1;

        float floatToIntMultiplier = 1000.0;

        float viewTransform[16];
        float intViewTransform[16];

        std::vector<uint32_t> indexMap = {};
        
        int indexCount = 0;
        float depthToLarge = 0x7fffffff / 4096 - 1;

        for (uint32_t i = 0; i < vertexCount; i++)
        {
            // if fast
            // if (i % 3 == 0)
            // {
            //     continue;
            // }

            int x = positions[3 * i + 0];
            int y = positions[3 * i + 1];
            int z = positions[3 * i + 2];

            uint32_t transformIndex = transformIndices[i];
            bool computeViewTransform = transformIndex != previousTransformIndex;
            if (computeViewTransform)
            {
                
                float *transform = &transforms[20 * transformIndex];
                //viewTransform[0] = transform[0] * viewProj[0] + transform[1] * viewProj[4] + transform[2] * viewProj[8] + transform[3] * viewProj[12];
                //viewTransform[1] = transform[0] * viewProj[1] + transform[1] * viewProj[5] + transform[2] * viewProj[9] + transform[3] * viewProj[13];
                viewTransform[2] = transform[0] * viewProj[2] + transform[1] * viewProj[6] + transform[2] * viewProj[10] + transform[3] * viewProj[14];
                //viewTransform[3] = transform[0] * viewProj[3] + transform[1] * viewProj[7] + transform[2] * viewProj[11] + transform[3] * viewProj[15];
                //viewTransform[4] = transform[4] * viewProj[0] + transform[5] * viewProj[4] + transform[6] * viewProj[8] + transform[7] * viewProj[12];
                //viewTransform[5] = transform[4] * viewProj[1] + transform[5] * viewProj[5] + transform[6] * viewProj[9] + transform[7] * viewProj[13];
                viewTransform[6] = transform[4] * viewProj[2] + transform[5] * viewProj[6] + transform[6] * viewProj[10] + transform[7] * viewProj[14];
                //viewTransform[7] = transform[4] * viewProj[3] + transform[5] * viewProj[7] + transform[6] * viewProj[11] + transform[7] * viewProj[15];
                //viewTransform[8] = transform[8] * viewProj[0] + transform[9] * viewProj[4] + transform[10] * viewProj[8] + transform[11] * viewProj[12];
                //viewTransform[9] = transform[8] * viewProj[1] + transform[9] * viewProj[5] + transform[10] * viewProj[9] + transform[11] * viewProj[13];
                viewTransform[10] = transform[8] * viewProj[2] + transform[9] * viewProj[6] + transform[10] * viewProj[10] + transform[11] * viewProj[14];
                //viewTransform[11] = transform[8] * viewProj[3] + transform[9] * viewProj[7] + transform[10] * viewProj[11] + transform[11] * viewProj[15];
                //viewTransform[12] = transform[12] * viewProj[0] + transform[13] * viewProj[4] + transform[14] * viewProj[8] + transform[15] * viewProj[12];
                //viewTransform[13] = transform[12] * viewProj[1] + transform[13] * viewProj[5] + transform[14] * viewProj[9] + transform[15] * viewProj[13];
                viewTransform[14] = transform[12] * viewProj[2] + transform[13] * viewProj[6] + transform[14] * viewProj[10] + transform[15] * viewProj[14];
                //viewTransform[15] = transform[12] * viewProj[3] + transform[13] * viewProj[7] + transform[14] * viewProj[11] + transform[15] * viewProj[15];
            }
            
                int intViewTransform3Rows[4] = {
                    (int)(floatToIntMultiplier * viewTransform[2]),
                    (int)(floatToIntMultiplier * viewTransform[6]),
                    (int)(floatToIntMultiplier * viewTransform[10]),
                    (int)(floatToIntMultiplier * viewTransform[14])
                };
                int _positions[4] = { x, y, z, 1 };
                //v128_t _vt = wasm_v128_load(&intViewTransform3Rows[0]);
                //v128_t _pos = wasm_v128_load(&_positions[0]);
                
            
            
            int projectedZ = viewTransform[2] * intViewTransform3Rows[0]
            + viewTransform[6] * intViewTransform3Rows[1]
            + viewTransform[10] * intViewTransform3Rows[2]
            + viewTransform[14];

            if (projectedZ <= 0 || projectedZ >= depthToLarge)
            {
                continue;
            }

            if (computeViewTransform)
            {
                float *transform = &transforms[20 * transformIndex];
                viewTransform[0] = transform[0] * viewProj[0] + transform[1] * viewProj[4] + transform[2] * viewProj[8] + transform[3] * viewProj[12];
                viewTransform[1] = transform[0] * viewProj[1] + transform[1] * viewProj[5] + transform[2] * viewProj[9] + transform[3] * viewProj[13];
                //viewTransform[2] = transform[0] * viewProj[2] + transform[1] * viewProj[6] + transform[2] * viewProj[10] + transform[3] * viewProj[14];
                viewTransform[3] = transform[0] * viewProj[3] + transform[1] * viewProj[7] + transform[2] * viewProj[11] + transform[3] * viewProj[15];
                viewTransform[4] = transform[4] * viewProj[0] + transform[5] * viewProj[4] + transform[6] * viewProj[8] + transform[7] * viewProj[12];
                viewTransform[5] = transform[4] * viewProj[1] + transform[5] * viewProj[5] + transform[6] * viewProj[9] + transform[7] * viewProj[13];
                //viewTransform[6] = transform[4] * viewProj[2] + transform[5] * viewProj[6] + transform[6] * viewProj[10] + transform[7] * viewProj[14];
                viewTransform[7] = transform[4] * viewProj[3] + transform[5] * viewProj[7] + transform[6] * viewProj[11] + transform[7] * viewProj[15];
                viewTransform[8] = transform[8] * viewProj[0] + transform[9] * viewProj[4] + transform[10] * viewProj[8] + transform[11] * viewProj[12];
                viewTransform[9] = transform[8] * viewProj[1] + transform[9] * viewProj[5] + transform[10] * viewProj[9] + transform[11] * viewProj[13];
                //viewTransform[10] = transform[8] * viewProj[2] + transform[9] * viewProj[6] + transform[10] * viewProj[10] + transform[11] * viewProj[14];
                viewTransform[11] = transform[8] * viewProj[3] + transform[9] * viewProj[7] + transform[10] * viewProj[11] + transform[11] * viewProj[15];
                viewTransform[12] = transform[12] * viewProj[0] + transform[13] * viewProj[4] + transform[14] * viewProj[8] + transform[15] * viewProj[12];
                viewTransform[13] = transform[12] * viewProj[1] + transform[13] * viewProj[5] + transform[14] * viewProj[9] + transform[15] * viewProj[13];
                //viewTransform[14] = transform[12] * viewProj[2] + transform[13] * viewProj[6] + transform[14] * viewProj[10] + transform[15] * viewProj[14];
                viewTransform[15] = transform[12] * viewProj[3] + transform[13] * viewProj[7] + transform[14] * viewProj[11] + transform[15] * viewProj[15];

                previousTransformIndex = transformIndex;
            }
            

            
            float projectedW = viewTransform[3] * x + viewTransform[7] * y + viewTransform[11] * z + viewTransform[15];
            uint8_t chunk = 0xff;
            bool reject = true;
            if (projectedW != 0)
            {
                float projectedX = viewTransform[0] * x + viewTransform[4] * y + viewTransform[8] * z + viewTransform[12];
                float projectedY = viewTransform[1] * x + viewTransform[5] * y + viewTransform[9] * z + viewTransform[13];
                
                float normalizedX = (projectedX / projectedW + 1) / 2;
                float normalizedY = (projectedY / projectedW + 1) / 2;
                if (normalizedX >= 0 && normalizedX < 1 && normalizedY >= 0 && normalizedY < 1)
                {
                    uint8_t screenSpaceX = (uint8_t)(normalizedX * 15);
                    uint8_t screenSpaceY = (uint8_t)(normalizedY * 15);
                    //chunk = screenSpaceX + screenSpaceY * 15;
                    
                    reject = false;
                    if (screenSpaceY == 0 || screenSpaceX == 0
                    || screenSpaceY == 1  || screenSpaceX == 1
                    || screenSpaceY == 15  || screenSpaceX == 15
                    || screenSpaceY == 14  || screenSpaceX == 14)
                    {
                        reject = true;
                    }
                    
                }
            }
            //chunks[i] = chunk;

            if (reject == false)
            {
                int32_t depth = projectedZ * 4096;
                indexMap.push_back(i);
                //indexMap[indexCount] = i;
                depthBuffer[indexCount] = depth;
                indexCount++;
                if (depth > maxDepth)
                {
                    maxDepth = depth;
                }
                if (depth < minDepth)
                {
                    minDepth = depth;
                }
            }
            
            
        }

        uint32_t minDepthRange =  64 * 256;
        uint32_t maxDepthRange = 256 * 256 * 16;
        uint32_t depthRange = (maxDepth - minDepth);
        if (depthRange < minDepthRange)
        {
            depthRange = minDepthRange;
        }
        else if(depthRange > maxDepthRange)
        {
            depthRange = maxDepthRange;
        }
         
        const float depthInv = (float)(depthRange - 1) / (maxDepth - minDepth);
        
        
        memset(counts, 0, depthRange * sizeof(uint32_t));
        int skipped = 0;
        for (uint32_t i = 0; i < indexCount; i++)
        {
            float _d = (depthBuffer[i] - minDepth) * depthInv;
            if (_d <= 0 || _d >= depthRange)
            {
                _d = 0.0;
                skipped++;
            }
            depthBuffer[i] = _d;
            counts[depthBuffer[i]]++;
        }

        starts[skipped] = 0;
        for (uint32_t i = skipped+1; i < depthRange; i++)
        {
            starts[i] = starts[i - 1] + counts[i - 1];
        }

        for (uint32_t i = 0; i < indexCount; i++)
        {
            depthIndex[-skipped + (starts[depthBuffer[i]]++)] = indexMap[i];
        }
        

        /*
        for (uint32_t i = 0; i < indexCount; i++)
        {
            depthBuffer[i] = (depthBuffer[i] - minDepth) * depthInv;
        }
        std:sort(indexMap.begin(), indexMap.end(),
        [depthBuffer](uint32_t i1, uint32_t i2) {
            return depthBuffer[i1] < depthBuffer[i2];
        });
        for (uint32_t i = 0; i < indexCount; i++)
        {
            depthIndex[i] = indexMap[i];
        }
        */
        outs[0] = (indexCount - skipped);
        // memset(depthIndex + (indexCount - skipped) * sizeof(uint32_t), 0xFFFFFFFF,
        //       (vertexCount - indexCount + skipped) * sizeof(uint32_t));
    }

    

    float computeProjectedValue(__m128 t0, __m128 t1, __m128 t2, __m128 t3, const float* viewProj, __m128 xyzw, int offset)
    {
        __m128 vp1 = _mm_set1_ps(viewProj[0 + offset]);
        __m128 vp2 = _mm_set1_ps(viewProj[4 + offset]);
        __m128 vp3 = _mm_set1_ps(viewProj[8 + offset]);
        __m128 vp4 = _mm_set1_ps(viewProj[12 + offset]);

        __m128 vt = _mm_add_ps(
                        _mm_add_ps(_mm_mul_ps(t0, vp1), _mm_mul_ps(t1, vp2)),
                        _mm_add_ps(_mm_mul_ps(t2, vp3), _mm_mul_ps(t3, vp4)));

        __m128 dpResult = _mm_dp_ps(vt, xyzw, 0xF1); 

        return _mm_cvtss_f32(dpResult);
    }

    struct indexDepthTuple
    {
        uint32_t index;
        int32_t depth;

        const inline bool operator<(const indexDepthTuple& other) const {
            return (depth < other.depth);
        };
        const inline bool operator<=(const indexDepthTuple& other) const {
            return (depth <= other.depth);
        };
        const inline bool operator>(const indexDepthTuple& other) const {
            return (depth > other.depth);
        };
        const inline bool operator>=(const indexDepthTuple& other) const {
            return (depth >= other.depth);
        };
        const inline bool operator==(const indexDepthTuple& other) const {
            return (depth == other.depth);
        };
    
    };
    struct computeDepthData
    {
        float *viewProj;
        float *transforms;
        uint32_t *transformIndices;
        float *positions;
        std::vector<indexDepthTuple> tuples;
        uint32_t start;
        uint32_t end;
        int thread_part;
    };


    void *computeDepths(void *input)
    {
        int indexCount = 0;
        float *viewProj = ((struct computeDepthData*)input)->viewProj;
        float *transforms = ((struct computeDepthData*)input)->transforms;
        uint32_t *transformIndices = ((struct computeDepthData*)input)->transformIndices;
        uint32_t start = ((struct computeDepthData*)input)->start;
        uint32_t end = ((struct computeDepthData*)input)->end;
  
        float *positions = ((struct computeDepthData*)input)->positions;
        std::vector<indexDepthTuple> tuples = ((struct computeDepthData*)input)->tuples;
        for (uint32_t i = start; i < end; i++)
        {
            bool reject = true;
            //depthBuffer[i] = 0;

            uint32_t transformIndex = transformIndices[i];
            float *transform = &transforms[20 * transformIndex];

            float x = positions[3 * i + 0];
            float y = positions[3 * i + 1];
            float z = positions[3 * i + 2];

            __m128 xyzw = _mm_set_ps(1.0f, z, y, x);
            __m128 t0 = _mm_load_ps(&transform[0]);  
            __m128 t1 = _mm_load_ps(&transform[4]);  
            __m128 t2 = _mm_load_ps(&transform[8]);  
            __m128 t3 = _mm_load_ps(&transform[12]); 

            float projectedZ = computeProjectedValue(t0, t1, t2, t3,  viewProj, xyzw, zOffset);

            if (projectedZ <= 0 || projectedZ >= depthToLarge)
            {
                continue;
            }
            else
            {

            }
            float projectedW = 0.77 * computeProjectedValue(t0, t1, t2, t3, viewProj, xyzw, wOffset);
            

            if (projectedW <= 0)
            {
                continue;
            }
            else
            {
                float projectedX = computeProjectedValue(t0, t1, t2, t3, viewProj, xyzw, xOffset);
                
                if (projectedX >= projectedW || projectedX <= -projectedW)
                {
                    continue;
                }
                else
                {
                    float projectedY = computeProjectedValue(t0, t1, t2, t3, viewProj, xyzw, yOffset);
                    if (projectedY >= projectedW || projectedY <= -projectedW)
                    {
                        continue;
                    }
                    else
                    {
                        reject = false;
                    };
                    
                }
            }

            if (reject == false)
            {
                int32_t depth = projectedZ * 4096;
                //indexMap.push_back(i);
                indexDepthTuple tuple;
                tuple.index = i;
                tuple.depth = depth;
                tuples.push_back(tuple);
                //depthBuffer[indexCount] = depth;
                //depthBuffer[i] = depth;
                indexCount++;
                // if (depth > maxDepth)
                // {
                //     maxDepth = depth;
                // }
                // if (depth < minDepth)
                // {
                //     minDepth = depth;
                // }
            }
        }

    };

    void sort(
        float *viewProj,
        float *transforms,
        uint32_t *transformIndices,
        uint32_t vertexCount,
        float *positions,
        uint8_t *chunks,
        uint32_t *depthBuffer,
        uint32_t *depthIndex,
        uint32_t *starts,
        uint32_t *counts,
        uint32_t *outs)
    {
        int32_t minDepth = 0x7fffffff;
        int32_t maxDepth = 0x80000000;
        int32_t previousTransformIndex = -1;

        float floatToIntMultiplier = 1000.0;

        float viewTransform[16];

        std::vector<uint32_t> indexMap = {};
        
        float depthToLarge = 0x7fffffff / 4096 - 1;
        int marginCount = 0;

        pthread_t thread1, thread2;

        computeDepthData td1, td2;

        /*
        float *viewProj,
        float *transforms,
        uint32_t *transformIndices,
        float *positions,
        std::vector<indexDepthTuple> *tuples,
        uint32_t start,
        uint32_t end,
        int thread_part
        */
        td1.viewProj = viewProj;
        td1.transforms = transforms;
        td1.transformIndices = transformIndices;
        td1.positions = positions;
        td1.start = 0;
        td1.end = vertexCount / 2;
        td1.thread_part = 0;

        td2.viewProj = viewProj;
        td2.transforms = transforms;
        td2.transformIndices = transformIndices;
        td2.positions = positions;
        td2.start = vertexCount / 2;
        td2.end = vertexCount;
        td2.thread_part = 1;


         pthread_create(&thread1, NULL, *computeDepths, (void *) &td1);
         pthread_create(&thread2, NULL, *computeDepths, (void *) &td2);
         // wait for threads to finish
         pthread_join(thread1,NULL);
         pthread_join(thread2,NULL);

        int indexCount = td1.tuples.size() + td2.tuples.size();
        indexDepthTuple arr[ indexCount ];
        copy(td1.tuples.begin(), td1.tuples.end(), arr);
        copy(td2.tuples.begin(), td2.tuples.end(), &arr[td1.tuples.size()] );
        mergeSortMT(arr, 2);

        

        

         for (uint32_t i = 0; i < indexCount; i++)
         {
             depthIndex[i] = arr[i].index;
         }
        
        outs[0] = (indexCount);
    }
}
