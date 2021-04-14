/**
 * SwitchML Project
 * @file common.h
 * @brief Defines types and includes libraries that are needed in most switchml header files.
 * 
 * It is included in all .h files.
 */

#ifndef SWITCHML_COMMON_H_
#define SWITCHML_COMMON_H_

#include <stdint.h>
#include <chrono>

#include <glog/logging.h>
#include "glog_fix.h"

namespace switchml {
    
    // Lets give primitives names to make sure we use the same type for
    // The same logical use case.

    /** Type used to represent all job ids. */
    typedef uint64_t JobId;
    /** Type used to represent all worker thread ids. */
    typedef int16_t WorkerTid;
    /** Type used to represent the number of elements in all tensors */
    typedef uint64_t Numel;
    /** The clock type used in all time measurements for switchml. **/
    typedef std::chrono::system_clock clock;

    /**
     * @brief Numerical data type enum.
     */
    enum DataType {
        FLOAT32, /**< Represents a standard float type */
        INT32 /**< Represents a standard 32 bit signed integer */
    };

    /**
     * @brief Returns the size of an element of the given DataType
     * 
     * @param [in] type the data type to ask about.
     * @return uint16_t The size of a an element of this data type.
     */
    static inline uint16_t DataTypeSize(enum DataType type){
        // SUGGESTION: Cleaner to move this function somewhere else?
        if(type == FLOAT32 || type == INT32 ){
            return 4;
        } else {
            LOG(FATAL) << "'" << type << "' is not a valid tensor data type";
        }
        return 0; // To silence compiler
    }

    /**
     * @brief A struct to group up variables describing a tensor to be processed.
     */
    struct Tensor {
        /** 
         * Pointer to the input memory of the tensor. 
         * Any data changes are always written to the output. The input data is to be read from only !
         */
        void* in_ptr;
        /** Pointer to the output memory of the tensor */
        void* out_ptr;
        /** Number of **elements** in the tensor. (Not the size) */
        Numel numel;
        /** The numerical data type of the elements in the tensor */
        DataType data_type;

        /**
         * @brief A convenience function that offsets the tensor pointers by number of elements.
         * 
         * It casts the ptrs to the data_type then increments the pointers by numel argument.
         * The member numel is untouched.
         * 
         * @param [in] numel Number of **elements** to offset.
         */
        inline void OffsetPtrs(Numel numel) {
            // SUGGESTION: Cleaner to move this function to utils ?
            if(this->data_type == FLOAT32){
                float* ptr = static_cast<float*>(this->in_ptr);
                ptr += numel;
                this->in_ptr = ptr;
                ptr = static_cast<float*>(this->out_ptr);
                ptr += numel;
                this->out_ptr = ptr;
            } else if (this->data_type == INT32){
                int32_t* ptr = static_cast<int32_t*>(this->in_ptr);
                ptr += numel;
                this->in_ptr = ptr;
                ptr = static_cast<int32_t*>(this->out_ptr);
                ptr += numel;
                this->out_ptr = ptr;
            } else {
                LOG(FATAL) << "'" << this->data_type << "' is not a valid tensor data type";
            }
        }
    };
} // namespace switchml
#endif // SWITCHML_COMMON_H_