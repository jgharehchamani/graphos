# GraphOS
This repository contains doubly oblivious data structures. The open-source is based on our following paper:

Ghareh Chamani, Javad , Ioannis Demertzis, Dimitrios Papadopoulos, Charalampos Papamanthou, and Rasool Jalili. "GraphOS: Towards Oblivious Graph Processing." accepted in VLDB 2023


## Pre-requisites: ###
Our schemes were tested with the following configuration
- 64-bit Ubuntu 18.04
- g++ = 7.5
- SGX SDK = 2.4
- SGX Driver = 2.6

Install IntelSGX Driver from https://github.com/intel/linux-sgx-driver/archive/refs/tags/sgx_driver_2.6.zip

Install SGX SDK 2.4: https://github.com/intel/linux-sgx/archive/refs/tags/sgx_2.4.zip

Install the above binary file in /opt/intel

Execute the following command to allow the app to find libsample_crypto.so:

sudo cp sample_libcrypto/libsample_libcrypto.so /usr/lib

## SGX2 version of the code ###
There are few changes in the SGX2 version of the code in comparison to SGX1, includeing:

- adding -lsgx_pthread link flag in Makefile
- importing "sgx_pthread.edl" in OMAP.edl and Enclave.edl
- replacing the remote attestation code of Enclave.cpp with the new version of SGXSDK

SGX2 version of Omix++ is available in [Omix++-SGX2 branch](https://github.com/jgharehchamani/graphos/tree/Omix%2B%2B-SGX2)

## Getting Started ###
Oblix++, Omix++, GraphOS, and GraphOS parallel version are provided in four separate subfolders. Use the following instruction to build them:


### Compiling and Running


Compiling:
Make
Running:
./app

For a sample test case, create a file (e.g., V13E-256.in) in the datasets folder and describe the graph in the following format:

source  destination  (1 for vertex and 0 for edge)

for example, a full graph with 4 nodes will be like this:

1 1 1\
2 2 1\
3 3 1\
4 4 1\
1 2 0\
1 3 0\
1 4 0\
2 3 0\
2 4 0\
3 4 0

Be careful not add an extra newline at the end of the file



### Contact ###
Feel free to reach out to me to get help in setting up our code or any other queries you may have related to our paper: jgc@cse.ust.hk
