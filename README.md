# Omix++
This repository contains OMIX++ ported to SGX2. The open-source is based on our following paper:

Ghareh Chamani, Javad , Ioannis Demertzis, Dimitrios Papadopoulos, Charalampos Papamanthou, and Rasool Jalili. "GraphOS: Towards Oblivious Graph Processing." accepted in VLDB 2023


## Pre-requisites: ###
Our schemes were tested with the following configuration
- 64-bit Ubuntu 22.04
- g++ = 11.4.0
- SGX SDK = 2.22 (commit:8a223177093da64a5d071b36127d12b04c0d3397)
- SGX Driver = 2.14 (commit:54c9c4c1fe30f459abe7c4b9c153ed2967973c22)
- Make = 4.3
- SGX-SSL = 3.0_Rev2 (commit:b483cba71334d79933f40cca2dbcf06514bd96ba)

Install IntelSGX Driver from https://github.com/intel/linux-sgx-driver/

Install SGX SDK: https://github.com/intel/linux-sgx/

Install the above binary file in /opt/intel

Install SGX SSL with OpenSSL 3.0.12: https://github.com/intel/intel-sgx-ssl


Replace include and lib64 folder of SGXSSL in Omix++/sgxssl folder

### Compiling and Running


Compiling:
Make
Running:
./app



### Contact ###
Feel free to reach out to me to get help in setting up our code or any other queries you may have related to our paper: jgc@cse.ust.hk
