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
 * @file glog_fix.h
 * @brief This file contains some workarounds for problems with 
 * the glog library version 3.5 (the version that we are currently using).
 * it seems that version 4.0 also suffers from the same issues.
 */

#ifndef SWITCHML_GLOG_FIX_H_
#define SWITCHML_GLOG_FIX_H_

// This is a workaround to expose the internal IsGoogleLoggingInitialized() function in glog.
// This might not be needed in future versions of glog. Refer to https://github.com/google/glog/issues/125
namespace google {
  namespace glog_internal_namespace_ {
    extern bool IsGoogleLoggingInitialized();
  }
}

// Convenience function to initialize the logger if its not initialized
#define INIT_LOG() if(!google::glog_internal_namespace_::IsGoogleLoggingInitialized()) \
  google::InitGoogleLogging("SwitchML");

// This is also a workaround to use the FLAGS_vmodule option which
// the glog folks documented but forgot to expose.
DECLARE_string(vmodule);

// Add DVLOG_IF
#if DCHECK_IS_ON()
#define DVLOG_IF(verboselevel, condition) VLOG_IF(verboselevel, condition)
#else
#define DVLOG_IF(verboselevel, condition) \
(true || !(condition) || !VLOG_IS_ON(verboselevel)) ? (void) 0 : google::LogMessageVoidify() & LOG(INFO)
#endif

#endif // SWITCHML_GLOG_FIX_H_
            