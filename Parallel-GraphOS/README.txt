First Download IntelSGX Driver from https://download.01.org/intel-sgx/ site for the target dirstribution: https://download.01.org/intel-sgx/linux-2.4/ubuntu16.04-server/

Install the above binary file in /opt/intel

Download source of IntelSGX from https://github.com/intel/linux-sgx: gir clone https://github.com/intel/linux-sgx.git


-----------------------
Purpose of Cxx11SGXDemo
-----------------------

The project demonstrates serveral C++11 features inside the Enclave:
- lambda expressions;
- rvalue references and move semantics;
- automatic type deduction with auto and decltype;
- nullptr type;
- strongly typed enum classes;
- Range-based for statements;
- static_assert keyword for compile-time assertion;
- initializer lists and uniform initialization syntax;
- New virtual function controls: override, final, default, and delete;
- delegating constructors;
- new container classes (unordered_set, unordered_map, unordered_multiset, and unordered_multimap);
- tuple class;
- function object wrapper;
- atomic, mutexes, condition_variables;
- new smart pointer classes: shared_ptr, unique_ptr;
- new c++ algorithms: all_of, any_of, none_of;
- variadic templates;
- SFINAE;

---------------------------------------------
How to Build/Execute the C++11 sample program
---------------------------------------------
1. Install Intel(R) Software Guard Extensions (Intel(R) SGX) SDK for Linux* OS
2. Make sure your environment is set:
    $ source ${sgx-sdk-install-path}/environment
3. Build the project with the prepared Makefile:
    a. Hardware Mode, Debug build:
        $ make
    b. Hardware Mode, Pre-release build:
        $ make SGX_PRERELEASE=1 SGX_DEBUG=0
    c. Hardware Mode, Release build:
        $ make SGX_DEBUG=0
    d. Simulation Mode, Debug build:
        $ make SGX_MODE=SIM
    e. Simulation Mode, Pre-release build:
        $ make SGX_MODE=SIM SGX_PRERELEASE=1 SGX_DEBUG=0
    f. Simulation Mode, Release build:
        $ make SGX_MODE=SIM SGX_DEBUG=0
4. Execute the binary directly:
    $ ./app
5. Remember to "make clean" before switching build mode



For adding OPENSSL lib to enclave use the following example:
https://github.com/intel/intel-sgx-ssl

https://github.com/tyler-lee/async_on_sync

For debuggin enclave, use non-debug version of openssl and in case of segmentation fault, Forward it and Continue
