import { PAG } from './types';
import { wasmAwaitRewind } from './utils/decorators';

@wasmAwaitRewind
export class PAGSurface {
  public static module: PAG;

  public static FromCanvas(canvasID: string): PAGSurface {
    const wasmIns = PAGSurface.module._PAGSurface._FromCanvas(canvasID);
    return new PAGSurface(wasmIns);
  }

  public static FromTexture(textureID: number, width: number, height: number, flipY: boolean): PAGSurface {
    const wasmIns = PAGSurface.module._PAGSurface._FromTexture(textureID, width, height, flipY);
    return new PAGSurface(wasmIns);
  }

  public static FromFrameBuffer(frameBufferID: number, width: number, height: number, flipY: boolean): PAGSurface {
    const wasmIns = PAGSurface.module._PAGSurface._FromFrameBuffer(frameBufferID, width, height, flipY);
    return new PAGSurface(wasmIns);
  }

  public wasmIns;

  public constructor(wasmIns: any) {
    this.wasmIns = wasmIns;
  }
  /**
   * The width of surface in pixels.
   */
  public width(): number {
    return this.wasmIns._width() as number;
  }
  /**
   * The height of surface in pixels.
   */
  public height(): number {
    return this.wasmIns._height() as number;
  }
  /**
   * Update the size of surface, and reset the internal surface.
   */
  public updateSize(): void {
    this.wasmIns._updateSize();
  }
  /**
   * Erases all pixels of this surface with transparent color. Returns true if the content has
   * changed.
   */
  public clearAll(): boolean {
    return this.wasmIns._clearAll() as boolean;
  }
  /**
   * Free the cache created by the surface immediately. Can be called to reduce memory pressure.
   */
  public freeCache(): void {
    this.wasmIns._freeCache();
  }
  /**
   * Copies pixels from current PAGSurface to dstPixels with specified color type, alpha type and
   * row bytes. Returns true if pixels are copied to dstPixels.
   */
  // TODO(zenoslin) binding PAGSurface::readPixels
  // public readPixels(colorType, alphaType, buffer): boolean {
  //   const dataUint8Array = new Uint8Array(buffer);
  //   const numBytes = dataUint8Array.byteLength * dataUint8Array.BYTES_PER_ELEMENT;
  //   const dataPtr = PAGSurface.module._malloc(numBytes);
  //   const dataOnHeap = new Uint8Array(PAGSurface.module.HEAPU8.buffer, dataPtr, numBytes);
  //   dataOnHeap.set(dataUint8Array);
  //   const res = this.wasmIns._readPixels(colorType, alphaType, dataOnHeap.byteOffset, dataOnHeap.length);
  //   PAGSurface.module._free(dataPtr);
  //   return res;
  // }

  public destroy(): void {
    this.wasmIns.delete();
  }
}
