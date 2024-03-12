import SortWorker from "web-worker:../utils/SortWorker.ts";

import { ShaderProgram } from "./ShaderProgram";
import { ShaderPass } from "../passes/ShaderPass";
import { RenderData } from "../utils/RenderData";
import { Color32 } from "../../../math/Color32";
import { ObjectAddedEvent, ObjectChangedEvent, ObjectRemovedEvent } from "../../../events/Events";
import { Splat } from "../../../splats/Splat";
import { CameraData, WebGLRenderer } from "../../WebGLRenderer";
import { Quaternion, Vector3 } from "../../../index";

const vertexShaderSource = /* glsl */ `#version 300 es
precision highp float;
precision highp int;

uniform highp usampler2D u_texture;
uniform highp sampler2D u_transforms;
uniform highp usampler2D u_transformIndices;
uniform mat4 projection, view;
uniform vec2 focal;
uniform vec2 viewport;

uniform bool useDepthFade;
uniform float depthFade;
uniform float margin;

in vec2 position;
in int index;

out vec4 vColor;
out vec2 vPosition;
out float vSize;
//out float vSelected;

const int marginbitmask = 1 << 31; 
const int farmarginbitmask = 1 << 30; 

void main () {
    // if (index < 0)
    // {
    //     return;
    // }
    bool _fromMargin = (index & marginbitmask) != 0;
    bool _fromFarMargin = (index & farmarginbitmask) != 0;
    uint _index = uint((index & ~marginbitmask) & ~farmarginbitmask);

    uvec4 cen = texelFetch(u_texture, ivec2((_index & 0x3ffu) << 1, _index >> 10), 0);
    //float selected = float((cen.w >> 24) & 0xffu);

    /*
    uint transformIndex = texelFetch(u_transformIndices, ivec2(_index & 0x3ffu, _index >> 10), 0).x;
    mat4 transform = mat4(
        texelFetch(u_transforms, ivec2(0, transformIndex), 0),
        texelFetch(u_transforms, ivec2(1, transformIndex), 0),
        texelFetch(u_transforms, ivec2(2, transformIndex), 0),
        texelFetch(u_transforms, ivec2(3, transformIndex), 0)
    );

    if (selected < 0.5) {
        selected = texelFetch(u_transforms, ivec2(4, transformIndex), 0).x;
    }
    */
    mat4 viewTransform = view; // * transform;

    vec4 cam = viewTransform * vec4(uintBitsToFloat(cen.xyz), 1);
    vec4 pos2d = projection * cam;

    float clip = 1.2 * pos2d.w;
    if (pos2d.z < -pos2d.w || pos2d.z > pos2d.w || pos2d.x < -clip || pos2d.x > clip || pos2d.y < -clip || pos2d.y > clip) {
        //gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        return;
    }

    uvec4 cov = texelFetch(u_texture, ivec2(((_index & 0x3ffu) << 1) | 1u, _index >> 10), 0);
    vec2 u1 = unpackHalf2x16(cov.x), u2 = unpackHalf2x16(cov.y), u3 = unpackHalf2x16(cov.z);
    mat3 Vrk = mat3(u1.x, u1.y, u2.x, u1.y, u2.y, u3.x, u2.x, u3.x, u3.y);

    mat3 J = mat3(
        focal.x / cam.z, 0., -(focal.x * cam.x) / (cam.z * cam.z), 
        0., -focal.y / cam.z, (focal.y * cam.y) / (cam.z * cam.z), 
        0., 0., 0.
    );

    mat3 T = transpose(mat3(viewTransform)) * J;
    mat3 cov2d = transpose(T) * Vrk * T;

    
    cov2d[0][0] += 0.5;
    cov2d[1][1] += 0.5;
    
    
    // compute the coef of alpha based on the detemintant
    float kernel_size = 0.1;
    float det_0 = max(1e-6, cov2d[0][0] * cov2d[1][1] - cov2d[0][1] * cov2d[0][1]);
    float det_1 = max(1e-6, (cov2d[0][0] + kernel_size) * (cov2d[1][1] + kernel_size) - cov2d[0][1] * cov2d[0][1]);
    float coef = sqrt(det_0 / (det_1+1e-6) + 1e-6);

    if (det_0 <= 1e-6 || det_1 <= 1e-6){
        coef = 0.0f;
    }
    //cov2d[0][0] += kernel_size;
    //cov2d[1][1] += kernel_size;
    

    float mid = (cov2d[0][0] + cov2d[1][1]) / 2.0;
    float radius = length(vec2((cov2d[0][0] - cov2d[1][1]) / 2.0, cov2d[0][1]));
    float lambda1 = mid + radius, lambda2 = max(mid - radius, 0.0);

    //if (lambda2 < 0.0) return;
    vec2 diagonalVector = normalize(vec2(cov2d[0][1], lambda1 - cov2d[0][0]));
    vec2 majorAxis = min(sqrt(2.0 * lambda1), 1024.0) * diagonalVector;
    vec2 minorAxis = min(sqrt(2.0 * lambda2), 1024.0) * vec2(diagonalVector.y, -diagonalVector.x);

    vColor = vec4((cov.w) & 0xffu, (cov.w >> 8) & 0xffu, (cov.w >> 16) & 0xffu, (cov.w >> 24) & 0xffu) / 255.0;
    //vColor.a *= coef;

    //vColor = vec4(255, 0, 255, (cov.w >> 24) & 0xffu) / 255.0;
    
    
    vPosition = position;
    vSize = length(majorAxis);
    //vSelected = selected;

    float scalingFactor = 1.0;

    if (useDepthFade) {
        float depthNorm = (pos2d.z / pos2d.w + 1.0) / 2.0;
        float near = 0.1; float far = 100.0;
        float normalizedDepth = (2.0 * near) / (far + near - depthNorm * (far - near));
        float start = max(normalizedDepth - 0.1, 0.0);
        float end = min(normalizedDepth + 0.1, 1.0);
        scalingFactor = clamp((depthFade - start) / (end - start), 0.0, 1.0);
    }
    if (_fromFarMargin) {
       scalingFactor = 2.0;
    }
     if (/*_fromMargin || */ _fromFarMargin) {
         float gray = (vColor.r + vColor.g + vColor.b) / 3.0;
         vColor = vec4(gray, gray, gray, vColor.a);
     }
    float sorterW = 0.6 * pos2d.w;
    float sorterMarginW = sorterW * margin;
    if ( (pos2d.x > -sorterMarginW && pos2d.x < sorterMarginW && pos2d.y > -sorterMarginW && pos2d.y < sorterMarginW)
        && !(pos2d.x > -sorterW && pos2d.x < sorterW && pos2d.y > -sorterW && pos2d.y < sorterW) ) {
        scalingFactor =1.0;// 0.5;
        //vColor = vec4(1.0, 0.0, 1.0, 0.1);
    } else if (!(pos2d.x > -sorterW && pos2d.x < sorterW && pos2d.y > -sorterW && pos2d.y < sorterW)) {
        scalingFactor = 1.0;//0.5;
        //vColor = vec4(1.0, 1.0, 0.0, 0.1);
    }
    
    //scalingFactor = 1.0;

    vec2 vCenter = vec2(pos2d) / pos2d.w;
    gl_Position = vec4(
        vCenter 
        + position.x * majorAxis * scalingFactor / viewport
        + position.y * minorAxis * scalingFactor / viewport, pos2d.z / pos2d.w, 1.0);
}
`;

const fragmentShaderSource = /* glsl */ `#version 300 es
precision highp float;

//uniform float outlineThickness;
//uniform vec4 outlineColor;

in vec4 vColor;
in vec2 vPosition;
//in float vSize;
//in float vSelected;

out vec4 fragColor;

void main () {
    float A = -dot(vPosition, vPosition);

    if(A < -8.0) discard;
    //if (A > -5.0 || A < -6.0) discard;
    // if (A > 0.0) discard;

    /*
    if (vSelected < 0.5) {
        float B = exp(A) * vColor.a;
        fragColor = vec4(B * vColor.rgb, B);
        return;
    }

    float outlineThreshold = -4.0 + (outlineThickness / vSize);

    if (A < outlineThreshold) {
        fragColor = outlineColor;
    } 
    else*/ {
        float B = exp(A) * vColor.a;
        fragColor = vec4(B * vColor.rgb, B);
    }
}
`;

class RenderProgram extends ShaderProgram {
    lagCompensation: boolean = true;
    private _outlineThickness: number = 10.0;
    private _outlineColor: Color32 = new Color32(255, 165, 0, 255);
    private _renderData: RenderData | null = null;
    private _depthIndex: Uint32Array = new Uint32Array();
    private _chunks: Uint8Array | null = null;
    private _splatTexture: WebGLTexture | null = null;

    private _postDataToSorter: boolean = false;
    private _waitingForSorter: boolean = false;
    private _sorterHasAnyData: boolean = false;
    private _drawCount: number = 0;
    private _useIntPositions: boolean = false;

    protected _initialize: (cameraData: CameraData) => void;
    protected _resize: (projectionMatrix: number[]) => void;
    protected _render: (cameraData: CameraData) => void;
    protected _dispose: () => void;

    private _setOutlineThickness: (value: number) => void;
    private _setOutlineColor: (value: Color32) => void;
    private _lastProj: number[] | null = null;
    private _lastRotation: Quaternion = new Quaternion();
    private _lastPosition: Vector3 = new Vector3();
    private _margin: number = 20;

    predictViewProj: (proj:number[]) => void

    private _predictMultiplier = 3.0;
    private _predictFeedback = 1.5;
    private _sortRequested = Date.now();
    private _paintRequested = Date.now();
    private _paintTime = 16.7;
    private _nextSort = 0;

    private _currentDiff: number = 1;
    private _prevDiff: number = -1;


    clamp = (num: number, min: number, max: number) => Math.min(Math.max(num, min), max)

    


    constructor(renderer: WebGLRenderer, passes: ShaderPass[]) {
        super(renderer, passes);

        const canvas = renderer.canvas;
        const gl = renderer.gl;

        let worker: Worker;

        let u_projection: WebGLUniformLocation;
        let u_viewport: WebGLUniformLocation;
        let u_focal: WebGLUniformLocation;
        let u_view: WebGLUniformLocation;
        let u_texture: WebGLUniformLocation;
        let u_transforms: WebGLUniformLocation;
        let u_transformIndices: WebGLUniformLocation;

        let u_margin: WebGLUniformLocation;

        let u_outlineThickness: WebGLUniformLocation;
        let u_outlineColor: WebGLUniformLocation;

        let positionAttribute: number;
        let indexAttribute: number;

        let transformsTexture: WebGLTexture;
        let transformIndicesTexture: WebGLTexture;

        let vertexBuffer: WebGLBuffer;
        let indexBuffer: WebGLBuffer;

        this.predictViewProj = (proj:number[]) => {
            if (this._lastProj == null) {
                this._lastProj = proj;
                return proj;
            } else {
                //let result = this.renderer.camera?.data.predict(this._lastPosition, this._lastRotation, this._predictMultiplier).buffer;
                let diff = 0.0;
                let result = new Array<number>(16);
                //this.camera.
                for(var i = 0; i < 16; i++) {
                    result[i] = proj[i] + (0.0 + 1.5) * this._predictMultiplier * (proj[i] - this._lastProj[i]);
                    diff += Math.abs(proj[i] - result![i]);
                }
                this._currentDiff = diff;
                const nextMargin = this.clamp(Math.round(Math.sqrt(diff * 2048.0)), 10, 150);
                if (nextMargin < this._margin - 5) {
                    this._margin = this._margin - 5;
                } else {
                    this._margin = nextMargin;
                }
                gl.uniform1f(u_margin, 1 + (this._margin/100.0));
                console.log('margin: ' + this._margin);
    
                this._lastPosition =  this.renderer.camera?.data.position!;
                this._lastRotation = this.renderer.camera?.data.rotation!;
                this._lastProj = proj;
                return result;
            }
        }

        this._resize = (projectionMatrix: number[]) => {
            if (!this._camera)
            {
                console.warn('resize to handle!!!');
            }
            //if (!this._camera) return;

            //this._camera.data.setSize(canvas.width, canvas.height);
            //this._camera.update();

            u_projection = gl.getUniformLocation(this.program, "projection") as WebGLUniformLocation;
            gl.uniformMatrix4fv(u_projection, false, projectionMatrix);

            u_viewport = gl.getUniformLocation(this.program, "viewport") as WebGLUniformLocation;
            gl.uniform2fv(u_viewport, new Float32Array([canvas.width, canvas.height]));
        };
        let firstBufferWrite = true;
        const createWorker = () => {
            worker = new SortWorker();
            worker.onmessage = (e) => {
                
                if (e.data.depthIndex) {
                    const { depthIndex, /*chunks,*/ count } = e.data;
                    //this._depthIndex = depthIndex;
                    //this._chunks = chunks;
                    //gl.bindBuffer(gl.ARRAY_BUFFER, indexBuffer);
                    if (firstBufferWrite) {
                        gl.bufferData(gl.ARRAY_BUFFER, depthIndex, gl.DYNAMIC_DRAW);
                        firstBufferWrite = false;
                    } else {
                        gl.bufferSubData(gl.ARRAY_BUFFER, 0, depthIndex);
                    }
                    this._waitingForSorter = false;
                    this._drawCount = count;
                    
                    const now = Date.now();
                    this._nextSort = (now - this._sortRequested);
                    //this._predictMultiplier = /*1.0 + 1.5 **/ (this._nextSort / this._paintTime);
                    //setTimeout(()=>{this._predictMultiplier  = 1.0;})
                    
                    //this._predictMultiplier = 1.0 + this._predictFeedback * ( ((now - this._sortRequested) / this._paintTime));
                }
            };
        };

        this._initialize = (cameraData: CameraData) => {
            if (!this._scene /*|| !this._camera*/) {
                console.error("Cannot render without scene and camera");
                return;
            }

            this._resize(cameraData.projectionMatrix);

            this._scene.addEventListener("objectAdded", handleObjectAdded);
            this._scene.addEventListener("objectRemoved", handleObjectRemoved);
            for (const object of this._scene.objects) {
                if (object instanceof Splat) {
                    object.addEventListener("objectChanged", handleObjectChanged);
                }
            }

            this._renderData = new RenderData(this._scene);

            u_focal = gl.getUniformLocation(this.program, "focal") as WebGLUniformLocation;
            gl.uniform2fv(u_focal, new Float32Array([cameraData.fx, cameraData.fy]));

            u_view = gl.getUniformLocation(this.program, "view") as WebGLUniformLocation;
            gl.uniformMatrix4fv(u_view, false, cameraData.viewMatrix);

            u_outlineThickness = gl.getUniformLocation(this.program, "outlineThickness") as WebGLUniformLocation;
            gl.uniform1f(u_outlineThickness, this.outlineThickness);

            u_outlineColor = gl.getUniformLocation(this.program, "outlineColor") as WebGLUniformLocation;
            gl.uniform4fv(u_outlineColor, new Float32Array(this.outlineColor.flatNorm()));

            this._splatTexture = gl.createTexture() as WebGLTexture;
            u_texture = gl.getUniformLocation(this.program, "u_texture") as WebGLUniformLocation;
            gl.uniform1i(u_texture, 0);

            /*
            transformsTexture = gl.createTexture() as WebGLTexture;
            u_transforms = gl.getUniformLocation(this.program, "u_transforms") as WebGLUniformLocation;
            gl.uniform1i(u_transforms, 1);

            transformIndicesTexture = gl.createTexture() as WebGLTexture;
            u_transformIndices = gl.getUniformLocation(this.program, "u_transformIndices") as WebGLUniformLocation;
            gl.uniform1i(u_transformIndices, 2);
            */

            vertexBuffer = gl.createBuffer() as WebGLBuffer;
            gl.bindBuffer(gl.ARRAY_BUFFER, vertexBuffer);
            gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-2, -2, 2, -2, 2, 2, -2, 2]), gl.STATIC_DRAW);
            //gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1.42, -1.42, 1.42, -1.42, 1.42, 1.42, -1.42, 1.42]), gl.STATIC_DRAW);
            //gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-10, -10, 10, -10, 10, 10, -10, 10]), gl.STATIC_DRAW);
            //gl.bindBuffer(gl.ARRAY_BUFFER, null);

            positionAttribute = gl.getAttribLocation(this.program, "position");
            gl.enableVertexAttribArray(positionAttribute);
            gl.vertexAttribPointer(positionAttribute, 2, gl.FLOAT, false, 0, 0);

            indexBuffer = gl.createBuffer() as WebGLBuffer;
            indexAttribute = gl.getAttribLocation(this.program, "index");
            gl.enableVertexAttribArray(indexAttribute);
            gl.bindBuffer(gl.ARRAY_BUFFER, indexBuffer);

            //
            gl.clearColor(0, 0, 0, 0);
            gl.disable(gl.DEPTH_TEST);
            gl.enable(gl.BLEND);
            gl.blendFuncSeparate(gl.ONE_MINUS_DST_ALPHA, gl.ONE, gl.ONE_MINUS_DST_ALPHA, gl.ONE);
            gl.blendEquationSeparate(gl.FUNC_ADD, gl.FUNC_ADD);

            gl.vertexAttribIPointer(indexAttribute, 1, gl.INT, 0, 0);
            gl.vertexAttribDivisor(indexAttribute, 1);

            u_margin = gl.getUniformLocation(this.program, "margin") as WebGLUniformLocation;
            gl.uniform1f(u_margin, 1.1);

            createWorker();
        };

        const handleObjectAdded = (event: Event) => {
            const e = event as ObjectAddedEvent;

            if (e.object instanceof Splat) {
                e.object.addEventListener("objectChanged", handleObjectChanged);
            }

            this.dispose();
        };

        const handleObjectRemoved = (event: Event) => {
            const e = event as ObjectRemovedEvent;

            if (e.object instanceof Splat) {
                e.object.removeEventListener("objectChanged", handleObjectChanged);
            }

            this.dispose();
        };

        const handleObjectChanged = (event: Event) => {
            const e = event as ObjectChangedEvent;

            if (e.object instanceof Splat && this._renderData) {
                this._renderData.markDirty(e.object);
            }
        };

        this._render = (cameraData: CameraData) => {

            if (!this._scene /*|| !this._camera */|| !this.renderData) {
                console.error("Cannot render without scene and camera");
                return;
            }

            if (this.renderData.needsRebuild) {
                this.renderData.rebuild();
            }

            if (this.renderData.dataChanged || this.renderData.transformsChanged) {
                if (this.renderData.dataChanged) {
                    gl.activeTexture(gl.TEXTURE0);
                    gl.bindTexture(gl.TEXTURE_2D, this.splatTexture);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
                    gl.texImage2D(
                        gl.TEXTURE_2D,
                        0,
                        gl.RGBA32UI,
                        this.renderData.width,
                        this.renderData.height,
                        0,
                        gl.RGBA_INTEGER,
                        gl.UNSIGNED_INT,
                        this.renderData.data,
                    );
                    //gl.bindTexture(gl.TEXTURE_2D, null);
                }
                /*
                if (this.renderData.transformsChanged) {
                    gl.activeTexture(gl.TEXTURE1);
                    gl.bindTexture(gl.TEXTURE_2D, transformsTexture);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
                    gl.texImage2D(
                        gl.TEXTURE_2D,
                        0,
                        gl.RGBA32F,
                        this.renderData.transformsWidth,
                        this.renderData.transformsHeight,
                        0,
                        gl.RGBA,
                        gl.FLOAT,
                        this.renderData.transforms,
                    );

                    gl.activeTexture(gl.TEXTURE2);
                    gl.bindTexture(gl.TEXTURE_2D, transformIndicesTexture);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
                    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
                    gl.texImage2D(
                        gl.TEXTURE_2D,
                        0,
                        gl.R32UI,
                        this.renderData.transformIndicesWidth,
                        this.renderData.transformIndicesHeight,
                        0,
                        gl.RED_INTEGER,
                        gl.UNSIGNED_INT,
                        this.renderData.transformIndices,
                    );
                    gl.bindTexture(gl.TEXTURE_2D, null);
                }*/

                //const detachedPositions = new Float32Array(this.renderData.positions.slice().buffer);
                //const detachedTransforms = new Float32Array(this.renderData.transforms.slice().buffer);
                //const detachedTransformIndices = new Uint32Array(this.renderData.transformIndices.slice().buffer);
                this._postDataToSorter = true;
                // worker.postMessage(
                //     {
                //         sortData: {
                //             positions: detachedPositions,
                //             transforms: detachedTransforms,
                //             transformIndices: detachedTransformIndices,
                //             vertexCount: this.renderData.vertexCount,
                //         },
                //     },
                //     [detachedPositions.buffer, detachedTransforms.buffer, detachedTransformIndices.buffer],
                // );

                this.renderData.dataChanged = false;
                this.renderData.transformsChanged = false;
            }

            //this._camera.update();
            if (!this._waitingForSorter) {
                this._sortRequested = Date.now();
                const sortMessage = {
                    message: {
                        margin: this._margin,
                        viewProj: (this.lagCompensation ? this.predictViewProj(cameraData.viewProj) : cameraData.viewProj)
                        //viewProj: cameraData.viewProj//this._camera.data.viewProj.buffer
                    },
                    transfer: []
                } as any;
                if (this._postDataToSorter) {
                    this._postDataToSorter = false;
    
                    const detachedPositions = this._useIntPositions
                        ? new Int32Array(this.renderData.positions)
                        : new Float32Array(this.renderData.positions);
                    const detachedTransforms = new Float32Array(this.renderData.transforms);
                    const detachedTransformIndices = new Uint32Array(this.renderData.transformIndices);
    
                    sortMessage.transfer = [...sortMessage.transfer, detachedPositions.buffer, detachedTransforms.buffer, detachedTransformIndices.buffer];
    
                    sortMessage.message.sortData = {
                        positions: detachedPositions,
                        transforms: detachedTransforms,
                        transformIndices: detachedTransformIndices,
                        vertexCount: this.renderData.vertexCount,
                        useIntPositions: this._useIntPositions
                    }
                    this._sorterHasAnyData = true;
                }
                if (this._sorterHasAnyData || sortMessage.message.sortData) {
                    worker.postMessage(sortMessage.message, sortMessage.transfer);
                    this._predictMultiplier = 0.0;
                    this._waitingForSorter = true;
                }
            }

            this._lastProj = [...cameraData.viewProj];
            

            gl.viewport(0, 0, canvas.width, canvas.height);
            
            gl.clear(gl.COLOR_BUFFER_BIT);


            gl.uniformMatrix4fv(u_projection, false, cameraData.projectionMatrix);
            gl.uniformMatrix4fv(u_view, false, cameraData.viewMatrix);

            const now = Date.now();
            if (this._drawCount > 0) {
                this._predictMultiplier = this._predictMultiplier + 1.0;
                console.log('slerp: ' + this._predictMultiplier);
                gl.drawArraysInstanced(gl.TRIANGLE_FAN, 0, 4, this._drawCount);
                
            }
            //const now = Date.now();
            this._paintTime = Date.now() - this._paintRequested;
            this._paintRequested = now;
            this._prevDiff = this._currentDiff;
            
        };

        this._dispose = () => {
            if (!this._scene /*|| !this._camera*/ || !this.renderData) {
                console.error("Cannot dispose without scene and camera");
                return;
            }

            this._scene.removeEventListener("objectAdded", handleObjectAdded);
            this._scene.removeEventListener("objectRemoved", handleObjectRemoved);
            for (const object of this._scene.objects) {
                if (object instanceof Splat) {
                    object.removeEventListener("objectChanged", handleObjectChanged);
                }
            }

            worker.terminate();
            this.renderData.dispose();

            gl.deleteTexture(this.splatTexture);
            gl.deleteTexture(transformsTexture);
            gl.deleteTexture(transformIndicesTexture);

            gl.deleteBuffer(indexBuffer);
            gl.deleteBuffer(vertexBuffer);
        };

        this._setOutlineThickness = (value: number) => {
            this._outlineThickness = value;
            if (this._initialized) {
                gl.uniform1f(u_outlineThickness, value);
            }
        };

        this._setOutlineColor = (value: Color32) => {
            this._outlineColor = value;
            if (this._initialized) {
                gl.uniform4fv(u_outlineColor, new Float32Array(value.flatNorm()));
            }
        };
    }

    get renderData() {
        return this._renderData;
    }

    get depthIndex() {
        return this._depthIndex;
    }

    get chunks() {
        return this._chunks;
    }

    get splatTexture() {
        return this._splatTexture;
    }

    get outlineThickness() {
        return this._outlineThickness;
    }

    set outlineThickness(value: number) {
        this._setOutlineThickness(value);
    }

    get outlineColor() {
        return this._outlineColor;
    }

    set outlineColor(value: Color32) {
        this._setOutlineColor(value);
    }

    protected _getVertexSource() {
        return vertexShaderSource;
    }

    protected _getFragmentSource() {
        return fragmentShaderSource;
    }
}

export { RenderProgram };
