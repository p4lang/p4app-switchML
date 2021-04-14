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