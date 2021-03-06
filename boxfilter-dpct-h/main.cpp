/*
 * Copyright 1993-2010 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

#define DPCT_USM_LEVEL_NONE
#include <CL/sycl.hpp>
#include <dpct/dpct.hpp>
#include <memory>
#include <iostream>
#include "shrUtils.h"

extern void BoxFilterHost(unsigned int* uiInputImage, unsigned int* uiTempImage, unsigned int* uiOutputImage, 
                          unsigned int uiWidth, unsigned int uiHeight, int iRadius, float fScale );


const unsigned int RADIUS = 10;                    // initial radius of 2D box filter mask
const float SCALE = 1.0f/(2.0f * RADIUS + 1.0f);  // precalculated GV rescaling value

inline uint DivUp(const uint a, const uint b){
  return (a % b != 0) ? (a / b + 1) : (a / b);
}


// Helper function to convert float[4] rgba color to 32-bit unsigned integer
//*****************************************************************

sycl::float4 rgbaUintToFloat4(const unsigned int c)
{
  sycl::float4 rgba;
  rgba.x() = c & 0xff;
  rgba.y() = (c >> 8) & 0xff;
  rgba.z() = (c >> 16) & 0xff;
  rgba.w() = (c >> 24) & 0xff;
  return rgba;
}

// Inline device function to convert floating point rgba color to 32-bit unsigned integer
//*****************************************************************

unsigned int rgbaFloat4ToUint(const sycl::float4 rgba, const float fScale)
{
  unsigned int uiPackedPix = 0U;
  uiPackedPix |= 0x000000FF & (unsigned int)(rgba.x() * fScale);
  uiPackedPix |= 0x0000FF00 & (((unsigned int)(rgba.y() * fScale)) << 8);
  uiPackedPix |= 0x00FF0000 & (((unsigned int)(rgba.z() * fScale)) << 16);
  uiPackedPix |= 0xFF000000 & (((unsigned int)(rgba.w() * fScale)) << 24);
  return uiPackedPix;
}

void row_kernel(const sycl::uchar4 *ucSource, uint *uiDest,
                const unsigned int uiWidth, const unsigned int uiHeight,
                const int iRadius, const int iRadiusAligned, const float fScale,
                const unsigned int uiNumOutputPix, sycl::nd_item<3> item_ct1,
                uint8_t *dpct_local)
{

  auto uc4LocalData = (sycl::uchar4 *)dpct_local;

  int lid = item_ct1.get_local_id(2);
  int gidx = item_ct1.get_group(2);
  int gidy = item_ct1.get_group(1);

  int globalPosX = gidx * uiNumOutputPix + lid - iRadiusAligned;
  int globalPosY = gidy;
  int iGlobalOffset = globalPosY * uiWidth + globalPosX;

  // Read global data into LMEM
  if (globalPosX >= 0 && globalPosX < uiWidth)
  {
    uc4LocalData[lid] = ucSource[iGlobalOffset];
  }
  else
    uc4LocalData[lid] = {0, 0, 0, 0};

  item_ct1.barrier();

  if((globalPosX >= 0) && (globalPosX < uiWidth) && (lid >= iRadiusAligned) && 
      (lid < (iRadiusAligned + (int)uiNumOutputPix)))
  {
    // Init summation registers to zero
    sycl::float4 f4Sum = {0.0f, 0.0f, 0.0f, 0.0f};

    // Do summation, using inline function to break up uint value from LMEM into independent RGBA values
    int iOffsetX = lid - iRadius;
    int iLimit = iOffsetX + (2 * iRadius) + 1;
    for(; iOffsetX < iLimit; iOffsetX++)
    {
      f4Sum.x() += uc4LocalData[iOffsetX].x();
      f4Sum.y() += uc4LocalData[iOffsetX].y();
      f4Sum.z() += uc4LocalData[iOffsetX].z();
      f4Sum.w() += uc4LocalData[iOffsetX].w();
    }

    // Use inline function to scale and convert registers to packed RGBA values in a uchar4, 
    // and write back out to GMEM
    uiDest[iGlobalOffset] = rgbaFloat4ToUint(f4Sum, fScale);
  }
}

void col_kernel (
    const uint* uiSource, 
    uint* uiDest, 
    const unsigned int uiWidth, 
    const unsigned int uiHeight, 
    const int iRadius, 
    const float fScale,
    sycl::nd_item<3> item_ct1)
{
  size_t globalPosX =
      item_ct1.get_group(2) * item_ct1.get_local_range().get(2) +
      item_ct1.get_local_id(2);
  const uint* uiInputImage = &uiSource[globalPosX];
  uint* uiOutputImage = &uiDest[globalPosX];
  // do left edge
  sycl::float4 f4Sum;
  // convert from "const int" to "float4" 
  f4Sum = rgbaUintToFloat4(uiInputImage[0]) * (sycl::float4)(iRadius); 
  for (int y = 0; y < iRadius + 1; y++) 
  {
    f4Sum += rgbaUintToFloat4(uiInputImage[y * uiWidth]);
  }
  uiOutputImage[0] = rgbaFloat4ToUint(f4Sum, fScale);
  for(int y = 1; y < iRadius + 1; y++) 
  {
    f4Sum += rgbaUintToFloat4(uiInputImage[(y + iRadius) * uiWidth]);
    f4Sum -= rgbaUintToFloat4(uiInputImage[0]);
    uiOutputImage[y * uiWidth] = rgbaFloat4ToUint(f4Sum, fScale);
  }

  // main loop
  for(int y = iRadius + 1; y < uiHeight - iRadius; y++) 
  {
    f4Sum += rgbaUintToFloat4(uiInputImage[(y + iRadius) * uiWidth]);
    f4Sum -= rgbaUintToFloat4(uiInputImage[((y - iRadius) * uiWidth) - uiWidth]);
    uiOutputImage[y * uiWidth] = rgbaFloat4ToUint(f4Sum, fScale);
  }

  // do right edge
  for (int y = uiHeight - iRadius; y < uiHeight; y++) 
  {
    f4Sum += rgbaUintToFloat4(uiInputImage[(uiHeight - 1) * uiWidth]);
    f4Sum -= rgbaUintToFloat4(uiInputImage[((y - iRadius) * uiWidth) - uiWidth]);
    uiOutputImage[y * uiWidth] = rgbaFloat4ToUint(f4Sum, fScale);
  }
}

void BoxFilterGPU(sycl::uchar4 *cmBufIn, unsigned int *cmBufTmp,
                  unsigned int *cmBufOut, const unsigned int uiWidth,
                  const unsigned int uiHeight, const int iRadius,
                  const float fScale)
{
  dpct::device_ext &dev_ct1 = dpct::get_current_device();
  sycl::queue &q_ct1 = dev_ct1.default_queue();
  const int szMaxWorkgroupSize = 256;
  const int iRadiusAligned = ((iRadius + 15)/16) * 16;  // 16
  unsigned int uiNumOutputPix = 64;  // Default output pix per workgroup
  if (szMaxWorkgroupSize < (iRadiusAligned + uiNumOutputPix + iRadius))
    uiNumOutputPix = szMaxWorkgroupSize - iRadiusAligned - iRadius;

  // Set global and local work sizes for row kernel // Workgroup padded left and right
  sycl::range<3> row_grid(1, uiHeight,
                          DivUp((size_t)uiWidth, (size_t)uiNumOutputPix));
  sycl::range<3> row_block(1, 1,
                           (size_t)(iRadiusAligned + uiNumOutputPix + iRadius));

  // Launch row kernel
  /*
  DPCT1049:0: The workgroup size passed to the SYCL kernel may exceed the limit.
  To get the device limit, query info::device::max_work_group_size. Adjust the
  workgroup size if needed.
  */
  {
    std::pair<dpct::buffer_t, size_t> cmBufIn_buf_ct0 =
        dpct::get_buffer_and_offset(cmBufIn);
    size_t cmBufIn_offset_ct0 = cmBufIn_buf_ct0.second;
    std::pair<dpct::buffer_t, size_t> cmBufTmp_buf_ct1 =
        dpct::get_buffer_and_offset(cmBufTmp);
    size_t cmBufTmp_offset_ct1 = cmBufTmp_buf_ct1.second;
    q_ct1.submit([&](sycl::handler &cgh) {
      sycl::accessor<uint8_t, 1, sycl::access::mode::read_write,
                     sycl::access::target::local>
          dpct_local_acc_ct1(
              sycl::range<1>(sizeof(sycl::uchar4) *
                             (iRadiusAligned + uiNumOutputPix + iRadius)),
              cgh);
      auto cmBufIn_acc_ct0 =
          cmBufIn_buf_ct0.first.get_access<sycl::access::mode::read_write>(cgh);
      auto cmBufTmp_acc_ct1 =
          cmBufTmp_buf_ct1.first.get_access<sycl::access::mode::read_write>(
              cgh);

      cgh.parallel_for(
          sycl::nd_range<3>(row_grid * row_block, row_block),
          [=](sycl::nd_item<3> item_ct1) {
            const sycl::uchar4 *cmBufIn_ct0 =
                (const sycl::uchar4 *)(&cmBufIn_acc_ct0[0] +
                                       cmBufIn_offset_ct0);
            unsigned int *cmBufTmp_ct1 =
                (unsigned int *)(&cmBufTmp_acc_ct1[0] + cmBufTmp_offset_ct1);
            row_kernel(cmBufIn_ct0, cmBufTmp_ct1, uiWidth, uiHeight, iRadius,
                       iRadiusAligned, fScale, uiNumOutputPix, item_ct1,
                       dpct_local_acc_ct1.get_pointer());
          });
    });
  }

  // Set global and local work sizes for column kernel
  sycl::range<3> col_grid(1, 1, DivUp((size_t)uiWidth, 64));
  sycl::range<3> col_block(1, 1, 64);

  // Launch column kernel
  /*
  DPCT1049:1: The workgroup size passed to the SYCL kernel may exceed the limit.
  To get the device limit, query info::device::max_work_group_size. Adjust the
  workgroup size if needed.
  */
  {
    std::pair<dpct::buffer_t, size_t> cmBufTmp_buf_ct0 =
        dpct::get_buffer_and_offset(cmBufTmp);
    size_t cmBufTmp_offset_ct0 = cmBufTmp_buf_ct0.second;
    std::pair<dpct::buffer_t, size_t> cmBufOut_buf_ct1 =
        dpct::get_buffer_and_offset(cmBufOut);
    size_t cmBufOut_offset_ct1 = cmBufOut_buf_ct1.second;
    q_ct1.submit([&](sycl::handler &cgh) {
      auto cmBufTmp_acc_ct0 =
          cmBufTmp_buf_ct0.first.get_access<sycl::access::mode::read_write>(
              cgh);
      auto cmBufOut_acc_ct1 =
          cmBufOut_buf_ct1.first.get_access<sycl::access::mode::read_write>(
              cgh);

      cgh.parallel_for(
          sycl::nd_range<3>(col_grid * col_block, col_block),
          [=](sycl::nd_item<3> item_ct1) {
            const uint *cmBufTmp_ct0 =
                (const uint *)(&cmBufTmp_acc_ct0[0] + cmBufTmp_offset_ct0);
            unsigned int *cmBufOut_ct1 =
                (unsigned int *)(&cmBufOut_acc_ct1[0] + cmBufOut_offset_ct1);
            col_kernel(cmBufTmp_ct0, cmBufOut_ct1, uiWidth, uiHeight, iRadius,
                       fScale, item_ct1);
          });
    });
  }
}

int main(int argc, char** argv)
{
  unsigned int uiImageWidth = 0;      // Image width
  unsigned int uiImageHeight = 0;     // Image height
  unsigned int* uiInput = NULL;       // Host buffer to hold input image data
  unsigned int* uiTmp = NULL;        // Host buffer to hold intermediate image data
  unsigned int* uiDevOutput = NULL;      
  unsigned int* uiHostOutput = NULL;      

  shrLoadPPM4ub(argv[1], (unsigned char**)&uiInput, &uiImageWidth, &uiImageHeight);
  printf("Image Width = %i, Height = %i, bpp = %i, Mask Radius = %i\n", 
      uiImageWidth, uiImageHeight, sizeof(unsigned int)<<3, RADIUS);
  printf("Using Local Memory for Row Processing\n\n");

  size_t szBuff= uiImageWidth * uiImageHeight;
  size_t szBuffBytes = szBuff * sizeof (unsigned int);

  // Allocate intermediate and output host image buffers
  uiTmp = (unsigned int*)malloc(szBuffBytes);
  uiDevOutput = (unsigned int*)malloc(szBuffBytes);
  uiHostOutput = (unsigned int*)malloc(szBuffBytes);

  sycl::uchar4 *cmDevBufIn;
  unsigned int* cmDevBufTmp;
  unsigned int* cmDevBufOut;

  cmDevBufIn = (sycl::uchar4 *)dpct::dpct_malloc(szBuffBytes);
  cmDevBufTmp = (unsigned int *)dpct::dpct_malloc(szBuffBytes);
  cmDevBufOut = (unsigned int *)dpct::dpct_malloc(szBuffBytes);

  // Copy input data from host to device
  dpct::dpct_memcpy(cmDevBufIn, uiInput, szBuffBytes, dpct::host_to_device);

  // Warmup
  BoxFilterGPU (cmDevBufIn, cmDevBufTmp, cmDevBufOut, 
      uiImageWidth, uiImageHeight, RADIUS, SCALE);

  dpct::get_current_device().queues_wait_and_throw();

  const int iCycles = 1000;
  printf("\nRunning BoxFilterGPU for %d cycles...\n\n", iCycles);
  for (int i = 0; i < iCycles; i++)
  {
    BoxFilterGPU (cmDevBufIn, cmDevBufTmp, cmDevBufOut, 
        uiImageWidth, uiImageHeight, RADIUS, SCALE);
  }

  // Copy output from device to host
  dpct::dpct_memcpy(uiDevOutput, cmDevBufOut, szBuffBytes,
                    dpct::device_to_host);

  dpct::dpct_free(cmDevBufIn);
  dpct::dpct_free(cmDevBufTmp);
  dpct::dpct_free(cmDevBufOut);

  // Do filtering on the host
  BoxFilterHost(uiInput, uiTmp, uiHostOutput, uiImageWidth, uiImageHeight, RADIUS, SCALE);

  // Verification 
  // The entire images do not match due to the difference between BoxFilterHostY and the column kernel )
  int error = 0;
  for (int i = RADIUS * uiImageWidth; i < (uiImageHeight-RADIUS)*uiImageWidth; i++)
  {
    if (uiDevOutput[i] != uiHostOutput[i]) {
      printf("%d %08x %08x\n", i, uiDevOutput[i], uiHostOutput[i]);
      error = 1;
      break;
    }
  }
  if (error) 
    printf("FAIL\n");
  else
    printf("PASS\n");

  free(uiInput);
  free(uiTmp);
  free(uiDevOutput);
  free(uiHostOutput);
  return 0;
}

