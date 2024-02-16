#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>

extern "C"
{
    

    void sort(
        float *viewProj, float *transforms,
        uint32_t *transformIndices, uint32_t vertexCount,
        float *positions, uint8_t *chunks,
        uint32_t *depthBuffer, uint32_t *depthIndex,
        uint32_t *starts, uint32_t *counts, uint32_t *outs)
    {
        int32_t minDepth = 0x7fffffff;
        int32_t maxDepth = 0x80000000;
        int32_t previousTransformIndex = -1;
        float viewTransform[16];

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

            float x = positions[3 * i + 0];
            float y = positions[3 * i + 1];
            float z = positions[3 * i + 2];

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

            float projectedZ = viewTransform[2] * x + viewTransform[6] * y + viewTransform[10] * z + viewTransform[14];

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
            

            float projectedX = viewTransform[0] * x + viewTransform[4] * y + viewTransform[8] * z + viewTransform[12];
            float projectedY = viewTransform[1] * x + viewTransform[5] * y + viewTransform[9] * z + viewTransform[13];
            float projectedW = viewTransform[3] * x + viewTransform[7] * y + viewTransform[11] * z + viewTransform[15];
            uint8_t chunk = 0xff;
            bool reject = true;
            if (projectedW != 0)
            {
                
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
}
