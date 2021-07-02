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
 * @file main.cu
 * @brief Implements the hello_world example.
 */

#include <switchml/context.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

int main(){
    switchml::Context& ctx = switchml::Context::GetInstance();
    printf("Hello world!. Starting the switchml context\n");
    ctx.Start();

    uint64_t numel = (1 << 15);
    int num_tensors = 8;
    int num_workers = ctx.GetConfig().general_.num_workers;
    printf("Allocating data\n");
    float in_data[num_tensors][numel];
    float out_data[num_tensors][numel];
    // Init data
    printf("Initializing data\n");
    for(int i = 0; i < num_tensors; i++) {
        for(uint64_t j = 0; j < numel; j++) {
            in_data[i][j] = i*numel+j;
        }
    }

    printf("Submitting all reduce jobs\n");
    for(int i = 0; i < num_tensors; i++) {
        ctx.AllReduceAsync(in_data[i], out_data[i], numel, switchml::FLOAT32, switchml::AllReduceOperation::SUM);
    }

    printf("Waiting for all jobs to finish\n");
    ctx.WaitForAllJobs();
    printf("Stopping the switchml context\n");
    ctx.Stop();

    // Verify results
    printf("Verifying results\n");
    for(int i = 0; i < num_tensors; i++) {
        for(uint64_t j = 0; j < numel; j++) {
            float input = i*numel+j; // Use the same formula you used when you initialized
            float expected = input * num_workers;
            float error = (expected-out_data[i][j]) / (expected + std::numeric_limits<float>::epsilon()) * 100; // We add epsilon to avoid running into division by 0
            if( error > 1) {
                printf("Failed to verify output data. Element %ld in tensor %d was %e but we expected %e (error %.2f%%)\n", j, i, out_data[i][j], expected, error);
                exit(1);
            }
            if(in_data[i][j] != input){
                printf("Failed to verify that input data is unchanged. Element %ld in tensor %d was %e but we expected %e\n", j, i, in_data[i][j], input);
                exit(1);
            }
        }
    }
    printf("Data verified successfully exiting main program\n");
}