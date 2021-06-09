# Contributing
The SwitchML project welcomes all contributions. But before trying to contribute please read through the following guidelines specifically the ones concerning the part that you would like to tackle. It will make you understand the design, style, and conventions used faster and get you up to speed on what you should adhere to and what you should be aware of when contributing. 

After doing that, please open up an issue on Github whether you wanted to fix a bug, implement a new feature...etc describing exactly what you plan to do. This will allow the team to give you feedback and discuss the various dimensions of the contribution. It will also make it all the more likely for your contribution to be pulled into the main repo.

**Table of contents**

- [General Guidelines](#general-guidelines)
	- [Coding Style Conventions](#coding-style-conventions)
- [Client Library](#client-library)
	- [How the different classes interact](#how-the-different-classes-interact)
	- [Logging](#logging)
- [P4](#p4)
- [Controller](#controller)
- [Examples](#examples)
- [Benchmarks](#benchmarks)
- [Documentation](#documentation)

## General Guidelines

When proposing a code change or adding a new feature, one must take into account all of the code that may have relied on the old code and update it accordingly. 

This is why API and interface changes of the different components/classes can be much more difficult to add and requires lots of vigilance. Changing something in the P4 program may require changes in both the client library backend and the python controller. Changing something in the client library context may require changes to all framework integration methods and many examples and benchmarks. So it is important to keep all of these dependencies in mind when changing how a component interfaces with other components. 

On the other hand, editing the implementation of components / functions to either fix a bug, improve performance, or enhance readability is much easier to accommodate and accept. 

Similarly, adding new features or functions that do not break any of the other components is also easy to accommodate and include given enough justification for the feature and showing that it is within the scope of SwitchML.

### Coding Style Conventions
When writing code for SwitchML, there is a few conventions that you should adhere to to ensure consistency and readability across the project depending on the type of file that you are working on.

### Header

We adopt the following header for all types of files

	# SwitchML Project
	# @file the name of the file
	# @brief a one line description of the file
	#
	# More details if needed

Whether you use `#`, or doxygen's `/** */` depends on the type of file that you are documenting.

#### C++
For documentation we adopt the javadoc [doxygen style](https://www.doxygen.nl/manual/docblocks.html) using `/** */` to enable us to generate automatic documentation. For everything else we adopt the [Google C++ Style](https://google.github.io/styleguide/cppguide.html) with the exception of indentation where we use 4 spaces for each level as
we feel that improves readability.

#### Python
...  
#### P4
...


## SwitchML Client Library

### How the different classes interact

Everything starts with the  **Context**  class. The Context is a singleton class that provides all SwitchML services to the user through its simplified API. Firstly, a user gets an instance of the context then starts it through the  _Start()_  function. During initialization the following occurs: 

1. A  **Config**  object is created to represent all of the options specified in the `switchml.cfg`  file. Optionally you can create this object yourself and pass it to the **Context** to avoid using configuration files. 

2. A **Backend** object is created (dependending on the backend chosen in the **Config**) which typically starts **WorkerThread**s that poll the context for work and carry it out. 

3. A **Scheduler** object is created (dependending on the scheduler chosen in the **Config**) which the context uses to dispatch **JobSlice**s to the **WorkerThread**s.

After initialization we will have a number of  Backend specific **WorkerThread**s (depending on the number set in the  **Config**) constantly asking the  **Context**  for work. A user can then submit work (Ex. an all reduce operation) through the **Context**. The  **Context**  then creates a  **Job**  out of this function call and submits it to its internal **Scheduler**. When the continuously running  **WorkerThread**s ask the  **Context** for work to do, the **Job** is divided into **JobSlice**s by the scheduler which are then dispatched to the  **WorkerThread**s. Each **WorkerThread** receives a  **JobSlice** then it performs the work necessary  to perform the actual communication. Finally the  **WorkerThread** notifies the **Context** when the **JobSlice** work is completed. When all  **JobSlice**s of a **Job** are completed, the context notifies any threads waiting on this **Job**.

**WorkerThread**s utilize a  **PrePostProcessor**  to load and unload the data from and into packets or messages. The  **WorkerThread**  never actually reads the client's data or writes into the client's buffers. That is the sole job of the  **PrePostProcessor**

### Logging
For logging we use the [glog](https://github.com/google/glog) library 

We have specific conventions for logging so make sure that you adhere to these conventions when writing your code. If you have a convincing reason on why you did not, please clarify that in your pull request.

| Severity | Usage |
|:--:|--|
| INFO | General information that the user might be interested in. Refer to the verbosity table for more details as we never use `LOG(INFO)` but instead `VLOG(verbosity)` for logging informational messages |
| WARNING| Alert the user to a potential problem in his configuration or setup |
| ERROR | Notify the user about a definite problem in their configuration or environment but SwitchML can remedy the error and continue on |
| FATAL | Notify the user about a serious unrecoverable problem that occurred after which the application will exit. We use both `LOG(FATAL)` and `CHECK(condition)` to signal problems of this magnitude |

We include all logging statements with severity higher than INFO in both the debugging and release builds. For the informational messages, whether they are included or not depends on the verbosity as will be illustrated in what follows.
| Verbosity| Usage|
|:--:|--|
| 0 | General initialization and cleanup information mostly produced by the context itself that all SwitchML users can understand |
| 1 | Initialization and cleanup information of all classes in the client library. This includes backend initialization and cleanup, worker threads initialization and cleanup, dumping usage stats..etc. |
| 2 | Information about job submission, scheduling, and job completion. |
| 3 | Information about packets and messages being sent and received |
| 4 | Information about the contents of the data itself that is being processed |

For verbosity levels <= 1, you should use `VLOG` since initialization and cleanup will not affect performance during the lifetime of the application.
On the other hand, for verbosity levels > 1, you should use `DVLOG` since we do not want these statements to be included except in a DEBUG build as they can affect performance significantly.

## SwitchML P4 Program

...

## SwitchML Controller

...

## Examples

Examples are meant to be step by step documented small programs to illustrate the usage of some component of switchml.
We always welcome new well written and well documented examples that make understanding the components of the library a little more easier.
We also welcome improvements of existing examples whether in documentation or the code/comments themselves.

## Benchmarks

Benchmarks are meant to measure the speed and correctness of switchml as a whole or for specific parts of it (For ex. schedulers, prepostprocessors...etc.).
We always welcome new benchmarks that help us evaluate the different parts of switchml in terms of speed and correctness.
We also welcome improvements of existing examples whether in documentation or the code/comments themselves.

## Documentation
Contributing to the documentation is always welcome. If you found an error, feel that you can explain something in a better way, or wanted to add new guides/pages that will help new users/developers understand switchml, then by all means don't hesitate to write your changes and open a pull request. However we ask that you try to adhere to the implicit conventions and 'feel' of the existing documentation.

Switchml is still in its early development stages and documentation can become outdated quite fast. While us the developers can get accustomed and miss some of these outdated portions, new users will certainly not, which makes a new user (perhaps yourself) a very important asset in helping us keep the documentation up to date.