/**
 * SwitchML Project
 * @file cpu_exponent_quantizer.h
 * @brief Implements the CpuExponentQuantizer class.
 */

#include "cpu_exponent_quantizer_ppp.h"

#include <arpa/inet.h>
#include <math.h>

#include "common_cc.h"

#ifdef VCL
#include "vcl/vectorclass.h"
#define ENDIANESS_CONVERSION                                                  \
  3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12, 19, 18, 17, 16, 23,   \
      22, 21, 20, 27, 26, 25, 24, 31, 30, 29, 28, 35, 34, 33, 32, 39, 38, 37, \
      36, 43, 42, 41, 40, 47, 46, 45, 44, 51, 50, 49, 48, 55, 54, 53, 52, 59, \
      58, 57, 56, 63, 62, 61, 60
#endif

namespace switchml {

CpuExponentQuantizerPPP::CpuExponentQuantizerPPP(Config& config, WorkerTid worker_tid, Numel ltu_size, Numel batch_num_ltus) :
                                                 PrePostProcessor(config, worker_tid, ltu_size, batch_num_ltus),
    job_slice_(nullptr),
    scaling_factors_(nullptr),
    total_main_num_ltus_(0)
{
    // Do nothing
}

CpuExponentQuantizerPPP::~CpuExponentQuantizerPPP() {
    this->CleanupJobSlice();
}

uint64_t CpuExponentQuantizerPPP::SetupJobSlice(JobSlice* job_slice) {
    this->job_slice_ = job_slice;
    uint64_t tensor_size = job_slice->slice.numel * DataTypeSize(job_slice->slice.data_type);
    this->total_main_num_ltus_ = (tensor_size + this->ltu_size_ - 1) / this->ltu_size_; // Roundup division
    this->batch_num_ltus_ = std::min(this->total_main_num_ltus_, this->batch_max_num_ltus_);
    if (job_slice->slice.data_type == DataType::FLOAT32) {
        this->scaling_factors_ = new float[this->total_main_num_ltus_];
    }
    return this->total_main_num_ltus_;
}

bool CpuExponentQuantizerPPP::NeedsExtraBatch() {
    return this->job_slice_->slice.data_type == DataType::FLOAT32;
}

void CpuExponentQuantizerPPP::PreprocessSingle(uint64_t ltu_id, void* entries_ptr, void* exponent_ptr) {
    // Number of elements in an ltu
    uint64_t ltu_numel = this->ltu_size_ / DataTypeSize(this->job_slice_->slice.data_type);
    if (this->job_slice_->slice.data_type == DataType::FLOAT32) {
        // If this is not an LTU from the extra batch then we quantize and fill the backend buffers with 
        // the correct contents.
        if (ltu_id >= this->batch_num_ltus_) {
            // We subtract a batch from ltu id to ignore the empty first batch that was sent.
            ltu_id -= this->batch_num_ltus_;
            uint64_t job_slice_numel_offset = ltu_id * ltu_numel;
            float* in_ptr = static_cast<float*>(this->job_slice_->slice.in_ptr) + job_slice_numel_offset;
            int32_t* out_ptr = static_cast<int32_t*>(entries_ptr);

            uint64_t remaining_numel = this->job_slice_->slice.numel - job_slice_numel_offset;
            uint64_t numel_to_process = std::min(ltu_numel, remaining_numel);
            DVLOG(3) << "Worker thread '" << this->worker_tid_ << "' Quantizing/loading ltu_id=" << ltu_id + this->batch_num_ltus_ << 
                " [" << job_slice_numel_offset << "-" << (job_slice_numel_offset + numel_to_process - 1) << "]";

            uint64_t i = 0;
#ifdef VCL
            Vec64c vectorial_byte_data;
            Vec16f vectorial_float_data;
            Vec16f vectorial_scaling_factor = this->scaling_factors_[ltu_id];
            uint64_t to_vector_process = numel_to_process - numel_to_process % 16;
            for(; i < to_vector_process; i += 16) {
                vectorial_float_data.load(in_ptr + i);
                // Quantization
                vectorial_byte_data = roundi(vectorial_float_data * vectorial_scaling_factor);
                // Byte-order conversion
                permute64<ENDIANESS_CONVERSION>(vectorial_byte_data).store(out_ptr + i);
            }
#endif
            // Quantize the remainder elements.
            for (; i < numel_to_process; i++) {
                out_ptr[i] = htonl(std::round(in_ptr[i] * scaling_factors_[ltu_id]));
                DVLOG(4) << "Worker thread '" << this->worker_tid_ 
                    << "' slice_index=" << job_slice_numel_offset + i
                    << "' out_ptr[" << i << "]=" << out_ptr[i] 
                    << " in_ptr[" << i << "]=" << in_ptr[i] 
                    << " scaling_factors[" << ltu_id << "]=" << scaling_factors_[ltu_id];
            }

            // Add the subtracted batch back to ltu id so that exponent calculation happens for the next LTU
            ltu_id += this->batch_num_ltus_;
        }

        // In both cases of being an extra LTU or not, we need to compute the exponents
        // of the next LTU. Unless we won't be sending a next LTU.
        if(ltu_id < this->total_main_num_ltus_) {
            uint64_t job_slice_numel_offset = ltu_id * ltu_numel;
            float* in_ptr = static_cast<float*>(this->job_slice_->slice.in_ptr) + job_slice_numel_offset;

            uint64_t remaining_numel = this->job_slice_->slice.numel - job_slice_numel_offset;
            uint64_t numel_to_process = std::min(ltu_numel, remaining_numel);

            DVLOG(3) << "Worker thread '" << this->worker_tid_ << "' Computing exponent ltu_id=" << ltu_id << 
                " [" << job_slice_numel_offset << "-" << (job_slice_numel_offset + numel_to_process - 1) << "]";

            // First step is to find the absolute maximum between the LTU elements
            uint64_t i = 0;
            float current_max = 0;
#ifdef VCL
            Vec16f vectorial_current_max = 0;
            Vec16f vectorial_float_data;
            uint64_t to_vector_process = numel_to_process - numel_to_process % 16;
            for(; i < to_vector_process; i += 16) {
                vectorial_float_data.load(in_ptr + i);
                vectorial_current_max = max(vectorial_current_max, abs(vectorial_float_data));
            }
            // This call has a large overhead
            current_max = horizontal_max<Vec16f>(vectorial_current_max);
#endif
            for (; i < numel_to_process; i++) {
                float v = abs(in_ptr[i]);
                if (v > current_max) {
                    current_max = v;
                }
            }
            // Now we have the absolute maximum. 

            // Next we just convert it to an exponent.
            int8_t* exponent_int_ptr = static_cast<int8_t*>(exponent_ptr);
			// To calculate the exponent we select the 8 bits that represent the exponent field in the IEEE float representation.
			// Shift the 8 bits to start from the LSB then subtract 127 to remove the exponent bias and finally add 1 because we
			// want the exponent e and the actual value v such that 2^e >= v.
            *exponent_int_ptr = ((*((int32_t*)(&current_max)) & 0x7f800000) >> 23) - 126;
            DVLOG(4) << "Worker thread '" << this->worker_tid_ << "' ltu_id= " << ltu_id << " maximum=" << current_max << " exponent=" << (int) *exponent_int_ptr;
        }

    } else if (this->job_slice_->slice.data_type == DataType::INT32) {

        // Convert to big endian and send.
        uint64_t job_slice_numel_offset = ltu_id * ltu_numel;
        int32_t* in_ptr = static_cast<int32_t*>(this->job_slice_->slice.in_ptr) + job_slice_numel_offset;
        int32_t* out_ptr = static_cast<int32_t*>(entries_ptr);

        uint64_t remaining_numel = this->job_slice_->slice.numel - job_slice_numel_offset;
        uint64_t numel_to_process = std::min(ltu_numel, remaining_numel);

        DVLOG(3) << "Worker thread '" << this->worker_tid_ << "' Converting endinannes/loading ltu_id=" << ltu_id << 
            " [" << job_slice_numel_offset << "-" << (job_slice_numel_offset + numel_to_process - 1) << "]";

        uint64_t i = 0;
#ifdef VCL
        Vec64c vectorial_byte_data;
        uint64_t to_vector_process = numel_to_process - numel_to_process % 16;
        for(; i < to_vector_process; i += 16) {
            vectorial_byte_data.load(in_ptr + i);
            // Byte-order conversion
            permute64<ENDIANESS_CONVERSION>(vectorial_byte_data).store(out_ptr + i);
        }
#endif
        // Convert the remainder elements.
        for (; i < numel_to_process; i++) {
            out_ptr[i] = htonl(in_ptr[i]);
            DVLOG(4) << "Worker thread '" << this->worker_tid_ 
                << "' slice_index=" << job_slice_numel_offset + i
                << "' out_ptr[" << i << "]=" << out_ptr[i] 
                << " in_ptr[" << i << "]=" << in_ptr[i];
        }
    } else {
        LOG(FATAL) << "Worker thread '" << this->worker_tid_ << "' '" << this->job_slice_->slice.data_type << "' is not a supported data type.";
    }
}

void CpuExponentQuantizerPPP::PostprocessSingle(uint64_t ltu_id, void* entries_ptr, void* exponent_ptr) {
    // Number of elements in an ltu
    uint64_t ltu_numel = this->ltu_size_ / DataTypeSize(this->job_slice_->slice.data_type);
    if (this->job_slice_->slice.data_type == DataType::FLOAT32) {
        // If the LTU is not from the extra batch then let's dequantize it and move the contents
        // back to the client's buffers.
        if (ltu_id >= this->batch_num_ltus_) {
            // We subtract a batch from ltu id to ignore the empty first batch that was sent.
            ltu_id -= this->batch_num_ltus_;
            uint64_t job_slice_numel_offset = ltu_id * ltu_numel;

            float* out_ptr = static_cast<float*>(this->job_slice_->slice.out_ptr) + job_slice_numel_offset;
            int32_t* in_ptr = static_cast<int32_t*>(entries_ptr);

            uint64_t remaining_numel = this->job_slice_->slice.numel - job_slice_numel_offset;
            uint64_t numel_to_process = std::min(ltu_numel, remaining_numel);

            DVLOG(3) << "Worker thread '" << this->worker_tid_ << "' Dequantizing/unloading ltu_id=" << ltu_id + this->batch_num_ltus_ << 
                " [" << job_slice_numel_offset << "-" << (job_slice_numel_offset + numel_to_process-1) << "]";

            uint64_t i = 0;
#ifdef VCL
            Vec64c vectorial_byte_data;
            Vec16f vectorial_float_data;
            Vec16f vectorial_scaling_factor = this->scaling_factors_[ltu_id];
            uint64_t to_vector_process = numel_to_process - numel_to_process % 16;
            for(; i < to_vector_process; i += 16) {
                vectorial_byte_data.load(in_ptr + i);

                // Byte-order conversion
                vectorial_float_data = to_float((Vec16i)reinterpret_i(
                    permute64<ENDIANESS_CONVERSION>(vectorial_byte_data)));

                // Dequantization
                vectorial_float_data /= vectorial_scaling_factor;

                // Move to client buffer
                vectorial_float_data.store(out_ptr + i);
            }
#endif
            // If we do not set this iterator to volatile the optimizer tries to optimize this loop and ends up causing a segfault for 
            // specific number of elements (255) for example.
            // Since this portion of the code is only executed rarely this does not affect performance.
            volatile uint64_t j = i; 
            // Dequantize the remainder elements.
            for (; j < numel_to_process; j++) {
                int32_t in_be = (int32_t) ntohl(in_ptr[j]);
                out_ptr[j] = in_be / this->scaling_factors_[ltu_id];
                DVLOG(4) << "Worker thread '" << this->worker_tid_ 
                    << "' slice_index=" << job_slice_numel_offset + i
                    << "' out_ptr[" << i << "]=" << out_ptr[i] 
                    << " in_ptr[" << i << "]=" << in_ptr[i] 
                    << " scaling_factors[" << ltu_id << "]=" << scaling_factors_[ltu_id];
            }

            // Add the subtracted batch back to ltu_id so that the received global exponent is stored for the next LTU
            ltu_id += this->batch_num_ltus_;
        }

        // Compute the scaling factor from the received global exponent then store it.
        if(ltu_id < this->total_main_num_ltus_) {
            DVLOG(3) << "Worker thread '" << this->worker_tid_ << "' Computing scaling factor from received global exponent. ltu_id=" << ltu_id;
            int8_t exponent = *static_cast<int8_t*>(exponent_ptr);
            this->scaling_factors_[ltu_id] = 
                double(INT32_MAX) / (this->config_.general_.num_workers * powf(2, exponent));
            DVLOG(4) << "Worker thread '" << this->worker_tid_ << "' Scaling factor=" << this->scaling_factors_[ltu_id] << " Computed from received global exponent=" << (int) exponent;
        }

    } else if (this->job_slice_->slice.data_type == DataType::INT32) {
        // Convert to little endian and store.
        uint64_t job_slice_numel_offset = ltu_id * ltu_numel;
        int32_t* in_ptr = static_cast<int32_t*>(entries_ptr);
        int32_t* out_ptr = static_cast<int32_t*>(this->job_slice_->slice.out_ptr) + job_slice_numel_offset;

        uint64_t remaining_numel = this->job_slice_->slice.numel - job_slice_numel_offset;
        uint64_t numel_to_process = std::min(ltu_numel, remaining_numel);

        DVLOG(3) << "Worker thread '" << this->worker_tid_ << "' Converting endinannes/unloading ltu_id=" << ltu_id << 
            " [" << job_slice_numel_offset << "-" << (job_slice_numel_offset + numel_to_process - 1) << "]";

        uint64_t i = 0;
#ifdef VCL
        Vec64c vectorial_byte_data;
        uint64_t to_vector_process = numel_to_process - numel_to_process % 16;
        for(; i < to_vector_process; i += 16) {
            vectorial_byte_data.load(in_ptr + i);
            // Byte-order conversion
            permute64<ENDIANESS_CONVERSION>(vectorial_byte_data).store(out_ptr + i);
        }
#endif
        // If we do not set this iterator to volatile the optimizer tries to optimize this loop and ends up causing a segfault for 
        // specific number of elements (255) for example.
        // Since this portion of the code is only executed rarely this does not affect performance.
        volatile uint64_t j = i;
        // Convert the remainder elements.
        for (;j < numel_to_process; j++) {
            out_ptr[j] = ntohl(in_ptr[j]);
            DVLOG(4) << "Worker thread '" << this->worker_tid_ 
                << "' slice_index=" << job_slice_numel_offset + i
                << "' out_ptr[" << i << "]=" << out_ptr[i] 
                << " in_ptr[" << i << "]=" << in_ptr[i];
        }
    } else {
        LOG(FATAL) << "Worker thread '" << this->worker_tid_ << "' '" << this->job_slice_->slice.data_type << "' is not a supported data type.";
    } 
}

void CpuExponentQuantizerPPP::CleanupJobSlice() {
    if (this->scaling_factors_ != nullptr) {
        delete [] this->scaling_factors_;
        this->scaling_factors_ = nullptr;
    }
}

} // namespace switchml