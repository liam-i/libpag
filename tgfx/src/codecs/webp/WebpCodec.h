/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#pragma once

#include "tgfx/core/ImageCodec.h"
#include "webp/decode.h"
#include "webp/demux.h"
#include "webp/encode.h"

namespace tgfx {
class WebpCodec : public ImageCodec {
 public:
  static std::shared_ptr<ImageCodec> MakeFrom(const std::string& filePath);
  static std::shared_ptr<ImageCodec> MakeFrom(std::shared_ptr<Data> imageBytes);
  static bool IsWebp(const std::shared_ptr<Data>& data);

#ifdef TGFX_USE_WEBP_ENCODE
  static std::shared_ptr<Data> Encode(const Pixmap& pixmap, int quality);
#endif

 protected:
  bool readPixels(const ImageInfo& dstInfo, void* dstPixels) const override;

 private:
  std::shared_ptr<Data> fileData;
  std::string filePath;

  explicit WebpCodec(int width, int height, EncodedOrigin origin, std::string filePath,
                     std::shared_ptr<Data> fileData)
      : ImageCodec(width, height, origin),
        fileData(std::move(fileData)),
        filePath(std::move(filePath)) {
  }
};

}  // namespace tgfx