import image from '@ohos.multimedia.image';
import fs from '@ohos.file.fs';
import fileUri from '@ohos.file.fileuri';

export interface PreviewPhoto {
  path: string;
  uri: string;
  width: number;
  height: number;
}

export function removePreviewPhoto(path: string): void {
  if (path.length === 0) {
    return;
  }
  try {
    fs.unlinkSync(path);
  } catch (e) {
    console.warn('[PreviewPhoto] Failed to remove temporary photo');
  }
}

// Takes ownership of a composited viewfinder snapshot. The compositor has
// already applied Surface rotation and clipping; do not crop the raw buffer again.
export async function savePreviewPhoto(pixels: image.PixelMap, cacheDir: string): Promise<PreviewPhoto> {
  let packer: image.ImagePacker | undefined = undefined;
  let file: fs.File | undefined = undefined;
  let completed = false;
  const path = `${cacheDir}/rtc-frame-${Date.now()}.jpg`;
  try {
    packer = image.createImagePacker();
    const info = await pixels.getImageInfo();
    if (info.size.width <= 1 || info.size.height <= 1) {
      throw new Error('预览画面尚未准备好，请稍后重试');
    }
    file = fs.openSync(path, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.TRUNC);
    await packer.packToFile(pixels, file.fd, { format: 'image/jpeg', quality: 95 });
    fs.closeSync(file);
    file = undefined;
    const uri = fileUri.getUriFromPath(path);
    completed = true;
    return { path: path, uri: uri, width: info.size.width, height: info.size.height };
  } finally {
    if (file !== undefined) {
      try {
        fs.closeSync(file);
      } catch (e) {
        console.warn('[PreviewPhoto] Failed to close temporary photo');
      }
    }
    if (!completed) {
      removePreviewPhoto(path);
    }
    try {
      await packer?.release();
    } catch (e) {
      console.warn('[PreviewPhoto] Failed to release image packer');
    }
    try {
      await pixels.release();
    } catch (e) {
      console.warn('[PreviewPhoto] Failed to release captured pixels');
    }
  }
}
