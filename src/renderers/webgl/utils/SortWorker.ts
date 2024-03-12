import loadWasm from "../../../wasm/sort";

// eslint-disable-next-line @typescript-eslint/no-explicit-any
let wasmModule: any;

async function initWasm() {
    wasmModule = await loadWasm();
}

let sortData: {
    positions: Float32Array | Int32Array;
    transforms: Float32Array;
    transformIndices: Uint32Array;
    vertexCount: number;
    useIntPositions: boolean;
};

let viewProjPtr: number;
let transformsPtr: number;
let transformIndicesPtr: number;
let positionsPtr: number;
let chunksPtr: number;
let depthBufferPtr: number;
let depthIndexPtr: number;
let startsPtr: number;
let indexMapPtr: number;
let countsPtr: number;
let outsPtr: number;
let useIntPositions: boolean = true;

let allocatedVertexCount: number = 0;
let allocatedTransformCount: number = 0;
let viewProj: Float32Array = new Float32Array(16);
let margin: number = 20;

let lock = false;
let allocationPending = false;
let sorting = false;

let first = true;

const allocateBuffers = async () => {
    if (!wasmModule) await initWasm();

    const targetAllocatedVertexCount = Math.pow(2, Math.ceil(Math.log2(sortData.vertexCount)));
    if (allocatedVertexCount < targetAllocatedVertexCount) {
        if (allocatedVertexCount > 0) {
            wasmModule._free(viewProjPtr);
            wasmModule._free(transformIndicesPtr);
            wasmModule._free(positionsPtr);
            wasmModule._free(chunksPtr);
            wasmModule._free(depthBufferPtr);
            wasmModule._free(depthIndexPtr);
            wasmModule._free(startsPtr);
            wasmModule._free(countsPtr);
            wasmModule._free(indexMapPtr);
        }

        allocatedVertexCount = targetAllocatedVertexCount;

        viewProjPtr = wasmModule._malloc(16 * 4);
        transformIndicesPtr = wasmModule._malloc(allocatedVertexCount * 4);
        positionsPtr = wasmModule._malloc(3 * allocatedVertexCount * 4);
        chunksPtr = wasmModule._malloc(allocatedVertexCount);
        depthBufferPtr = wasmModule._malloc(allocatedVertexCount * 4);
        depthIndexPtr = wasmModule._malloc(allocatedVertexCount * 4);
        startsPtr = wasmModule._malloc(allocatedVertexCount * 4);
        //countsPtr = wasmModule._malloc(allocatedVertexCount * 4);
        countsPtr = wasmModule._malloc(allocatedVertexCount * 4);
        indexMapPtr = wasmModule._malloc(allocatedVertexCount * 4);
        outsPtr = wasmModule._malloc(16 * 4);
    }

    if (allocatedTransformCount < sortData.transforms.length) {
        if (allocatedTransformCount > 0) {
            wasmModule._free(transformsPtr);
        }

        allocatedTransformCount = sortData.transforms.length;

        transformsPtr = wasmModule._malloc(allocatedTransformCount * 4);
    }

    useIntPositions = sortData.useIntPositions;
    wasmModule.HEAPF32.set(sortData.positions, positionsPtr / 4);
    wasmModule.HEAPF32.set(sortData.transforms, transformsPtr / 4);
    wasmModule.HEAPU32.set(sortData.transformIndices, transformIndicesPtr / 4);

    wasmModule._cleanUp(countsPtr);

    first = true;
};

const runSort = () => {
    
    wasmModule.HEAPF32.set(viewProj, viewProjPtr / 4);

    console.time('wasm.sort');

    wasmModule._sort(
        viewProjPtr,
        transformsPtr,
        transformIndicesPtr,
        sortData.vertexCount,
        positionsPtr,
        chunksPtr,
        depthBufferPtr,
        depthIndexPtr,
        startsPtr,
        countsPtr,
        outsPtr,
        indexMapPtr,
        margin
    );

    console.timeEnd('wasm.sort');

    const outs = new Uint32Array(wasmModule.HEAPU32.buffer, outsPtr, 4);
    const count = outs[0];

    //const depthIndex = new Uint32Array(wasmModule.HEAPU32.buffer, depthIndexPtr, sortData.vertexCount);
    const depthIndex = first ? wasmModule.HEAP32.subarray(depthIndexPtr / 4, depthIndexPtr / 4 + sortData.vertexCount)
    : wasmModule.HEAP32.subarray(depthIndexPtr / 4, depthIndexPtr / 4 + count);
    const detachedDepthIndex = new Uint32Array(depthIndex);
    //const detachedDepthIndex = first ? new Uint32Array(depthIndex.slice().buffer) : new Uint32Array(depthIndex.slice(0, count).buffer);
    // const detachedDepthIndex = first ? wasmModule.HEAP32.subarray(depthIndexPtr / 4, depthIndexPtr / 4 + sortData.vertexCount)
    // : wasmModule.HEAP32.subarray(depthIndexPtr / 4, depthIndexPtr / 4 + count);
    first = false;
    //const chunks = new Uint8Array(wasmModule.HEAPU8.buffer, chunksPtr, sortData.vertexCount);
    //const detachedChunks = new Uint8Array(chunks.slice().buffer);

    

    self.postMessage({ depthIndex: detachedDepthIndex, /*chunks: detachedChunks,*/ count: count }, [
        detachedDepthIndex.buffer,
        /*detachedChunks.buffer,*/
    ]);
    setTimeout(()=>{wasmModule._cleanUp(countsPtr);});
    
};


self.onmessage = async (e) => {
    if (lock) {
        throw 'sorter is working'
    } else {
        lock = true;
        if (e.data.sortData) {
            sortData = {
                positions: e.data.sortData.positions as (Float32Array | Int32Array),
                transforms: e.data.sortData.transforms as Float32Array,
                transformIndices: e.data.sortData.transformIndices as Uint32Array,
                vertexCount: e.data.sortData.vertexCount,
                useIntPositions: e.data.useIntPositions
            };
            await allocateBuffers();
        }
        if (e.data.viewProj) {
            viewProj = Float32Array.from(e.data.viewProj);
            margin = e.data.margin || 20
            runSort();
        }
        lock = false;
    }
    
};
