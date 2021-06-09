/**
 * SwitchML Project
 * @file main.cc
 * @brief Implements the allreduce_test example.
 */

#include <switchml/context.h>

#ifdef CUDA
#include <cuda_runtime.h>
#endif
#include <boost/program_options.hpp>
#include <string>
#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <signal.h>
#include <future>

namespace po = boost::program_options;

struct TestConfig{
    uint64_t tensor_numel;
    std::string tensor_type;
    std::string device;
    uint32_t num_jobs;
    uint32_t num_warmup;
    uint32_t sync_every;
    bool verify;
    float allowed_error_percentage;
    bool random;
    uint32_t seed;
    bool dump_stats_per_sync;
};

volatile bool stop = false;

void signal_handler() {
    std::cout << "Signal handler thread started. Waiting for any signals." << std::endl;
    siginfo_t si;
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    // wait until a signal is delivered:
    struct timespec ts;
    ts.tv_nsec = 0;
    ts.tv_sec = 3;
    while(!stop) {
        if(sigtimedwait(&sigset, &si, &ts) != -1) {
            std::cout << "Signal " << si.si_signo << " received, preparing to exit..." << std::endl;
            switchml::Context::GetInstance().Stop();
            stop = true;
        }
    }
    std::cout << "Signal handler thread is exiting" << std::endl;
}

int main(int argc, char* argv[]) {
    // block SIGINT and SIGTERM signals in this thread and subsequently spawned threads
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &sigset, nullptr);

    // Launch the signal handler as a seperate thread.
    // We do this because if we register a normal signal handler using signal or sigaction
    // then a lot of the multithreaded syncrhonization primitives cannot be called safely.
    // for example notifying condition variables is not signal safe and this is used by context Stop().
    // So instead we launch a seperate normal thread that waits for a signal and then stops the context.
    std::thread signal_handler_thread(signal_handler);

    TestConfig tconf;

    std::stringstream help_msg;
    po::variables_map vm;
    po::options_description test_options("Allreduce Test");
    test_options.add_options()
        ("help,h", "Display this help message")
        ("tensor-numel", po::value<uint64_t>(&tconf.tensor_numel)->default_value(268435456), 
            "Number of elements to all reduce.")
        ("tensor-type", po::value<std::string>(&tconf.tensor_type)->default_value("int32"), 
            "Specify the data type to use. Choose from [float, int32].")
        ("device", po::value<std::string>(&tconf.device)->default_value("cpu"), 
#ifdef CUDA
            "Allocate the tensors on the specified device. Choose from [cpu, gpu]")
#else
            "Allocate the tensors on the specified device. Choose from [cpu]")
#endif
        ("num-jobs", po::value<uint32_t>(&tconf.num_jobs)->default_value(10),
            "How many timed all reduce jobs should we submit?")
        ("num-warmup-jobs", po::value<uint32_t>(&tconf.num_warmup)->default_value(5),
            "How many untimed all reduce jobs should we submit before the timed ones?")
        ("verify", po::value<bool>(&tconf.verify)->default_value(false), 
            "Verify results to make sure they are as expected")
        ("sync-every", po::value<uint32_t>(&tconf.sync_every)->default_value(1), 
            "When to wait for the submitted all reduce jobs to finish?. Set to 0 to wait only after you submit all of the jobs.")
        ("err", po::value<float>(&tconf.allowed_error_percentage)->default_value(1), 
            "The allowed error percentage. Used when verify is set to true")
        ("random", po::value<bool>(&tconf.random)->default_value(false), 
            "Initialize the data with random values.")
        ("seed", po::value<uint32_t>(&tconf.seed)->default_value(0), 
            "If you want to fix the seed of the random generator (In case you set random to true). Set to 0 to set to a random seed.")
        ("dump-stats", po::value<bool>(&tconf.dump_stats_per_sync)->default_value(false), 
            "Should we print out and clear the switchml statistics after each sync?")
    ;

    po::store(po::command_line_parser(argc, argv).options(test_options).run(), vm);
    po::notify(vm);

    if(vm.count("help")) {
        std::cout << test_options << std::endl;
        exit(EXIT_SUCCESS);
    }

    // Verify arguments
    if (tconf.tensor_numel <= 0) {
        std::cout << "The number of tensor elements must be greater than 0. '" << tconf.tensor_numel << "' is not valid" << std::endl;
        exit(EXIT_FAILURE);
    }
    if (tconf.tensor_type != "float" && tconf.tensor_type != "int32") {
        std::cout << "'" << tconf.tensor_type << "' is not a valid tensor type. Choose from [float, int32]" << std::endl;
        exit(EXIT_FAILURE);
    }
    if (tconf.device != "cpu"
#ifdef CUDA
       && tconf.device != "gpu"
#endif
    ) {
#ifdef CUDA
        std::cout << "'" << tconf.device << "' is not a valid device. Choose from [gpu, cpu]" << std::endl;
#else
        std::cout << "'" << tconf.device << "' is not a valid device. Choose from [cpu]" << std::endl;
#endif
        exit(EXIT_FAILURE);
    }
    if (tconf.num_jobs <= 0) {
        std::cout << "The number of jobs must be greater than 0. '" << tconf.num_jobs << "' is not valid" << std::endl;
        exit(EXIT_FAILURE);
    }
    if (tconf.allowed_error_percentage < 0) {
        std::cout << "The allowed error percentage must be greater than or equal to 0. '" << tconf.num_jobs << "' is not valid" << std::endl;
        exit(EXIT_FAILURE);
    }

    switchml::Context& ctx = switchml::Context::GetInstance();

    // Allocate and initialize data.
    switchml::DataType switchml_data_type;
    void* cpu_src_data;
    void* cpu_dst_data;
    void* cpu_ctrl_data;
#ifdef CUDA
    void* gpu_src_data;
    void* gpu_dst_data;
#endif

    // Allocate and initialize cpu buffers
    if(tconf.random) {
        if(tconf.seed == 0) {
            tconf.seed = time(NULL);
        }
        srand(tconf.seed);
        std::cout << "Using random seed " << tconf.seed << std::endl;
    }
    if(tconf.tensor_type == "float") {
        switchml_data_type = switchml::DataType::FLOAT32;
        cpu_src_data = new float[tconf.tensor_numel];
        cpu_dst_data = new float[tconf.tensor_numel];
        cpu_ctrl_data = new float[tconf.tensor_numel];
        float* float_cpu_src_data = static_cast<float*>(cpu_src_data);
        float* float_cpu_dst_data = static_cast<float*>(cpu_dst_data);
        if(tconf.random) {
            for (uint64_t i = 0; i < tconf.tensor_numel; i++)
            {
                int r = rand();
                int exponent = r%254; // We avoid 255 because we do not want +INF -INF or NaN
                int sign = r%2;
                int mantissa = r%(1<<23);
                int float_bits = (sign << 31) | (exponent << 23) | mantissa;
                float_cpu_src_data[i] = *reinterpret_cast<float*>(&float_bits);
            }
        } else {
            int sign = 1;
            for (uint64_t i = 0; i < tconf.tensor_numel; i++)
            {
                float_cpu_src_data[i] = float(i) * sign;
                sign *= -1;
            }
        }
        // Let's populate the destination with a fixed pattern that we can recognize and know
        // it has been changed or not.
        for(uint64_t i = 0; i < tconf.tensor_numel; i++) {
            float_cpu_dst_data[i] = 123456789;
        }
    } else if (tconf.tensor_type == "int32") {
        switchml_data_type = switchml::DataType::INT32;
        cpu_src_data = new int32_t[tconf.tensor_numel];
        cpu_dst_data = new int32_t[tconf.tensor_numel];
        cpu_ctrl_data = new int32_t[tconf.tensor_numel];
        int32_t* int32_cpu_src_data = reinterpret_cast<int32_t*>(cpu_src_data);
        int32_t* int32_cpu_dst_data = reinterpret_cast<int32_t*>(cpu_dst_data);
        if(tconf.random) {
            for (uint64_t i = 0; i < tconf.tensor_numel; i++)
            {
                int32_cpu_src_data[i] = rand() + rand(); // rand() is only from 0 to +INT_MAX. This way with overflow we can get to -INT_MAX as well
            }
        } else {
            int sign = 1;
            for (int32_t i = 0; i < (int32_t) tconf.tensor_numel; i++)
            {
                int32_cpu_src_data[i] = i * sign;
                sign *= -1;
            }
        }
        // Let's populate the destination with a fixed pattern that we can recognize and know
        // if it has been changed or not.
        for(uint64_t i = 0; i < tconf.tensor_numel; i++) {
            int32_cpu_dst_data[i] = 123456789;
        }
    } else {
        std::cout << "'" << tconf.tensor_type << "' is not a valid tensor type. Choose from [float, int32]" << std::endl;
        exit(EXIT_FAILURE);
    }

    void* src_data = nullptr;
    void* dst_data = nullptr;
    memcpy(cpu_ctrl_data, cpu_src_data,  switchml::DataTypeSize(switchml_data_type) * tconf.tensor_numel);
    if (tconf.device == "cpu") {
        src_data = cpu_src_data;
        dst_data = cpu_dst_data;
    }
#ifdef CUDA
    else if(tconf.device == "gpu") {
        cudaMalloc(&gpu_src_data, tconf.tensor_numel*switchml::DataTypeSize(switchml_data_type));
        cudaMalloc(&gpu_dst_data, tconf.tensor_numel*switchml::DataTypeSize(switchml_data_type));
        cudaMemcpy(gpu_src_data, cpu_src_data, tconf.tensor_numel*switchml::DataTypeSize(switchml_data_type),
                   cudaMemcpyKind::cudaMemcpyHostToDevice);
        src_data = gpu_src_data;
        dst_data = gpu_dst_data;
    }
#endif
    else {
#ifdef CUDA
        std::cout << "'" << tconf.device << "' is not a valid device. Choose from [gpu, cpu]" << std::endl;
#else
        std::cout << "'" << tconf.device << "' is not a valid device. Choose from [cpu]" << std::endl;
#endif
        exit(EXIT_FAILURE);
    }

    // Start the context
    ctx.Start();

    // Submit warmup jobs
    std::cout << "Submitting " << tconf.num_warmup << " warmup jobs." << std::endl;
    for (uint32_t i = 0; i < tconf.num_warmup; i++) {
        if(stop) exit(EXIT_SUCCESS);
        ctx.AllReduceAsync(src_data, dst_data, tconf.tensor_numel, switchml_data_type, switchml::AllReduceOperation::SUM);
    }
    ctx.WaitForAllJobs();
    std::cout << "Warmup finished." << std::endl;

    // Submit timed jobs
    std::cout << "Submitting " << tconf.num_jobs << " jobs." << std::endl;
    std::vector<unsigned long> durations_ns;
    std::chrono::time_point<switchml::clock> begin = switchml::clock::now();
    uint32_t jobs_before_sync = 0;
    for(uint32_t i = 0; i < tconf.num_jobs; i++) {
        if(stop) exit(EXIT_SUCCESS);
        ctx.AllReduceAsync(src_data, dst_data, tconf.tensor_numel, switchml_data_type, switchml::AllReduceOperation::SUM);
        jobs_before_sync++;
        if((i+1)%tconf.sync_every == 0) {
            ctx.WaitForAllJobs();
            if(stop) exit(EXIT_SUCCESS);
            durations_ns.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(switchml::clock::now() - begin).count());
            char job_str[40];
            if (jobs_before_sync > 1) {
                sprintf(job_str, "%d-%d", i-jobs_before_sync+1, i);
            } else {
                sprintf(job_str, "%d", i);
            }
            std::cout << "Job(s) #" << job_str << "# finished. Duration: #" << durations_ns[durations_ns.size()-1] << "# ns Goodput: #" 
                << tconf.tensor_numel*4.0*8*jobs_before_sync/durations_ns[durations_ns.size()-1] << "# Gbps." << std::endl;
            jobs_before_sync = 0;
            if(tconf.dump_stats_per_sync) {
                ctx.GetStats().LogStats();
                ctx.GetStats().ResetStats();
            }
            begin = switchml::clock::now();
        }
    }
    ctx.WaitForAllJobs();
    if(stop) exit(EXIT_SUCCESS);
    std::cout << "All jobs finished." << std::endl;

    // Verification
    if (tconf.verify) {
        std::cout << "Verifying final results" << std::endl;

#ifdef CUDA
        if(tconf.device == "gpu") {
            cudaMemcpy(cpu_src_data, gpu_src_data, tconf.tensor_numel*switchml::DataTypeSize(switchml_data_type),
                    cudaMemcpyKind::cudaMemcpyDeviceToHost);
            cudaMemcpy(cpu_dst_data, gpu_dst_data, tconf.tensor_numel*switchml::DataTypeSize(switchml_data_type),
                    cudaMemcpyKind::cudaMemcpyDeviceToHost);
            src_data = cpu_src_data;
            dst_data = cpu_dst_data;
        }
#endif
        int max_num_errors = 10;
        if(tconf.tensor_type == "float") {
            float* float_src_data = static_cast<float*>(src_data);
            float* float_dst_data = static_cast<float*>(dst_data);
            float* float_ctrl_data = static_cast<float*>(cpu_ctrl_data);
            float output_multiplier = ctx.GetConfig().general_.num_workers;
            for(uint64_t j = 0; j < tconf.tensor_numel && max_num_errors > 0; j++) {
                float expected_input = float_ctrl_data[j];
                float expected_output = expected_input * output_multiplier;
                float error = (expected_input-float_src_data[j]) / (expected_input + std::numeric_limits<float>::epsilon()) * 100; // We add epsilon to avoid running into division by 0
                if(error > tconf.allowed_error_percentage) {
                    printf("Verification error at input buffer index [%ld]. Expected %e but found %e (%.2f%% error).\n", j, expected_input, float_src_data[j], error);
                    max_num_errors--;
                }
                error = (expected_output-float_dst_data[j]) / expected_output * 100;
                if(error > tconf.allowed_error_percentage) {
                    printf("Verification error at output buffer index [%ld]. Expected %e but found %e (%.2f%% error).\n", j, expected_output, float_dst_data[j], error);
                    max_num_errors--;
                }
            }
        } else {
            int32_t* int32_src_data = static_cast<int32_t*>(src_data);
            int32_t* int32_dst_data = static_cast<int32_t*>(dst_data);
            int32_t* int32_ctrl_data = static_cast<int32_t*>(cpu_ctrl_data);
            int32_t output_multiplier = ctx.GetConfig().general_.num_workers;
            for(uint64_t j = 0; j < tconf.tensor_numel && max_num_errors > 0; j++) {
                int32_t expected_input = int32_ctrl_data[j];
                int32_t expected_output = expected_input * output_multiplier;
                float error = (expected_input-int32_src_data[j]) / (float(expected_input) + std::numeric_limits<float>::epsilon()) * 100; // We add epsilon to avoid running into division by 0
                if(error > tconf.allowed_error_percentage) {
                    printf("Verification error at input buffer index [%ld]. Expected %d but found %d (%.2f%% error).\n", j, expected_input, int32_src_data[j], error);
                    max_num_errors--;
                }
                error = (expected_output-int32_dst_data[j]) / float(expected_output) * 100;
                if(error > tconf.allowed_error_percentage) {
                    printf("Verification error at output buffer index [%ld]. Expected %d but found %d (%.2f%% error).\n", j, expected_output, int32_dst_data[j], error);
                    max_num_errors--;
                }
            }
        }
        if(max_num_errors == 10) {
            std::cout << "Data verified successfully." << std::endl;
        } else {
            std::cout << "Verification failed. There could be more errors but we do not print more than 10." << std::endl;
        }
    }
    
    // Print our statistics
    double num_bits = tconf.sync_every * tconf.tensor_numel * 4 * 8;
    // Min-Max
    uint64_t minmax = *std::min_element(durations_ns.begin(), durations_ns.end());
    double rate = double(num_bits) / minmax;
    std::cout << std::endl << std::endl;
    std::cout << "Min " << minmax << " ns " << rate << " Gbps" << std::endl;
    minmax = *std::max_element(durations_ns.begin(), durations_ns.end());
    rate = double(num_bits) / minmax;
    std::cout << "Max " << minmax << " ns " << rate << " Gbps" << std::endl;

    // Median
    size_t median_idx = durations_ns.size() / 2;
    std::nth_element(durations_ns.begin(), durations_ns.begin() + median_idx, durations_ns.end());
    uint64_t median = durations_ns[median_idx];
    rate = double(num_bits) / median;
    std::cout << "Median " << median << " ns " << rate << " Gbps" << std::endl;

    // Mean
    uint64_t sum = std::accumulate(durations_ns.begin(), durations_ns.end(), (uint64_t)0);
    double mean = sum / double(durations_ns.size());
    rate = num_bits / mean;
    std::cout << "Mean " << (uint64_t)mean << " ns " << rate << " Gbps"
                << std::endl;

    // Standard deviation
    std::vector<double> diff(durations_ns.size());
    transform(durations_ns.begin(), durations_ns.end(), diff.begin(),
                [mean](double x) { return x - mean; });
    double sq_sum = inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
    double std_dev = sqrt(sq_sum / double(durations_ns.size()));
    std::cout << "Std dev " << std_dev << " ns" << std::endl;

    // Cleanup
    std::cout << "Cleaning up." << std::endl;
    ctx.Stop();

    if(tconf.tensor_type == "float") {
        delete [] static_cast<float*>(cpu_src_data);
        delete [] static_cast<float*>(cpu_dst_data);
        delete [] static_cast<float*>(cpu_ctrl_data);
    } else {
        delete [] static_cast<int32_t*>(cpu_src_data);
        delete [] static_cast<int32_t*>(cpu_dst_data);
        delete [] static_cast<int32_t*>(cpu_ctrl_data);
    }
#ifdef CUDA
    if (tconf.device == "gpu") {
        cudaFree(gpu_src_data);
        cudaFree(gpu_dst_data);
    }
#endif
    stop = true;
    signal_handler_thread.join();
}