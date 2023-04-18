/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "base/utils/Log.h"
#include "base/utils/TGFXCast.h"
#include "base/utils/TimeUtil.h"
#include "pag/pag.h"
#include "rendering/caches/DiskCache.h"

namespace pag {
float PAGDecoder::GetFrameRate(std::shared_ptr<PAGComposition> pagComposition) {
  auto numChildren = pagComposition->numChildren();
  if (numChildren == 0) {
    auto composition = static_cast<PreComposeLayer*>(pagComposition->layer)->composition;
    if (composition->type() == CompositionType::Bitmap ||
        composition->type() == CompositionType::Video) {
      return composition->frameRate;
    }
  }
  if (numChildren == 1) {
    auto firstLayer = pagComposition->getLayerAt(0);
    if (firstLayer->layerType() == LayerType::PreCompose) {
      return GetFrameRate(std::static_pointer_cast<PAGComposition>(firstLayer));
    }
  }
  return pagComposition->frameRate();
}

std::pair<int, float> PAGDecoder::GetFrameCountAndRate(std::shared_ptr<PAGComposition> composition,
                                                       float maxFrameRate) {
  auto compositionFrameRate = GetFrameRate(composition);
  auto frameRate = std::min(maxFrameRate, compositionFrameRate);
  auto duration = composition->duration();
  auto numFrames = static_cast<int>(round(static_cast<double>(duration) * frameRate / 1000000.0));
  return {numFrames, frameRate};
}

std::shared_ptr<PAGDecoder> PAGDecoder::MakeFrom(std::shared_ptr<PAGComposition> composition,
                                                 float maxFrameRate, float scale) {
  if (composition == nullptr) {
    return nullptr;
  }
  auto width = roundf(static_cast<float>(composition->width()) * scale);
  auto height = roundf(static_cast<float>(composition->height()) * scale);
  auto result = GetFrameCountAndRate(composition, maxFrameRate);
  return std::shared_ptr<PAGDecoder>(new PAGDecoder(std::move(composition), static_cast<int>(width),
                                                    static_cast<int>(height), result.first,
                                                    result.second, maxFrameRate));
}

PAGDecoder::PAGDecoder(std::shared_ptr<PAGComposition> composition, int width, int height,
                       int numFrames, float frameRate, float maxFrameRate)
    : _width(width), _height(height), _numFrames(numFrames), _frameRate(frameRate),
      maxFrameRate(maxFrameRate) {
  container = PAGComposition::Make(width, height);
  container->addLayer(composition);
}

int PAGDecoder::numFrames() {
  std::lock_guard<std::mutex> auoLock(locker);
  checkCompositionChange(getComposition());
  return _numFrames;
}

float PAGDecoder::frameRate() {
  std::lock_guard<std::mutex> auoLock(locker);
  checkCompositionChange(getComposition());
  return _frameRate;
}

bool PAGDecoder::readFrame(int index, void* pixels, size_t rowBytes, pag::ColorType colorType,
                           pag::AlphaType alphaType) {
  std::lock_guard<std::mutex> auoLock(locker);
  auto composition = getComposition();
  if (!checkSequenceFile(composition, rowBytes, colorType, alphaType)) {
    return false;
  }
  if (index < 0 || index >= _numFrames) {
    LOGE("PAGDecoder::readFrame() The index is out of range!");
    return false;
  }
  auto success = sequenceFile->readFrame(index, pixels);
  if (!success) {
    success = renderFrame(composition, index, pixels, rowBytes, colorType, alphaType);
    if (success) {
      success = sequenceFile->writeFrame(index, pixels);
      if (!success) {
        LOGE("PAGDecoder::readFrame() Failed to write frame to SequenceFile!");
      }
    }
  }
  if (sequenceFile->isComplete() && composition != nullptr) {
    if (pagPlayer != nullptr) {
      pagPlayer = nullptr;
      if (!composition.unique()) {
        container->addLayer(composition);
      }
    } else if (composition.use_count() <= 2) {
      container->removeAllLayers();
    }
  }
  return success;
}

bool PAGDecoder::renderFrame(std::shared_ptr<PAGComposition> composition, int index, void* pixels,
                             size_t rowBytes, pag::ColorType colorType, pag::AlphaType alphaType) {
  if (composition == nullptr) {
    pagPlayer = nullptr;
    LOGE(
        "PAGDecoder: Failed to get PAGComposition! the associated PAGComposition "
        "may be added to another parent after the PAGDecoder was created.");
    return false;
  }
  if (pagPlayer == nullptr) {
    auto pagSurface = PAGSurface::MakeOffscreen(_width, _height);
    if (pagSurface == nullptr) {
      LOGE("PAGDecoder::renderFrame() Failed to create PAGSurface!");
      return false;
    }
    pagPlayer = std::make_unique<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(composition);
  }
  auto progress = FrameToProgress(static_cast<Frame>(index), _numFrames);
  pagPlayer->setProgress(progress);
  pagPlayer->flush();
  auto pagSurface = pagPlayer->getSurface();
  return pagSurface->readPixels(colorType, alphaType, pixels, rowBytes);
}

bool PAGDecoder::checkSequenceFile(std::shared_ptr<PAGComposition> composition, size_t rowBytes,
                                   ColorType colorType, AlphaType alphaType) {
  checkCompositionChange(composition);
  if (sequenceFile != nullptr) {
    if (rowBytes != lastRowBytes || colorType != lastColorType || alphaType != lastAlphaType) {
      LOGE(
          "PAGDecoder::readFrame() The rowBytes, colorType or alphaType is not the same as the "
          "previous call!");
      return false;
    }
    return true;
  }
  if (composition == nullptr) {
    LOGE(
        "PAGDecoder: Failed to get PAGComposition! the associated PAGComposition "
        "may be added to another parent after the PAGDecoder was created.");
    return false;
  }
  auto key = generateCacheKey(composition);
  auto info =
      tgfx::ImageInfo::Make(_width, _height, ToTGFX(colorType), ToTGFX(alphaType), rowBytes);
  sequenceFile = DiskCache::OpenSequence(key, info, _numFrames, _frameRate);
  if (sequenceFile == nullptr) {
    LOGE("PAGDecoder: Failed to open SequenceFile!");
    return false;
  }
  lastRowBytes = rowBytes;
  lastColorType = colorType;
  lastAlphaType = alphaType;
  return true;
}

void PAGDecoder::checkCompositionChange(std::shared_ptr<PAGComposition> composition) {
  if (composition == nullptr || composition->contentVersion == lastContentVersion) {
    return;
  }
  sequenceFile = nullptr;
  lastContentVersion = composition->contentVersion;
  auto result = GetFrameCountAndRate(composition, maxFrameRate);
  _numFrames = result.first;
  _frameRate = result.second;
}

std::string PAGDecoder::generateCacheKey(std::shared_ptr<PAGComposition> composition) {
  if (!composition->isPAGFile() || composition->contentModified()) {
    return "";
  }
  auto filePath = static_cast<PAGFile*>(composition.get())->path();
  return filePath + "." + std::to_string(_width) + "x" + std::to_string(_height);
}

std::shared_ptr<PAGComposition> PAGDecoder::getComposition() {
  if (container->numChildren() == 1) {
    return std::static_pointer_cast<PAGComposition>(container->getLayerAt(0));
  }
  if (pagPlayer != nullptr) {
    return pagPlayer->getComposition();
  }
  return nullptr;
}
}  // namespace pag