/**
 * SwitchML Project
 * @file stats.cc
 * @brief Implements the statistics class.
 */

#include "stats.h"

#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>

#include "common.h"

namespace switchml {

Stats::Stats():
    num_worker_threads_(0),
    jobs_submitted_num_(0),
    jobs_submitted_numel_(),
    jobs_finished_num_(0),
    total_pkts_sent_(nullptr),
    wrong_pkts_received_(nullptr),
    correct_pkts_received_(nullptr)
#ifdef TIMEOUTS
    ,timeouts_num_(nullptr)
#endif
{
    // Do nothing
}

Stats::~Stats() {
    delete [] this->total_pkts_sent_;
    delete [] this->wrong_pkts_received_;
    delete [] this->correct_pkts_received_;
#ifdef TIMEOUTS
    delete [] this->timeouts_num_;
#endif
}

void Stats::InitStats(WorkerTid num_worker_threads) {
    LOG_IF(FATAL, total_pkts_sent_ != nullptr) << "Trying to initialize stats twice";
    this->num_worker_threads_ = num_worker_threads;
    total_pkts_sent_ = new uint64_t[num_worker_threads];
    wrong_pkts_received_ = new uint64_t[num_worker_threads];
    correct_pkts_received_ = new uint64_t[num_worker_threads];
#ifdef TIMEOUTS
    timeouts_num_ = new uint64_t[num_worker_threads];
#endif
    this->jobs_submitted_numel_.reserve(1024); // Start with a big number to improve performance
    ResetStats();
}

void Stats::LogStats() {
    std::ostringstream output;

    // Add global stats
    output << "Stats: "
        << "\n    Submitted jobs: #" << this->jobs_submitted_num_ << "#"
        << "\n    Submitted jobs sizes: #" << List2Str(this->jobs_submitted_numel_) << "#"
        << "\n    Submitted jobs sizes distribution: #" << DescribeIntList(this->jobs_submitted_numel_) << "#"
        << "\n    Finished jobs: #" << this->jobs_finished_num_ << "#"
    ;

    // Add per worker stats
    std::vector<uint64_t> ls;
    for (WorkerTid i = 0; i < this->num_worker_threads_; i++) {
        output
            << "\n    Worker thread: #" << i << "#"
            << "\n        Total packets sent: #" << total_pkts_sent_[i] << "#"
            << "\n        Total packets received: #" << wrong_pkts_received_[i] + correct_pkts_received_[i] << "#"
            << "\n        Wrong packets received: #" << wrong_pkts_received_[i] << "#"
            << "\n        Correct packets received: #" << correct_pkts_received_[i] << "#"
#ifdef TIMEOUTS
            << "\n        Number of timeouts: #" << timeouts_num_[i] << "#"
#endif
        ;
    }

    VLOG(1) << output.str();
}

void Stats::ResetStats() {
    // Clear global stats
    this->jobs_submitted_num_ = 0;
    this->jobs_submitted_numel_.clear();
    this->jobs_finished_num_ = 0;

    // Clear worker thread stats
    for (WorkerTid i = 0; i < this->num_worker_threads_; i++)
    {
        this->total_pkts_sent_[i] = 0;
        this->wrong_pkts_received_[i] = 0;
        this->correct_pkts_received_[i] = 0;
#ifdef TIMEOUTS
        this->timeouts_num_[i] = 0;
#endif
    }
}

std::string Stats::DescribeIntList(std::vector<uint64_t> list) {
    if(list.size() == 0) {
        return "";
    }
    // Sum
    uint64_t sum = std::accumulate(list.begin(), list.end(), (uint64_t)0);
    // Mean
    double mean = sum / list.size();
    // Max
    uint64_t max = *std::max_element(list.begin(), list.end());
    // Min
    uint64_t min = *std::min_element(list.begin(), list.end());
    // Median
    size_t median_idx = list.size() / 2;
    std::nth_element(list.begin(), list.begin() + median_idx, list.end());
    uint64_t median = list[median_idx];
    // Standard deviation
    std::vector<double> diff(list.size());
    std::transform(list.begin(), list.end(), diff.begin(), [mean](double x) { return x - mean; });
    double sq_sum = std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
    double std_dev = std::sqrt(sq_sum / double(list.size()));

    char buffer[200];
    sprintf(buffer, "Sum: %-10ld Mean: %-10.4f Max: %-10ld Min: %-10ld Median: %-10ld Stdev: %-10.4f",
            sum, mean, max, min, median, std_dev);
    return std::string(buffer);
}

std::string Stats::DescribeFloatList(std::vector<double> list) {
    if(list.size() == 0) {
        return "";
    }
    // Sum
    double sum = std::accumulate(list.begin(), list.end(), (double)0);
    // Mean
    double mean = sum / list.size();
    // Max
    double max = *std::max_element(list.begin(), list.end());
    // Min
    double min = *std::min_element(list.begin(), list.end());
    // Median
    size_t median_idx = list.size() / 2;
    std::nth_element(list.begin(), list.begin() + median_idx, list.end());
    double median = list[median_idx];
    // Standard deviation
    std::vector<double> diff(list.size());
    std::transform(list.begin(), list.end(), diff.begin(), [mean](double x) { return x - mean; });
    double sq_sum = std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
    double std_dev = std::sqrt(sq_sum / double(list.size()));

    char buffer[200];
    sprintf(buffer, "Sum: %-10.4f Mean: %-10.4f Max: %-10.4f Min: %-10.4f Median: %-10.4f Stdev: %-10.4f",
            sum, mean, max, min, median, std_dev);
    return std::string(buffer);
}

template<typename T>
std::string Stats::List2Str(std::vector<T> list) {
    std::ostringstream output;
    output << "[";
    for(size_t i = 0; i < list.size(); i++) {
        output << list[i] << ",";
    }
    output << "]";
    return output.str();
}

template std::string Stats::List2Str(std::vector<uint64_t> list);
template std::string Stats::List2Str(std::vector<double> list);

} // namespace switchml