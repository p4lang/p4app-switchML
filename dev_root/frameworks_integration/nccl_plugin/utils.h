/*
  Copyright 2021 Intel-KAUST-Microsoft

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

/**
 * SwitchML Project
 * @file utils.h
 * @brief Utility functions for the switchml nccl plugin.
 */

#ifndef NCCL_UTILS_H_
#define NCCL_UTILS_H_

#include <nccl_net.h>

extern ncclDebugLogger_t logger;

#define WARN(...)        logger(NCCL_LOG_WARN, NCCL_ALL, __FILE__, __LINE__, __VA_ARGS__)
#define INFO(FLAGS, ...) logger(NCCL_LOG_INFO, (FLAGS), __func__, __LINE__, __VA_ARGS__)

#ifdef ENABLE_TRACE
#pragma message "TRACE ENABLED"

#include <chrono>
#include <string>

#define TRACE(FLAGS, msg, ...) {                                                                                          \
  std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();                              \
  std::string ms = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()); \
  std::string message = ms + ": " + msg;                                                                                  \
  logger(NCCL_LOG_INFO, (FLAGS), __func__, __LINE__,                                                                      \
         message.c_str(), ##__VA_ARGS__); }

#else
#define TRACE(...)
#endif

// Propagate errors up
#define NCCLCHECK(call) do { \
  ncclResult_t res = call; \
  if (res != ncclSuccess) { \
    /* Print the back trace*/ \
    INFO(NCCL_ALL,"%s:%d -> %d", __FILE__, __LINE__, res); \
    return res; \
  } \
} while (0);

#define CUDACHECK(call) do { \
  cudaError_t res = call; \
  if(res != cudaSuccess) { \
    INFO(NCCL_ALL, "CUDA ERROR: %s", cudaGetErrorString(res)); \
    return ncclUnhandledCudaError; \
  } \
} while(0);

#define DIVUP(x, y) \
  (((x) + (y)-1) / (y))

#define ROUNDUP(x, y) \
  (DIVUP((x), (y)) * (y))

// Allocate memory to be potentially ibv_reg_mr'd. This needs to be
// allocated on separate pages as those pages will be marked DONTFORK
// and if they are shared, that could cause a crash in a child process
ncclResult_t NcclIbMalloc(void** ptr, size_t size) {
  size_t page_size = sysconf(_SC_PAGESIZE);
  void* p;
  int size_aligned = ROUNDUP(size, page_size);
  int ret = posix_memalign(&p, page_size, size_aligned);
  if (ret != 0)
    return ncclSystemError;
  memset(p, 0, size);
  *ptr = p;
  return ncclSuccess;
}

#endif // end include guard
