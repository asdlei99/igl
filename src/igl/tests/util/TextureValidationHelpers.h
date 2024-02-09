/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <gtest/gtest.h>
#include <igl/IGL.h>
#include <igl/opengl/IContext.h>

#if IGL_PLATFORM_IOS || IGL_PLATFORM_MACOS
#include "simd/simd.h"
#else
#include "simdstub.h"
#endif
namespace igl::tests::util {
/// Reads back a range of texture data
/// @param device The device the texture was created with
/// @param cmdQueue A command queue to submit any read requests on
/// @param texture The texture to validate
/// @param isRenderTarget True if the texture was the target of a render pass; false otherwise
/// @param range The range of data to validate. Must resolve to a single 2D texture region
/// @param expectedData The expected data in the specified range
/// @param message A message to print when validation fails
inline void validateTextureRange(IDevice& device,
                                 ICommandQueue& cmdQueue,
                                 const std::shared_ptr<ITexture>& texture,
                                 bool isRenderTarget,
                                 const TextureRangeDesc& range,
                                 const uint32_t* expectedData,
                                 const char* message);

inline void validateFramebufferTextureRange(IDevice& device,
                                            ICommandQueue& cmdQueue,
                                            const IFramebuffer& framebuffer,
                                            const TextureRangeDesc& range,
                                            const uint32_t* expectedData,
                                            const char* message);
inline void validateFramebufferTexture(IDevice& device,
                                       ICommandQueue& cmdQueue,
                                       const IFramebuffer& framebuffer,
                                       const uint32_t* expectedData,
                                       const char* message);

inline void validateUploadedTextureRange(IDevice& device,
                                         ICommandQueue& cmdQueue,
                                         const std::shared_ptr<ITexture>& texture,
                                         const TextureRangeDesc& range,
                                         const uint32_t* expectedData,
                                         const char* message);
inline void validateUploadedTexture(IDevice& device,
                                    ICommandQueue& cmdQueue,
                                    const std::shared_ptr<ITexture>& texture,
                                    const uint32_t* expectedData,
                                    const char* message);

inline void validateTextureRange(IDevice& device,
                                 ICommandQueue& cmdQueue,
                                 const std::shared_ptr<ITexture>& texture,
                                 bool isRenderTarget,
                                 const TextureRangeDesc& range,
                                 const uint32_t* expectedData,
                                 const char* message) {
  Result ret;
  // Dummy command buffer to wait for completion.
  auto cmdBuf = cmdQueue.createCommandBuffer({}, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(cmdBuf != nullptr);
  cmdQueue.submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();

  ASSERT_EQ(range.numLayers, 1);
  ASSERT_EQ(range.numMipLevels, 1);
  ASSERT_EQ(range.depth, 1);

  const auto expectedDataSize = range.width * range.height;
  std::vector<uint32_t> actualData;
  actualData.resize(expectedDataSize);

  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = texture;
  auto fb = device.createFramebuffer(framebufferDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(fb != nullptr);

  fb->copyBytesColorAttachment(cmdQueue, 0, actualData.data(), range);

  if (!isRenderTarget && (device.getBackendType() == igl::BackendType::Metal ||
                          device.getBackendType() == igl::BackendType::Vulkan)) {
    // The Vulkan and Metal implementations of copyBytesColorAttachment flip the returned image
    // vertically. This is the desired behavior for render targets, but for non-render target
    // textures, we want the unflipped data. This flips the output image again to get the unmodified
    // data.
    std::vector<uint32_t> tmpData;
    tmpData.resize(actualData.size());
    for (size_t h = 0; h < range.height; ++h) {
      size_t src = (range.height - 1 - h) * range.width;
      size_t dst = h * range.width;
      for (size_t w = 0; w < range.width; ++w) {
        tmpData[dst++] = actualData[src++];
      }
    }
    actualData = std::move(tmpData);
  }

  for (size_t i = 0; i < expectedDataSize; i++) {
    ASSERT_EQ(expectedData[i], actualData[i])
        << message << ": Mismatch at index " << i << ": Expected: " << std::hex << expectedData[i]
        << " Actual: " << std::hex << actualData[i];
  }
}

inline void validateFramebufferTextureRange(IDevice& device,
                                            ICommandQueue& cmdQueue,
                                            const IFramebuffer& framebuffer,
                                            const TextureRangeDesc& range,
                                            const uint32_t* expectedData,
                                            const char* message) {
  validateTextureRange(
      device, cmdQueue, framebuffer.getColorAttachment(0), true, range, expectedData, message);
}

inline void validateFramebufferTexture(IDevice& device,
                                       ICommandQueue& cmdQueue,
                                       const IFramebuffer& framebuffer,
                                       const uint32_t* expectedData,
                                       const char* message) {
  validateFramebufferTextureRange(device,
                                  cmdQueue,
                                  framebuffer,
                                  framebuffer.getColorAttachment(0)->getFullRange(),
                                  expectedData,
                                  message);
}

inline void validateUploadedTextureRange(IDevice& device,
                                         ICommandQueue& cmdQueue,
                                         const std::shared_ptr<ITexture>& texture,
                                         const TextureRangeDesc& range,
                                         const uint32_t* expectedData,
                                         const char* message) {
  validateTextureRange(device, cmdQueue, texture, false, range, expectedData, message);
}

inline void validateUploadedTexture(IDevice& device,
                                    ICommandQueue& cmdQueue,
                                    const std::shared_ptr<ITexture>& texture,
                                    const uint32_t* expectedData,
                                    const char* message) {
  validateTextureRange(
      device, cmdQueue, texture, false, texture->getFullRange(), expectedData, message);
}

} // namespace igl::tests::util
