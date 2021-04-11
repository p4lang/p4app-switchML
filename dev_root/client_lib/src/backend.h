/**
 * SwitchML Project
 * @file backend.h
 * @brief Declares the Backend interface.
 */

#ifndef SWITCHML_BACKEND_H_
#define SWITCHML_BACKEND_H_

#include <vector>
#include <memory>

#include "common.h"
#include "config.h"

namespace switchml {

// Forward declare the context so that we can store its reference as a member
class Context;

/**
 * @brief An interface that describes the backend.
 * 
 * A backend is the class responsible for creating worker threads
 * and actually carrying out the jobs submitted by performing the communication.
 */
class Backend {
  public:

    /**
     * @brief Factory function to create a backend instance based on the configuration passed.
     * 
     * @param [in] context a reference to the switchml context.
     * @param [in] config a reference to the switchml configuration.
     * @return std::unique_ptr<Backend> a unique pointer to the created backend object.
     */
    static std::unique_ptr<Backend> CreateInstance(Context& context, Config& config);

    ~Backend() = default;
    
    Backend(Backend const&) = delete;
    void operator=(Backend const&) = delete;

    Backend(Backend&&) = default;
    Backend& operator=(Backend&&) = default;

    /**
     * @brief Initializes backend specific variables and starts worker threads.
     * @see CleanupWorker()
     */
    virtual void SetupWorker() = 0;

    /**
     * @brief Cleans up all worker state and **waits** for the worker threads to exit.
     * @see SetupWorker()
     */
    virtual void CleanupWorker() = 0;
  
  protected:
    /**
     * @brief Initializes the members with the passed references.
     * 
     * Must be called explicitly by all subclass constructors.
     * 
     * @param [in] context The context
     * @param [in] config The context configuration.
     */
    Backend(Context& context, Config& config);

    /** A reference to the context */
    Context& context_;
    /** A reference to the context configuration */
    Config& config_;
};

} // namespace switchml

#endif // SWITCHML_BACKEND_H_