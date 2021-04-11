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

    uint64_t numel = (1 << 15) + 31412; // The tensors are stored on the stack so there might be some limits on the size.
    int num_tensors = 8;
    int num_workers = ctx.GetConfig().general_.num_workers;
    printf("Allocating data\n");
    float in_data[num_tensors][numel];
    float out_data[num_tensors][numel];
    // Init data
    printf("Initializing data\n");
    for(int i = 0; i < num_tensors; i++) {
        for(uint64_t j = 0; j < numel; j++) {
            in_data[i][j] = (i*numel+j) / 1.24; // Use any random numbers
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
            float input = (i*numel+j) / 1.24; // Use the same formula you used when you initialized
            float expected = input * num_workers;
            if( out_data[i][j] != expected) {
                printf("Failed to verify output data. Element %ld in tensor %d was %e but we expected %e\n", j, i, out_data[i][j], expected);
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