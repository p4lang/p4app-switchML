/**
 * SwitchML Project
 * @file main.cc
 * @brief Implements the allreduce benchmark.
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

#include <fstream>
#include <vector>
#include <utility> // std::pair
#include <stdexcept> // std::runtime_error
#include <sstream> // std::stringstream

namespace po = boost::program_options;

struct TestConfig{
    std::string model_path;
    std::string device;
    uint32_t num_iters;
    uint32_t num_warmup;
    bool verify;
    float allowed_error_percentage;
    bool random;
    uint32_t seed;
};

struct Layer {
    uint64_t numel;
    uint64_t forward_pass_ns;
    uint64_t backward_pass_ns;
    std::shared_ptr<switchml::Job> allreduce_job;
    float* data;
};

struct Model {
    uint64_t total_numel;
    uint64_t num_layers;
    Layer* layers;
    float* data;
};

volatile bool stop = false;

std::vector<std::pair<std::string, std::vector<uint64_t>>> read_csv(std::string filename){
    // Reads a CSV file into a vector of <string, vector<int>> pairs where
    // each pair represents <column name, column values>

    // Create a vector of <string, int vector> pairs to store the result
    std::vector<std::pair<std::string, std::vector<uint64_t>>> result;

    // Create an input filestream
    std::ifstream myFile(filename);

    // Make sure the file is open
    if(!myFile.is_open()) throw std::runtime_error("Could not open file");

    // Helper vars
    std::string line, colname;
    int val;

    // Read the column names
    if(myFile.good())
    {
        // Extract the first line in the file
        std::getline(myFile, line);

        // Create a stringstream from line
        std::stringstream ss(line);

        // Extract each column name
        while(std::getline(ss, colname, ',')){
            
            // Initialize and add <colname, int vector> pairs to result
            result.push_back({colname, std::vector<uint64_t> {}});
        }
    }

    // Read data, line by line
    while(std::getline(myFile, line))
    {
        // Create a stringstream of the current line
        std::stringstream ss(line);
        
        // Keep track of the current column index
        int colIdx = 0;
        
        // Extract each integer
        while(ss >> val){
            
            // Add the current integer to the 'colIdx' column's values vector
            result.at(colIdx).second.push_back(val);
            
            // If the next token is a comma, ignore it and move on
            if(ss.peek() == ',') ss.ignore();
            
            // Increment the column index
            colIdx++;
        }
    }

    // Close file
    myFile.close();

    return result;
}

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
        ("model-path", po::value<std::string>(&tconf.model_path), 
            "Path to the CSV file that contains the model layers information.")
        ("device", po::value<std::string>(&tconf.device)->default_value("cpu"), 
#ifdef CUDA
            "Allocate the tensors on the specified device. Choose from [cpu, gpu]")
#else
            "Allocate the tensors on the specified device. Choose from [cpu]")
#endif
        ("num-iters", po::value<uint32_t>(&tconf.num_iters)->default_value(10),
            "How many iterations should we run")
        ("num-warmup-iters", po::value<uint32_t>(&tconf.num_warmup)->default_value(5),
            "How many warmup iterations should we run")
        ("verify", po::value<bool>(&tconf.verify)->default_value(false), 
            "Verify results to make sure they are as expected")
        ("err", po::value<float>(&tconf.allowed_error_percentage)->default_value(1), 
            "The allowed error percentage. Used when verify is set to true")
        ("random", po::value<bool>(&tconf.random)->default_value(false), 
            "Initialize the data with random values.")
        ("seed", po::value<uint32_t>(&tconf.seed)->default_value(0), 
            "If you want to fix the seed of the random generator (In case you set random to true). Set to 0 to set to a random seed.")
    ;

    po::store(po::command_line_parser(argc, argv).options(test_options).run(), vm);
    po::notify(vm);

    if(vm.count("help")) {
        std::cout << test_options << std::endl;
        exit(EXIT_SUCCESS);
    }

    // Verify arguments
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
    if (tconf.num_iters <= 0) {
        std::cout << "The number of iterations must be greater than 0. '" << tconf.num_iters << "' is not valid" << std::endl;
        exit(EXIT_FAILURE);
    }
    if (tconf.allowed_error_percentage < 0) {
        std::cout << "The allowed error percentage must be greater than or equal to 0. '" << tconf.allowed_error_percentage << "' is not valid" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Read and construct model
    Model model = {};
    std::vector<std::pair<std::string, std::vector<uint64_t>>> csv_model = read_csv(tconf.model_path);
    
    model.num_layers = csv_model[0].second.size();
    model.layers = new Layer[model.num_layers];
    for (size_t layer_index = 0; layer_index < model.num_layers; layer_index++)
    {
        Layer& layer = model.layers[layer_index];
        layer.allreduce_job = nullptr;
        layer.numel = csv_model[1].second[layer_index];
        layer.forward_pass_ns = csv_model[2].second[layer_index];
        layer.backward_pass_ns = csv_model[3].second[layer_index];
        model.total_numel += layer.numel;
    }


    // Get context instance
    switchml::Context& ctx = switchml::Context::GetInstance();

    // Allocate and initialize data.
    float* cpu_data;
    float* cpu_ctrl_data;
#ifdef CUDA
    float* gpu_data;
#endif

    // Allocate and initialize cpu buffers
    if(tconf.random) {
        if(tconf.seed == 0) {
            tconf.seed = time(NULL);
        }
        srand(tconf.seed);
        std::cout << "Using random seed " << tconf.seed << std::endl;
    }
    cpu_data = new float[model.total_numel];
    cpu_ctrl_data = new float[model.total_numel];

    if(tconf.random) {
        for (uint64_t i = 0; i < model.total_numel; i++)
        {
            int r = rand();
            int exponent = r%254; // We avoid 255 because we do not want +INF -INF or NaN
            int sign = r%2;
            int mantissa = r%(1<<23);
            int float_bits = (sign << 31) | (exponent << 23) | mantissa;
            cpu_data[i] = *reinterpret_cast<float*>(&float_bits);
        }
    } else {
        int sign = 1;
        for (uint64_t i = 0; i < model.total_numel; i++)
        {
            cpu_data[i] = float(i) * sign;
            sign *= -1;
        }
    }
    memcpy(cpu_ctrl_data, cpu_data, model.total_numel * sizeof(float));
    model.data = cpu_data;

#ifdef CUDA
    if(tconf.device == "gpu") {
        cudaMalloc(&gpu_data, model.total_numel * sizeof(float));
        cudaMemcpy(gpu_data, cpu_data, model.total_numel*switchml::DataTypeSize(switchml_data_type),
                   cudaMemcpyKind::cudaMemcpyHostToDevice);
        model.data = gpu_data;
    }
#endif

    uint64_t numel_offset = 0;
    for (size_t layer_index = 0; layer_index < model.num_layers; layer_index++) {
        Layer& layer = model.layers[layer_index];
        layer.data = model.data + numel_offset;
        numel_offset += layer.numel;
    }

    // Start the context
    ctx.Start();

    // Start training
    std::vector<uint64_t> durations_ns;
    std::chrono::time_point<switchml::clock> begin = switchml::clock::now();
    std::chrono::time_point<switchml::clock> end;
    for (size_t i = 0; i < tconf.num_iters + tconf.num_warmup; i++)
    {
        // Forward Pass
        for (size_t layer_index = 0; layer_index < model.num_layers; layer_index++)
        {
            Layer& layer = model.layers[layer_index];
            if(layer.allreduce_job != nullptr) {
                layer.allreduce_job->WaitToComplete();
                if(stop) exit(EXIT_SUCCESS);
            }
            std::this_thread::sleep_for(std::chrono::nanoseconds(layer.forward_pass_ns));
            if(stop) exit(EXIT_SUCCESS);
        }
        // Backward Pass
        for (int64_t layer_index = model.num_layers-1; layer_index >= 0; layer_index--)
        {
            Layer& layer = model.layers[layer_index];
            std::this_thread::sleep_for(std::chrono::nanoseconds(layer.backward_pass_ns));
            if(stop) exit(EXIT_SUCCESS);
            // Launch communication in the background.
            layer.allreduce_job = ctx.AllReduceAsync(layer.data, layer.data, layer.numel, switchml::DataType::FLOAT32, switchml::AllReduceOperation::SUM);
        }
        if (i >= tconf.num_warmup) {
            end = switchml::clock::now();
            durations_ns.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count());
            begin = end;
            std::cout << "Iteration #" << i << "# finished. Duration: #" << durations_ns[durations_ns.size()-1] << "# ns Rate: #" 
                << 1.0e9/durations_ns[durations_ns.size()-1] << "# iter/s." << std::endl;
        }
    }

    ctx.WaitForAllJobs();
    if(stop) exit(EXIT_SUCCESS);
    std::cout << "Training finished." << std::endl;

    // Verification
    if (tconf.verify) {
        std::cout << "Verifying final results" << std::endl;

#ifdef CUDA
        if(tconf.device == "gpu") {
            cudaMemcpy(cpu_data, gpu_data, model.total_numel * sizeof(float),
                    cudaMemcpyKind::cudaMemcpyDeviceToHost);
        }
#endif
        int max_num_errors = 10;

        float output_multiplier = std::pow(ctx.GetConfig().general_.num_workers, tconf.num_iters + tconf.num_warmup);
        for(uint64_t j = 0; j < model.total_numel && max_num_errors > 0; j++) {
            float expected_output = cpu_ctrl_data[j] * output_multiplier;
            
            float error = (expected_output-cpu_data[j]) / expected_output * 100;
            if(error > tconf.allowed_error_percentage) {
                printf("Verification error at buffer index [%ld]. Expected %e but found %e (%.2f%% error).\n", j, expected_output, cpu_data[j], error);
                max_num_errors--;
            }
        }
        if(max_num_errors == 10) {
            std::cout << "Data verified successfully." << std::endl;
        } else {
            std::cout << "Verification failed. There could be more errors but we do not print more than 10." << std::endl;
        }
    }
    
    // Print our statistics
    // Min-Max
    uint64_t minmax = *std::min_element(durations_ns.begin(), durations_ns.end());
    double rate = 1.0e9 / minmax;
    std::cout << std::endl << std::endl;
    std::cout << "Min " << minmax << " ns " << rate << " iter/s" << std::endl;
    minmax = *std::max_element(durations_ns.begin(), durations_ns.end());
    rate = 1.0e9 / minmax;
    std::cout << "Max " << minmax << " ns " << rate << " iter/s" << std::endl;

    // Median
    size_t median_idx = durations_ns.size() / 2;
    std::nth_element(durations_ns.begin(), durations_ns.begin() + median_idx, durations_ns.end());
    uint64_t median = durations_ns[median_idx];
    rate = 1.0e9 / median;
    std::cout << "Median " << median << " ns " << rate << " iter/s" << std::endl;

    // Mean
    uint64_t sum = std::accumulate(durations_ns.begin(), durations_ns.end(), (uint64_t)0);
    double mean = sum / double(durations_ns.size());
    rate = 1.0e9 / mean;
    std::cout << "Mean " << (uint64_t)mean << " ns " << rate << " iter/s"
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

    delete [] cpu_data;
    delete [] cpu_ctrl_data;
#ifdef CUDA
    if (tconf.device == "gpu") {
        cudaFree(gpu_data);
    }
#endif
    stop = true;
    signal_handler_thread.join();
}