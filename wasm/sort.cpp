#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cmath>
// #include <vector>
// #include <wasm_simd128.h>

extern "C"
{
    void cleanUp(uint32_t *counts)
    {
        memset(counts, 0, 256 * 256 * sizeof(uint32_t));
    }
    inline bool bit_check(uint32_t number, uint32_t n) {
    return (number >> n) & (uint32_t)1;
}
    inline uint32_t bit_set_to(uint32_t number, uint32_t n, bool x) {
    return (number & ~((uint32_t)1 << n)) | ((uint32_t)x << n);
}
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
        uint32_t *outs,
        uint32_t *indexMap,
        uint32_t margin)
    {
        int32_t minDepth = 0x7fffffff;
        int32_t maxDepth = 0x80000000;
        int32_t previousTransformIndex = -1;

        float floatToIntMultiplier = 10000.0;

        float WFactor = ((100.0 + margin) / 100.0);

        float viewTransform[16];

        bool enableRejections = true;

        float ratio = 9.0/16.0;

        
        int indexCount = 0;
        float depthToLarge = 0x7fffffff / 4096 - 1;
        //int depthToLarge = 0x7fffffff - 1;/// 4096 - 1;
        int marginCount = 0;
        for (uint32_t i = 0; i < vertexCount; i++)
        {
            // if fast
            //  if (margin > 130 && i % 2 != 0)
            //  {
            //      continue;
            //  }

            float x = positions[3 * i + 0];
            float y = positions[3 * i + 1];
            float z = positions[3 * i + 2];

            bool fromMargin = false;
            bool fromFarMargin = false;

            uint32_t transformIndex = transformIndices[i];
            float *transform = &transforms[20 * transformIndex];
            bool computeViewTransform = false;//transformIndex != previousTransformIndex;

            /**/
            if (computeViewTransform)
            {
                //float *transform = &transforms[20 * transformIndex];
                viewTransform[2] = transform[0] * viewProj[2] + transform[1] * viewProj[6] + transform[2] * viewProj[10] + transform[3] * viewProj[14];
                viewTransform[6] = transform[4] * viewProj[2] + transform[5] * viewProj[6] + transform[6] * viewProj[10] + transform[7] * viewProj[14];
                viewTransform[10] = transform[8] * viewProj[2] + transform[9] * viewProj[6] + transform[10] * viewProj[10] + transform[11] * viewProj[14];
                viewTransform[14] = transform[12] * viewProj[2] + transform[13] * viewProj[6] + transform[14] * viewProj[10] + transform[15] * viewProj[14];
            }
            
            float projectedZ = viewProj[2] * x + viewProj[6] * y + viewProj[10] * z + viewProj[14];
            //float projectedZ = viewTransform[2] * x + viewTransform[6] * y + viewTransform[10] * z + viewTransform[14];

            // float projectedZ = (transform[0] * viewProj[2] + transform[1] * viewProj[6] + transform[2] * viewProj[10] + transform[3] * viewProj[14]) * x +
            //  (transform[4] * viewProj[2] + transform[5] * viewProj[6] + transform[6] * viewProj[10] + transform[7] * viewProj[14]) * y +
            //  (transform[8] * viewProj[2] + transform[9] * viewProj[6] + transform[10] * viewProj[10] + transform[11] * viewProj[14]) * z +
            //  (transform[12] * viewProj[2] + transform[13] * viewProj[6] + transform[14] * viewProj[10] + transform[15] * viewProj[14]);
            bool reject = true;
            if (enableRejections == false)
            {
                if (projectedZ < 0)
                {
                    projectedZ = 0;
                }
                else if (projectedZ >= depthToLarge)
                {
                    projectedZ = 0;//depthToLarge - 4097;
                }
                reject = false;
            }
            else
            {
                if (projectedZ <= 0 || projectedZ >= depthToLarge)
                {
                    continue;
                }

                if (computeViewTransform)
                {
                    float *transform = &transforms[20 * transformIndex];
                    //for W
                    viewTransform[3] = transform[0] * viewProj[3] + transform[1] * viewProj[7] + transform[2] * viewProj[11] + transform[3] * viewProj[15];
                    viewTransform[7] = transform[4] * viewProj[3] + transform[5] * viewProj[7] + transform[6] * viewProj[11] + transform[7] * viewProj[15];
                    viewTransform[11] = transform[8] * viewProj[3] + transform[9] * viewProj[7] + transform[10] * viewProj[11] + transform[11] * viewProj[15];
                    viewTransform[15] = transform[12] * viewProj[3] + transform[13] * viewProj[7] + transform[14] * viewProj[11] + transform[15] * viewProj[15];
                }
                float projectedW = (viewProj[3] * x + viewProj[7] * y + viewProj[11] * z + viewProj[15]);
                //float projectedW = 1.0 * (viewTransform[3] * x + viewTransform[7] * y + viewTransform[11] * z + viewTransform[15]);
                
                

                if (projectedW != 0)
                {
                    float _projectedW = projectedW * 0.6;
                    projectedW = WFactor * _projectedW;
                    if (computeViewTransform)
                    {
                        //for X
                        viewTransform[0] = transform[0] * viewProj[0] + transform[1] * viewProj[4] + transform[2] * viewProj[8] + transform[3] * viewProj[12];
                        viewTransform[4] = transform[4] * viewProj[0] + transform[5] * viewProj[4] + transform[6] * viewProj[8] + transform[7] * viewProj[12];
                        viewTransform[8] = transform[8] * viewProj[0] + transform[9] * viewProj[4] + transform[10] * viewProj[8] + transform[11] * viewProj[12];
                        viewTransform[12] = transform[12] * viewProj[0] + transform[13] * viewProj[4] + transform[14] * viewProj[8] + transform[15] * viewProj[12];
                        
                        //for Y
                        viewTransform[1] = transform[0] * viewProj[1] + transform[1] * viewProj[5] + transform[2] * viewProj[9] + transform[3] * viewProj[13];
                        viewTransform[5] = transform[4] * viewProj[1] + transform[5] * viewProj[5] + transform[6] * viewProj[9] + transform[7] * viewProj[13];
                        viewTransform[9] = transform[8] * viewProj[1] + transform[9] * viewProj[5] + transform[10] * viewProj[9] + transform[11] * viewProj[13];
                        viewTransform[13] = transform[12] * viewProj[1] + transform[13] * viewProj[5] + transform[14] * viewProj[9] + transform[15] * viewProj[13];
                        
                        previousTransformIndex = transformIndex;
                    }
                    
                    float projectedX = viewProj[0] * x + viewProj[4] * y + viewProj[8] * z + viewProj[12];
                    //float projectedX = viewTransform[0] * x + viewTransform[4] * y + viewTransform[8] * z + viewTransform[12];

                    float projectedY = 1.0 * (viewProj[1] * x + viewProj[5] * y + viewProj[9] * z + viewProj[13]);
                    //float projectedY = viewTransform[1] * x + viewTransform[5] * y + viewTransform[9] * z + viewTransform[13];

                    //if (projectedX > -projectedW && projectedX < projectedW && projectedY > -projectedW && projectedY < projectedW)
                    //if (projectedW * projectedW > projectedX * projectedX + projectedY * projectedY)
                    //float WW = projectedW * projectedW;
                    //if (WW > 2 * projectedX * projectedX)
                    //if (projectedX > -projectedW && projectedX < projectedW && projectedY > -projectedW && projectedY < projectedW)
                    //float larger = abs(projectedX) > abs(projectedY) ? projectedX * projectedX : projectedY * projectedY;


                    //float normalizedX = (projectedX / projectedW ) / 2;
                    //float normalizedY = /*9.0/16.0 */ (projectedY / projectedW ) / 2;
                    //float WW = projectedW * projectedW;
                    //float XX = normalizedX * normalizedX;
                    //float YY = normalizedY * normalizedY;
                    //if (normalizedX >= 0 && normalizedX < 1 && normalizedY >= 0 && normalizedY < 1)
                    if (projectedX > -projectedW && projectedX < projectedW && projectedY > -projectedW && projectedY < projectedW)
                    {
                        reject = false;
                        fromMargin = !(projectedX > -_projectedW && projectedX < _projectedW && projectedY > -_projectedW && projectedY < _projectedW);
                    }
                    else
                    {
                        /*
                        float xx = projectedX * projectedX;
                        float yy = ratio * projectedY * ratio * projectedY;
                        float ww = projectedW * projectedW;
                        if(xx + yy < ww * 1.42 && marginCount++ % 2 == 0)
                        {
                            reject = false;
                        }*/
                        float W2 = 10.0 * projectedW;
                        if (projectedX > -W2 && projectedX < W2 && projectedY > -W2 && projectedY < W2 && marginCount++ % 4 == 0)
                        {
                            reject = false;
                            fromMargin = true;
                            fromFarMargin = true;
                        }
                    }
                    /*
                    else 
                    {
                    projectedW = 1.6 * projectedW;
                    if (projectedX > -projectedW && projectedX < projectedW && projectedY > -projectedW && projectedY < projectedW)
                    {
                        reject = marginCount++ % 3 != 0;
                    }
                        
                    }
                    */
                }
            }
            

            if (reject == false)
            {
                int32_t depth = projectedZ * 4096;
                //indexMap.push_back(i);
                indexMap[indexCount] = i;

                if (fromMargin)
                {
                    indexMap[indexCount] = bit_set_to(i, 31, true);
                }
                if (fromFarMargin)
                {
                    indexMap[indexCount] = bit_set_to(i, 30, true);
                }

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
        uint32_t maxDepthRange = 256 * 256;
        uint32_t depthRange = (maxDepth - minDepth);
        if (depthRange < minDepthRange)
        {
            depthRange = minDepthRange;
        }
        else if(depthRange >= maxDepthRange)
        {
            depthRange = maxDepthRange;
        }
         
        const float depthInv = (float)(depthRange - 1) / (maxDepth - minDepth);
        
        
        //memset(counts, 0, depthRange * sizeof(uint32_t));
        int skipped = 0;
        for (uint32_t i = 0; i < indexCount; i++)
        {
            float _d = (depthBuffer[i] - minDepth) * depthInv;
             if (_d <= 0 || _d >= depthRange)
             {
                 //_d = 0.0; //depthRange -2;
                 skipped++;
             }
             else
             {
                depthBuffer[i] = _d;
                counts[depthBuffer[i]]++;
             }
            // if (_d < 0)
            // {
            //     _d = 0.0;
            // }
            // else if (_d >= depthRange)
            // {
            //     _d = depthRange - 1;
            // }
            
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
         //memset(depthIndex + (indexCount - skipped) * sizeof(uint32_t), 0xFFFFFFFF,
        //       (vertexCount - indexCount + skipped) * sizeof(uint32_t));
    }
}
