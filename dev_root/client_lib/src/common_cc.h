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
 * @file common_cc.h
 * @brief Defines types and includes libraries that are needed in most switchml implementation files.
 * 
 * The idea is we do not want the user to include lots of libraries as soon as it includes context.h.
 * Many libraries could be only needed for implementation.
 * The file is included in all .cc files
 */

// SUGGESTION: include logging here or in common.h ??
// #include <glog/logging.h>
// #include "glog_fix.inc"