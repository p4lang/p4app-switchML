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
 * @file ProcessGroupSML.hpp
 * @brief Declares the ProcessGroupSML class.
 * 
 * This file was mainly constructed by following this tutorial then adding necessary SwitchML functionality.
 * https://pytorch.org/tutorials//intermediate/process_group_cpp_extension_tutorial.html
 */

#include <torch/python.h>

#include <c10d/ProcessGroup.hpp>
#include <c10d/Store.hpp>
#include <c10d/Types.hpp>
#include <c10d/Utils.hpp>

#include <c10/cuda/CUDAStream.h>

#include <pybind11/chrono.h>
#include <string>

#include "switchml/context.h"

namespace c10d {

/**
 * @brief A ProcessGroup that performs work through SwitchML's client library.
 */
class ProcessGroupSML: public ProcessGroup {
  public:

    /**
     * @brief An internal class that represents a single work request from ProcessGroupSML
     */
    class WorkSML : public ProcessGroup::Work {
      public:

        /**
         * @brief Construct a new WorkSML object and kick off copies to pinned host memory if needed
         * @param opType 
         * @param tensors 
         */
        WorkSML(OpType opType, std::vector<at::Tensor>& tensors);

        void run();

        /**
         * @brief Checks if the work has finished whether successfully or not.
         * 
         * @return true if the work finished
         * @return false otherwise
         */
        bool isCompleted() override;

        /**
         * @brief Checks if the work has finished successfully.
         * 
         * @return true if the work finished successfully
         * @return false otherwise
         */
        bool isSuccess() const override;

        /**
         * @brief Wait for the work to complete (Same as calling synchronize)
         * 
         * TODO: Use timeout
         * @param timeout unused
         * @return true always
         * @see synchronize()
         */
        bool wait(std::chrono::milliseconds timeout = kUnsetTimeout) override;

        /**
         * @brief Wait for the work to complete
         */
        virtual void synchronize() override;

        /**
         * @brief Return the resulting tensor which in our case will be
         * the same as the input tensor since the work is done in-place.
         * 
         * @return std::vector<at::Tensor> The resulting all-reduced tensor
         */
        std::vector<at::Tensor> result() override;

        /**
         * @brief Not supported !
         */
        virtual c10::intrusive_ptr<c10::ivalue::Future> getFuture() override;

      private:
        /** A reference to the submitted tensors to perform the work on. */
        std::vector<at::Tensor>& tensors_;
        /** Includes tensors that have been copied to host pinned memory in case they were originally on a GPU. */
        std::vector<at::Tensor> host_tensors_;

        std::vector<c10::Stream> streams_;

        std::vector<c10::Event> events_;

        /** A reference to the SwitchML job that is tied to this WorkSML object */
        std::shared_ptr<switchml::Job> sml_job_;
        /** Are the submitted tensors on cuda ? should we copy back to gpu after finishing? */
        bool on_cuda_;
        /** A convenient reference to the SwitchML context */
        switchml::Context& sml_ctx_;


    };

    ProcessGroupSML(int rank, int size);

    /**
     * @brief Get the Backend Name 
     * 
     * @return const std::string "sml"
     */
    const std::string getBackendName() const override {
        return std::string("sml");
    }


    /**
     * @brief Submit an allreduce request to this ProcessGroup
     * 
     * @param tensors A reference to the tensors to allreduce. 
     * These tensors have been observed to deallocate after this call finishes.
     * @param opts AllReduce options
     * @return c10::intrusive_ptr<ProcessGroup::Work> 
     */
    c10::intrusive_ptr<ProcessGroup::Work> allreduce(
        std::vector<at::Tensor>& tensors,
        const AllreduceOptions& opts = AllreduceOptions()) override;

    // The collective communication APIs without a custom implementation
    // like allgather will error out if invoked by application code.

    static c10::intrusive_ptr<ProcessGroup> createProcessGroupSML(
        const c10::intrusive_ptr<::c10d::Store>& store,
        int rank,
        int size,
        const std::chrono::duration<float>& timeout);

    static void ProcessGroupSMLConstructor() __attribute__((constructor)) {
        py::object module = py::module::import("torch.distributed");
        py::object register_backend =
            module.attr("Backend").attr("register_backend");
        // torch.distributed.Backend.register_backend will add `sml` as a
        // new valid backend.
        register_backend("sml", py::cpp_function(createProcessGroupSML));
    }

  private:
    switchml::Context& ctx_;
};
} // namespace c10d