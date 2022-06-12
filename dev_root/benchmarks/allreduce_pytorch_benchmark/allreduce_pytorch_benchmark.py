#  Copyright 2021 Intel-KAUST-Microsoft
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

"""
SwitchML Project
@file allreduce_pytorch_benchmark.py

A benchmark for testing allreduce operations using different pytorch backends.
This benchmark is very similar to the switchml C++ allreduce benchmark. 
The difference is this benchmark goes through PyTorch whereas the other uses the switchml client library directly.
Another difference is that this benchmark can also be used to measure other backends' performances on the allreduce collective.
"""

import torch
import numpy as np

import importlib

try:
    import torch_switchml
    found_sml_extension = True
except ModuleNotFoundError:
    found_sml_extension = False

import torch.distributed as dist
import random
import argparse
import time

if __name__ == "__main__":

    # Parse arguments
    backend_choices = ["gloo"]
    backend_default = "gloo"
    if found_sml_extension:
        backend_choices.append("sml")
        backend_default = "sml"
    if dist.is_mpi_available():
        backend_choices.append("mpi")
    if dist.is_nccl_available():
        backend_choices.append("nccl")

    devices = ["cpu"]
    if torch.cuda.is_available():
        devices.append("gpu")

    parser = argparse.ArgumentParser("AllReduce PyTorch Benchmark")
    parser.add_argument("-n", "--num-workers", type=int, help="Number of workers in the setup", default=2)
    parser.add_argument("-r", "--rank", type=int, help="The rank of this worker in the setup", default=0)
    parser.add_argument("-ip" ,"--rank0-ip", type=str, help="The ip address of the node containing the worker with rank 0", default="127.0.0.1")
    parser.add_argument("-bk", "--backend", type=str, help="Which pytorch distributed backend do you want to use?", choices=backend_choices, default=backend_default)

    parser.add_argument("-tn", "--tensor-numel", type=int, help="Number of elements to all reduce.", default=268435456)
    parser.add_argument("-tt", "--tensor-type", type=str, help="Specify the data type to use.", choices=["float", "int32"], default="float")
    parser.add_argument("-d", "--device", type=str, help="Allocate the tensors on the specified device.", choices=devices, default="cpu")

    parser.add_argument("-nj", "--num-jobs", type=int, help="How many timed all reduce jobs should we submit?", default=10)
    parser.add_argument("-nw", "--num-warmup-jobs", type=int, help="How many untimed all reduce jobs should we submit before the timed ones?", default=5)
    parser.add_argument("-v", "--verify", action="store_true", help="Verify results to make sure they are as expected?", default=False)
    parser.add_argument("-sv", "--sync-every", type=int, help="When to wait for the submitted all reduce jobs to finish?. Set to 0 to wait only after you submit all of the jobs.", default=1)
    parser.add_argument("-e", "--err", type=float, help="The allowed error percentage. Used when verify is set to true", default=1)

    parser.add_argument("--random", action="store_true", help="Initialize the data with random values.", default=False)
    parser.add_argument("--seed", type=int, help="If you want to fix the seed of the random generator (In case you set random to true). Set to 0 to set to a random seed.", default=None)

    args = parser.parse_args()

    # Fix seed if needed
    if args.seed:
        torch.manual_seed(args.seed)
        random.seed(args.seed)

    # Allocate and initialize data.
    print("Allocating and initializing data..")
    dtype= torch.float if args.tensor_type == "float" else torch.int32

    device = {"cpu": "cpu", "gpu": "cuda"}[args.device]

    if args.random:
        data = torch.rand(args.tensor_numel, dtype=dtype, device=device)
    else:
        # Create fixed pattern 0 -1 2 -3 4 -5...
        data = torch.arange(0, args.tensor_numel, dtype=dtype, device=device)
        data = data * (-1)**data # To alternate sign

    if args.verify:
        expected_data = torch.clone(data) * args.num_workers ** (args.num_jobs + args.num_warmup_jobs)

    # Initialize process group
    print("Initializing process group..")
    dist.init_process_group(backend=args.backend, rank = args.rank, world_size=args.num_workers, init_method="tcp://{}:23456".format(args.rank0_ip))

    # Submit warmup jobs
    print("Submitting {} warmup jobs..".format(args.num_warmup_jobs))
    job_handles = list()
    for i in range(args.num_warmup_jobs):
        jh = dist.all_reduce(data, op=dist.ReduceOp.SUM, async_op=True)
        job_handles.append(jh)
    
    for jh in job_handles:
        jh.wait()

    job_handles.clear()
    print("Warmup finished.")

    # Submit timed jobs
    print("Submitting {} jobs..".format(args.num_jobs))

    round_str = list()
    durations_ns = list()
    throughput_gbps = list()

    begin = time.time_ns()
    for i in range(args.num_jobs):
        jh = dist.all_reduce(data, op=dist.ReduceOp.SUM, async_op=True)
        job_handles.append(jh)

        if len(job_handles) == args.sync_every:
            # Wait for all jobs
            for jh in job_handles:
                jh.wait()

            durations_ns.append(time.time_ns()-begin)
            throughput_gbps.append(args.tensor_numel*4*8*len(job_handles) / durations_ns[-1])
            round_str.append("{}-{}".format(i-len(job_handles)+1, i) if len(job_handles) > 1 else i)

            print("Job(s) #{}# finished. Duration: #{}# ns Goodput: #{}# Gbps.".format(
                round_str[-1],
                durations_ns[-1], 
                throughput_gbps[-1]
            ))

            job_handles.clear()
            begin = time.time_ns()
    
    for jh in job_handles:
        jh.wait()
    job_handles.clear()
    print("All jobs finished")
    
    # Verify data
    if args.verify:
        # This computes all errors in the tensor. It might be faster to just use a for loop so we can stop early.
        print("Verifying results")
        error_indices = None
        max_num_errors = 10
        if args.tensor_type == "int32":
            comparison = expected_data != data
        elif args.tensor_type == "float":
            epsilon = np.finfo(np.float32).eps # We add epsilon to avoid running into division by 0
            comparison = torch.abs( (expected_data - data) / (expected_data+epsilon) ) > (args.err/100)

        if torch.any(comparison):
            error_indices = torch.nonzero(comparison)
            print("Failed to verify data. Found {}/{} ({:.2f}%) errors.".format(len(error_indices), len(expected_data), len(error_indices)/len(expected_data)*100))
            
            i = 0
            for c in range(min(max_num_errors, len(error_indices))):
                if c == 0:
                    print("Printing first few errors")
                    index_sign = 1
                elif c == max_num_errors//2:
                    print("Printing last few errors")
                    index_sign = -1
                    i=1
                
                j = int(error_indices[i*index_sign])
                print("data[{:4}]: expected: {:6}, received: {:6}".format(j, float(expected_data[j]), float(data[j])))
                i += 1
        
        if error_indices is None:
            print("Results verified successfully.")

    # Print statistics
    print("Printing general statistics..")

    worst_job = np.argmin(throughput_gbps)
    print("Worst job(s): #{:2}#. Duration: #{:11.0f}# ns Goodput: #{:5.2f}# Gbps.".format(
        round_str[worst_job],
        durations_ns[worst_job], 
        throughput_gbps[worst_job]
    ))

    best_job = np.argmax(throughput_gbps)
    print("Best job(s) : #{:2}#. Duration: #{:11.0f}# ns Goodput: #{:5.2f}# Gbps.".format(
        round_str[best_job],
        durations_ns[best_job], 
        throughput_gbps[best_job]
    ))

    for stat in ["median", "mean", "std", "ptp"]:
        fn = getattr(np, stat)
        print("{:6}: Duration: #{:11.0f}# ns Goodput: #{:5.2f}# Gbps.".format(
            stat,
            fn(durations_ns),
            fn(throughput_gbps)
        ))

