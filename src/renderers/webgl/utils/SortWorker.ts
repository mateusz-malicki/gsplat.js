import loadWasm from "../../../wasm/sort";

// eslint-disable-next-line @typescript-eslint/no-explicit-any
let wasmModule: any;

async function initWasm() {
    wasmModule = await loadWasm();
}

let sortData: {
    positions: Float32Array;
    transforms: Float32Array;
    transformIndices: Uint32Array;
    vertexCount: number;
};

let viewProjPtr: number;
let transformsPtr: number;
let transformIndicesPtr: number;
let positionsPtr: number;
let chunksPtr: number;
let depthBufferPtr: number;
let depthIndexPtr: number;
let startsPtr: number;
let countsPtr: number;
let outsPtr: number;

let allocatedVertexCount: number = 0;
let allocatedTransformCount: number = 0;
let viewProj: Float32Array = new Float32Array(16);

let lock = false;
let allocationPending = false;
let sorting = false;

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
        }

        allocatedVertexCount = targetAllocatedVertexCount;

        viewProjPtr = wasmModule._malloc(16 * 4);
        transformIndicesPtr = wasmModule._malloc(allocatedVertexCount * 4);
        positionsPtr = wasmModule._malloc(3 * allocatedVertexCount * 4);
        chunksPtr = wasmModule._malloc(allocatedVertexCount);
        depthBufferPtr = wasmModule._malloc(allocatedVertexCount * 4);
        depthIndexPtr = wasmModule._malloc(allocatedVertexCount * 4);
        startsPtr = wasmModule._malloc(allocatedVertexCount * 4);
        countsPtr = wasmModule._malloc(allocatedVertexCount * 4);
        outsPtr = wasmModule._malloc(16 * 4);
    }

    if (allocatedTransformCount < sortData.transforms.length) {
        if (allocatedTransformCount > 0) {
            wasmModule._free(transformsPtr);
        }

        allocatedTransformCount = sortData.transforms.length;

        transformsPtr = wasmModule._malloc(allocatedTransformCount * 4);
    }

    wasmModule.HEAPF32.set(sortData.positions, positionsPtr / 4);
    wasmModule.HEAPF32.set(sortData.transforms, transformsPtr / 4);
    wasmModule.HEAPU32.set(sortData.transformIndices, transformIndicesPtr / 4);
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
        outsPtr
    );

    console.timeEnd('wasm.sort');

    const depthIndex = new Uint32Array(wasmModule.HEAPU32.buffer, depthIndexPtr, sortData.vertexCount);
    const detachedDepthIndex = new Uint32Array(depthIndex.slice().buffer);

    //const chunks = new Uint8Array(wasmModule.HEAPU8.buffer, chunksPtr, sortData.vertexCount);
    //const detachedChunks = new Uint8Array(chunks.slice().buffer);

    const outs = new Uint32Array(wasmModule.HEAPU32.buffer, outsPtr, 4);
    const count = outs[0];

    self.postMessage({ depthIndex: detachedDepthIndex, /*chunks: detachedChunks,*/ count: count }, [
        detachedDepthIndex.buffer,
        /*detachedChunks.buffer,*/
    ]);
};


self.onmessage = async (e) => {
    if (lock) {
        throw 'sorter is working'
    } else {
        lock = true;
        if (e.data.sortData) {
            sortData = {
                positions: e.data.sortData.positions as Float32Array,
                transforms: e.data.sortData.transforms as Float32Array,
                transformIndices: e.data.sortData.transformIndices as Uint32Array,
                vertexCount: e.data.sortData.vertexCount,
            };
            await allocateBuffers();
        }
        if (e.data.viewProj) {
            viewProj = Float32Array.from(e.data.viewProj);
            runSort();
        }
        lock = false;
    }
    
};
