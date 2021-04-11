/**
 * SwitchML Project
 * @file stats.h
 * @brief Declares the statistics class.
 */

#ifndef SWITCHML_STATISTICS_H_
#define SWITCHML_STATISTICS_H_

#include <vector>
#include <string>

#include "common.h"


namespace switchml {

/**
 * @brief A class that groups up all statistics.
 * 
 * The class does no attempt to syncrhonize in any of its calls.
 * It is the user classes responsibility to synchronize when needed. 
 */
class Stats {
  public:
    
    /**
     * @brief Initialize all members.
     * 
     * InitStats() must be called before any of the stats functions are used.
     * 
     * @see InitStats()
     */
    Stats();

    /**
     * @brief Cleans up the memory that has been allocated by InitStats()
     * 
     * @see InitStats()
     */
    ~Stats();

    Stats(Stats const&) = delete;
    void operator=(Stats const&) = delete;

    Stats(Stats&&) = default;
    Stats& operator=(Stats&&) = default;

    /**
     * @brief Dynamically allocate necessary objects and reset all stats using ResetStats().
     * 
     * @param [in] num_worker_threads The number of worker threads which will use this stats object.
     */
    void InitStats(WorkerTid num_worker_threads);

    /**
     * @brief Parse and log all of the statistics using glog
     */
    void LogStats();

    /**
     * @brief Clear all accumulated statistics
     */
    void ResetStats();

    /**
     * @brief Describe the distribution of a list of integers.
     * 
     * Computes sum, mean, max, min, median, stdev
     * 
     * @param list The list of integers to describe
     * @return std::string a single line with all of the metrics.
     */
    std::string DescribeIntList(std::vector<uint64_t> list);

    /**
     * @brief Describe the distribution of a list of doubles.
     * 
     * Computes sum, mean, max, min, median, stdev
     * 
     * @param list The list of doubles to describe
     * @return std::string a single line with all of the metrics.
     */
    std::string DescribeFloatList(std::vector<double> list);

    template<typename T>
    /**
     * @brief Create a string representation of a list.
     * 
     * @param list The vector representing the list
     * @return std::string A single line with all of the elements
     */
    std::string List2Str(std::vector<T> list);

    inline void IncJobsSubmittedNum() {
        this->jobs_submitted_num_++;
    }

    inline void AppendJobSubmittedNumel(uint64_t size) {
        this->jobs_submitted_numel_.push_back(size);
    }

    inline void IncJobsFinishedNum() {
        this->jobs_finished_num_++;
    }

    inline void AddTotalPktsSent(WorkerTid wtid, uint64_t to_add) {
        this->total_pkts_sent_[wtid] += to_add;
    }

    inline void AddCorrectPktsReceived(WorkerTid wtid, uint64_t to_add) {
        this->correct_pkts_received_[wtid] += to_add;
    }

    inline void AddWrongPktsReceived(WorkerTid wtid, uint64_t to_add) {
        this->wrong_pkts_received_[wtid] += to_add;
    }

#ifdef TIMEOUTS
    inline void AddTimeouts(WorkerTid wtid, uint64_t to_add) {
        this->timeouts_num_[wtid] += to_add;
    }
#endif
    const WorkerTid num_worker_threads_;

  private:
    /** The total number of jobs submitted to the context */
    uint64_t jobs_submitted_num_;

    /** A list of all the job sizes submitted to the context */
    std::vector<uint64_t> jobs_submitted_numel_;

    /** The total number of jobs finished */
    uint64_t jobs_finished_num_;

    // What follows are per worker thread statistics so we use an array to keep the stats
    // stat_name[0] for worker thread 0 stat_name[1] for worker thread 1 and so on.

    uint64_t* total_pkts_sent_;

    uint64_t* wrong_pkts_received_;

    uint64_t* correct_pkts_received_;

#ifdef TIMEOUTS
    uint64_t* timeouts_num_;
#endif
};

} // namespace switchml

#endif // SWITCHML_STATISTICS_H_