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
 * @file ProcessGroupSML.cpp.cc
 * @brief Implements the ProcessGroupSML class.
 * 
 * This file was mainly constructed by following this tutorial then adding necessary SwitchML functionality.
 * https://pytorch.org/tutorials//intermediate/process_group_cpp_extension_tutorial.html
 * 
 * It currently imitates what ProcessGroupGloo does when copying from gpu to cpu memory and back. 
 * (/torch/csrc/distributed/c10d/ProcessGroupGloo.cpp)
 * ProcessGroupGloo does the communication in-sync and the GPU copy back afterwards async.
 */

#include "ProcessGroupSML.hpp"

#include <ATen/cuda/PinnedMemoryAllocator.h>
#include <c10/cuda/CUDAGuard.h>
#include <iostream>

namespace c10d {

/** SwitchML type typing */
std::map<at::ScalarType, switchml::DataType> smlDataType = {
    {at::kFloat, switchml::DataType::FLOAT32},
    {at::kInt, switchml::DataType::INT32}
};

static switchml::DataType getSmlDataType(at::ScalarType torch_type) {
    auto it = smlDataType.find(torch_type);
    CHECK(it != smlDataType.end()) << "Input tensor data type is not supported for SML process group";
    return it->second;
}

/** CUDA Utility functions */

/**
 * @brief takes in a tensor and allocates a host pinned buffer equal to its size.
 * 
 * This function was taken as is from ProcessGroupGloo in PyTorch.
 * 
 * I do not understand the implementation details. However,
 * the function is clear, which is that it takes in a tensor and allocates 
 * a host pinned buffer equal to its size.
 * It is easier to just use the CUDA api directly using cudaMallocHost but I chose 
 * to immitate ProcessGroupGloo as it seems that PinnedMemoryAllocator adds a layer of
 * management that allows us to reuse pinned memory and avoid many expensive cudaMallocHost calls.
 * 
 * @param tensor The reference tensor which we will get the size, dtype...etc. info from.
 * @return at::Tensor A tensor that is stored on pinned host memory.
 */
at::Tensor pinnedLike(at::Tensor& tensor) {
  auto* allocator = at::cuda::getPinnedMemoryAllocator();
  auto storage = c10::Storage(
      c10::Storage::use_byte_size_t(),
      at::detail::computeStorageNbytes(
          tensor.sizes(), tensor.strides(), tensor.dtype().itemsize()),
      allocator,
      /*resizable=*/false);
  return at::empty({0}, tensor.options().device(at::kCPU))
      .set_(storage, 0, tensor.sizes(), tensor.strides());
}

/**
 * @brief This function initializes a vector of CUDA streams, one for every
 * tensor in the input tensor vector, and ensures that these streams are
 * synchronized with the current default streams. This is needed so
 * that new work on the new streams is serialized w.r.t. all operations
 * on the tensors.
 * 
 * This function was taken from ProcessGroupGloo in PyTorch and modified.
 * 
 * @param tensors The input tensors
 * @param streams The work streams
 * @param events The event streams
 */
void initializeStreamsEvents( const std::vector<at::Tensor>& tensors,
                              std::vector<c10::Stream>& streams, std::vector<c10::Event>& events) {
        c10::Device device = tensors[0].device();
        c10::impl::VirtualGuardImpl impl(device.type());
        // Record event on current stream
        events.emplace_back(device.type());
        events[0].record(impl.getStream(device));
        // Get a non-default stream to execute asynchronous CUDA operations
        // on for this device. This ensures that the default stream used
        // by the caller is not occupied by c10d related operations.
        streams.push_back(impl.getStreamFromGlobalPool(device, /*isHighPriority=*/true));
        // Ensure the new stream is synchronized with the current stream.
        events[0].block(streams[0]);

        // `tensors` are created on a different stream. Hence, they must record
        // new streams in this Work to prevent being freed before the Work finishes.
        impl.recordDataPtrOnStream(tensors[0].storage().data_ptr(), streams[0]);
}

/** WorkSML implementations */

ProcessGroupSML::WorkSML::WorkSML(OpType opType, std::vector<at::Tensor>& tensors) 
    : ProcessGroup::Work(-1, opType),
      tensors_(tensors), sml_ctx_(switchml::Context::GetInstance()) {
    
    if (this->tensors_[0].is_cuda()) {
        // To understand PyTorch cuda semantics, visit
        // https://pytorch.org/docs/stable/notes/cuda.html
        // https://pytorch.org/cppdocs/notes/tensor_cuda_stream.html

        initializeStreamsEvents(this->tensors_, this->streams_, this->events_);
        // Kick off copy from CUDA tensors to pinned CPU tensors.
        at::cuda::OptionalCUDAStreamGuard guard;
        guard.reset_stream(this->streams_[0]);
        this->host_tensors_.push_back(pinnedLike(this->tensors_[0]).copy_(this->tensors_[0], /*non_blocking=*/ true));
        this->on_cuda_ = true;
    } else {
        this->on_cuda_ = false;
    }

    // TODO: Have a queue and a thread that consumes and runs work for more copy parallelism
    // Just like ProcessGroupGloo
    this->run();
}

void ProcessGroupSML::WorkSML::run() {

    switchml::DataType data_type = getSmlDataType(this->tensors_[0].scalar_type());
    void* data_ptr;
    if(this->on_cuda_) {
        // Synchronize with copy operations.
        this->streams_[0].synchronize();
        data_ptr = this->host_tensors_[0].data_ptr();
    } else {
        data_ptr = this->tensors_[0].data_ptr();
    }

    this->sml_job_ = this->sml_ctx_.AllReduceAsync(data_ptr, data_ptr, this->tensors_[0].numel(),
                                                   data_type, switchml::AllReduceOperation::SUM);
    this->sml_job_->WaitToComplete();

    if(this->on_cuda_) {
        // Copy the tensor back to GPU in the style of ProcessGroupGloo
        at::cuda::OptionalCUDAStreamGuard guard;
        guard.reset_stream(this->streams_[0]);

        // FIXME: Figure out why input tensors are being deallocated when the function exits.
        // Then set non_blocking to true and uncomment the synchronization code in synchronize.
        this->tensors_[0].copy_(this->host_tensors_[0], /*non_blocking=*/ false);
        this->events_[0].record(this->streams_[0]);
    }
}

bool ProcessGroupSML::WorkSML::isCompleted() {
    return (this->sml_job_->GetJobStatus() == switchml::JobStatus::FINISHED || 
           this->sml_job_->GetJobStatus() == switchml::JobStatus::FAILED) &&
           this->events_[0].query();
}

bool ProcessGroupSML::WorkSML::isSuccess() const {
    return this->sml_job_->GetJobStatus() == switchml::JobStatus::FINISHED &&
           this->events_[0].query();
}

void ProcessGroupSML::WorkSML::synchronize() {
    // FIXME:
    // if(this->on_cuda_) {
    //     // Synchronize with the copy back to CUDA tensors.
    //     c10::Device device = this->tensors_[0].device();
    //     this->events_[0].block(c10::impl::VirtualGuardImpl(device.type()).getStream(device));
    // }
}

// Same as calling synchronize().
bool ProcessGroupSML::WorkSML::wait(std::chrono::milliseconds) {
    this->synchronize();
    return true;
}

c10::intrusive_ptr<c10::ivalue::Future> ProcessGroupSML::WorkSML::getFuture() {
    throw std::runtime_error("ProcessGroupSML does not support the getFuture API");
}

std::vector<at::Tensor> ProcessGroupSML::WorkSML::result() {
  return this->tensors_;
}

/** ProcessGroupSML implementations */

ProcessGroupSML::ProcessGroupSML(int rank, int size)
    : ProcessGroup(rank, size), 
      ctx_(switchml::Context::GetInstance()) {
    // Start the switchml context
    this->ctx_.Start();
}

c10::intrusive_ptr<ProcessGroup::Work> ProcessGroupSML::allreduce(
    std::vector<at::Tensor>& tensors,
    const AllreduceOptions& opts) {

    CHECK_EQ(tensors.size(), 1) << "The number of tensors to reduce in a single allreduce call must be equal to 1. "
     << "The torch_switchml plugin does not currently support multiple gpus on the same machine.";

    CHECK_EQ(static_cast<uint8_t>(opts.reduceOp), static_cast<uint8_t>(ReduceOp::SUM)) << "Switchml only supports summation allreduce.";

    return c10::make_intrusive<WorkSML>(OpType::ALLREDUCE, tensors);
}

c10::intrusive_ptr<ProcessGroup> ProcessGroupSML::createProcessGroupSML(
        const c10::intrusive_ptr<::c10d::Store>& /* unused */,
        int rank,
        int size,
        const std::chrono::duration<float>& /* unused */) {
    return c10::make_intrusive<ProcessGroupSML>(rank, size);
}

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {
    m.def("createProcessGroupSML", &ProcessGroupSML::createProcessGroupSML);
}

} // namespace c10d