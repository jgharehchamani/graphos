#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>
#include <pwd.h>
#include <array>
#include <iostream>
#include <cstring>
#include <fstream>
#include <stdexcept>

using namespace std;
#define MAX_PATH FILENAME_MAX

#include "sgx_urts.h"
#include "sgx_uae_service.h"
#include "App.h"
#include "Enclave_u.h"
#include "OMAP/RAMStoreEnclaveInterface.h"
#include "../Common/Common.h"
#include "OMAP/GraphNode.h"
#include "OMAP/Node.h"
/*
 * Copyright (C) 2011-2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

// This sample is confined to the communication between a SGX client platform
// and an ISV Application Server. 


#include <chrono>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
// Needed for definition of remote attestation messages.
#include "remote_attestation_result.h"

#include "Enclave_u.h"

// Needed to call untrusted key exchange library APIs, i.e. sgx_ra_proc_msg2.
#include "sgx_ukey_exchange.h"

// Needed to get service provider's information, in your real project, you will
// need to talk to real server.
#include "network_ra.h"

// Needed to create enclave and do ecall.
#include "sgx_urts.h"

// Needed to query extended epid group id.
#include "sgx_uae_service.h"

#include "../service_provider/service_provider.h"

#ifndef SAFE_FREE
#define SAFE_FREE(ptr) {if (NULL != (ptr)) {free(ptr); (ptr) = NULL;}}
#endif

// In addition to generating and sending messages, this application
// can use pre-generated messages to verify the generation of
// messages and the information flow.
#include "sample_messages.h"


#define ENCLAVE_PATH "isv_enclave.signed.so"

uint8_t* msg1_samples[] = {msg1_sample1, msg1_sample2};
uint8_t* msg2_samples[] = {msg2_sample1, msg2_sample2};
uint8_t* msg3_samples[] = {msg3_sample1, msg3_sample2};
uint8_t* attestation_msg_samples[] = {attestation_msg_sample1, attestation_msg_sample2};

// Some utility functions to output some of the data structures passed between
// the ISV app and the remote attestation service provider.

void PRINT_BYTE_ARRAY(
        FILE *file, void *mem, uint32_t len) {
    if (!mem || !len) {
        fprintf(file, "\n( null )\n");
        return;
    }
    uint8_t *array = (uint8_t *) mem;
    fprintf(file, "%u bytes:\n{\n", len);
    uint32_t i = 0;
    for (i = 0; i < len - 1; i++) {
        fprintf(file, "0x%x, ", array[i]);
        if (i % 8 == 7) fprintf(file, "\n");
    }
    fprintf(file, "0x%x ", array[i]);
    fprintf(file, "\n}\n");
}

void PRINT_ATTESTATION_SERVICE_RESPONSE(
        FILE *file,
        ra_samp_response_header_t *response) {
    if (!response) {
        fprintf(file, "\t\n( null )\n");
        return;
    }

    fprintf(file, "RESPONSE TYPE:   0x%x\n", response->type);
    fprintf(file, "RESPONSE STATUS: 0x%x 0x%x\n", response->status[0],
            response->status[1]);
    fprintf(file, "RESPONSE BODY SIZE: %u\n", response->size);

    if (response->type == TYPE_RA_MSG2) {
        sgx_ra_msg2_t* p_msg2_body = (sgx_ra_msg2_t*) (response->body);

        fprintf(file, "MSG2 gb - ");
        PRINT_BYTE_ARRAY(file, &(p_msg2_body->g_b), sizeof (p_msg2_body->g_b));

        fprintf(file, "MSG2 spid - ");
        PRINT_BYTE_ARRAY(file, &(p_msg2_body->spid), sizeof (p_msg2_body->spid));

        fprintf(file, "MSG2 quote_type : %hx\n", p_msg2_body->quote_type);

        fprintf(file, "MSG2 kdf_id : %hx\n", p_msg2_body->kdf_id);

        fprintf(file, "MSG2 sign_gb_ga - ");
        PRINT_BYTE_ARRAY(file, &(p_msg2_body->sign_gb_ga),
                sizeof (p_msg2_body->sign_gb_ga));

        fprintf(file, "MSG2 mac - ");
        PRINT_BYTE_ARRAY(file, &(p_msg2_body->mac), sizeof (p_msg2_body->mac));

        fprintf(file, "MSG2 sig_rl - ");
        PRINT_BYTE_ARRAY(file, &(p_msg2_body->sig_rl),
                p_msg2_body->sig_rl_size);
    } else if (response->type == TYPE_RA_ATT_RESULT) {
        sample_ra_att_result_msg_t *p_att_result =
                (sample_ra_att_result_msg_t *) (response->body);
        fprintf(file, "ATTESTATION RESULT MSG platform_info_blob - ");
        PRINT_BYTE_ARRAY(file, &(p_att_result->platform_info_blob),
                sizeof (p_att_result->platform_info_blob));

        fprintf(file, "ATTESTATION RESULT MSG mac - ");
        PRINT_BYTE_ARRAY(file, &(p_att_result->mac), sizeof (p_att_result->mac));

        fprintf(file, "ATTESTATION RESULT MSG secret.payload_tag - %u bytes\n",
                p_att_result->secret.payload_size);

        fprintf(file, "ATTESTATION RESULT MSG secret.payload - ");
        PRINT_BYTE_ARRAY(file, p_att_result->secret.payload,
                p_att_result->secret.payload_size);
    } else {
        fprintf(file, "\nERROR in printing out the response. "
                "Response of type not supported %d\n", response->type);
    }
}

/* Global EID shared by multiple threads */
sgx_enclave_id_t global_eid = 0;

typedef struct _sgx_errlist_t {
    sgx_status_t err;
    const char *msg;
    const char *sug; /* Suggestion */
} sgx_errlist_t;

/* Error code returned by sgx_create_enclave */
static sgx_errlist_t sgx_errlist[] = {
    {
        SGX_ERROR_UNEXPECTED,
        "Unexpected error occurred.",
        NULL
    },
    {
        SGX_ERROR_INVALID_PARAMETER,
        "Invalid parameter.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_MEMORY,
        "Out of memory.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_LOST,
        "Power transition occurred.",
        "Please refer to the sample \"PowerTransition\" for details."
    },
    {
        SGX_ERROR_INVALID_ENCLAVE,
        "Invalid enclave image.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ENCLAVE_ID,
        "Invalid enclave identification.",
        NULL
    },
    {
        SGX_ERROR_INVALID_SIGNATURE,
        "Invalid enclave signature.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_EPC,
        "Out of EPC memory.",
        NULL
    },
    {
        SGX_ERROR_NO_DEVICE,
        "Invalid SGX device.",
        "Please make sure SGX module is enabled in the BIOS, and install SGX driver afterwards."
    },
    {
        SGX_ERROR_MEMORY_MAP_CONFLICT,
        "Memory map conflicted.",
        NULL
    },
    {
        SGX_ERROR_INVALID_METADATA,
        "Invalid enclave metadata.",
        NULL
    },
    {
        SGX_ERROR_DEVICE_BUSY,
        "SGX device was busy.",
        NULL
    },
    {
        SGX_ERROR_INVALID_VERSION,
        "Enclave version was invalid.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ATTRIBUTE,
        "Enclave was not authorized.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_FILE_ACCESS,
        "Can't open enclave file.",
        NULL
    },
    {
        SGX_ERROR_NDEBUG_ENCLAVE,
        "The enclave is signed as product enclave, and can not be created as debuggable enclave.",
        NULL
    },
};

/* Check error conditions for loading enclave */
void print_error_message(sgx_status_t ret) {
    size_t idx = 0;
    size_t ttl = sizeof sgx_errlist / sizeof sgx_errlist[0];

    for (idx = 0; idx < ttl; idx++) {
        if (ret == sgx_errlist[idx].err) {
            if (NULL != sgx_errlist[idx].sug)
                printf("Info: %s\n", sgx_errlist[idx].sug);
            printf("Error: %s\n", sgx_errlist[idx].msg);
            break;
        }
    }

    if (idx == ttl)
        printf("Error: Unexpected error occurred.\n");
}

/* Initialize the enclave:
 *   Call sgx_create_enclave to initialize an enclave instance
 */
int initialize_enclave(void) {
    sgx_launch_token_t token = {0};
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    int updated = 0;

    /* Call sgx_create_enclave to initialize an enclave instance */
    /* Debug Support: set 2nd parameter to 1 */
    ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, &token, &updated, &global_eid, NULL);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        return -1;
    }

    return 0;
}

std::string exec(const char* cmd) {
    char buffer[128];
    std::string result = "";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    try {
        while (fgets(buffer, sizeof buffer, pipe) != NULL) {
            result += buffer;
        }
    } catch (...) {
        pclose(pipe);
        throw;
    }
    pclose(pipe);
    return result;
}

void initializeEdgeList(bytes<Key> secretkey, vector<GraphNode>* edgeList, char** edges) {
    unsigned long long blockSize = sizeof (GraphNode);
    unsigned long long clen_size = AES::GetCiphertextLength((int) (blockSize));
    unsigned long long plaintext_size = (blockSize);

    for (int i = 0; i < edgeList->size(); i++) {
        std::array<byte_t, sizeof (GraphNode) > data;

        const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*edgeList)[i]));
        const byte_t* end = begin + sizeof (GraphNode);
        std::copy(begin, end, std::begin(data));

        block buffer(data.begin(), data.end());
        block ciphertext = AES::Encrypt(secretkey, buffer, clen_size, plaintext_size);
        memcpy((uint8_t*) (*edges) + i * ciphertext.size(), ciphertext.data(), ciphertext.size());
    }
}

void initializeCiphertexts(bytes<Key> secretkey, map<Bid, string>* pairs, vector<block>* ciphertexts) {
    vector<Node*> nodes;
    for (auto pair : (*pairs)) {
        Node* node = new Node();
        node->key = pair.first;
        node->index = 0;
        std::fill(node->value.begin(), node->value.end(), 0);
        std::copy(pair.second.begin(), pair.second.end(), node->value.begin());
        node->leftID = 0;
        node->leftPos = -1;
        node->rightPos = -1;
        node->rightID = 0;
        node->pos = 0;
        node->isDummy = false;
        node->height = 1; // new node is initially added at leaf
        nodes.push_back(node);
    }

    unsigned long long blockSize = sizeof (Node);
    unsigned long long clen_size = AES::GetCiphertextLength((int) (blockSize));
    unsigned long long plaintext_size = (blockSize);

    for (int i = 0; i < nodes.size(); i++) {
        std::array<byte_t, sizeof (Node) > data;

        const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*nodes[i])));
        const byte_t* end = begin + sizeof (Node);
        std::copy(begin, end, std::begin(data));

        block buffer(data.begin(), data.end());
        block ciphertext = AES::Encrypt(secretkey, buffer, clen_size, plaintext_size);
        (*ciphertexts).push_back(ciphertext);
    }
}








/* Application entry */
#define _T(x) x

int SGX_CDECL main(int argc, char *argv[]) {
    (void) (argc);
    (void) (argv);

    bool check_remote_attestation = false;
    double t;

    if (check_remote_attestation) {


        int ret = 0;
        ra_samp_request_header_t *p_msg0_full = NULL;
        ra_samp_response_header_t *p_msg0_resp_full = NULL;
        ra_samp_request_header_t *p_msg1_full = NULL;
        ra_samp_response_header_t *p_msg2_full = NULL;
        sgx_ra_msg3_t *p_msg3 = NULL;
        ra_samp_response_header_t* p_att_result_msg_full = NULL;
        //    sgx_enclave_id_t enclave_id = 0;
        int enclave_lost_retry_time = 1;
        int busy_retry_time = 4;
        sgx_ra_context_t context = INT_MAX;
        sgx_status_t status = SGX_SUCCESS;
        ra_samp_request_header_t* p_msg3_full = NULL;

        int32_t verify_index = -1;
        int32_t verification_samples = sizeof (msg1_samples) / sizeof (msg1_samples[0]);

        FILE* OUTPUT = stdout;

#define VERIFICATION_INDEX_IS_VALID() (verify_index > 0 && \
                                       verify_index <= verification_samples)
#define GET_VERIFICATION_ARRAY_INDEX() (verify_index-1)



        // Remote attestation will be initiated the ISV server challenges the ISV
        // app or if the ISV app detects it doesn't have the credentials
        // (shared secret) from a previous attestation required for secure
        // communication with the server.
        {
            // ISV application creates the ISV enclave.
            do {
                //            sgx_launch_token_t token = {0};
                //            sgx_status_t ret = SGX_ERROR_UNEXPECTED;
                //            int updated = 0;
                //
                //            /* Call sgx_create_enclave to initialize an enclave instance */
                //            /* Debug Support: set 2nd parameter to 1 */
                //            ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, &token, &updated, &global_eid, NULL);
                //            if (ret != SGX_SUCCESS) {
                //                print_error_message(ret);
                //                return -1;
                //            }


                //
                //
                //
                //
                //
                //
                //
                ret = sgx_create_enclave(ENCLAVE_FILENAME,
                        SGX_DEBUG_FLAG,
                        NULL,
                        NULL,
                        &global_eid, NULL);
                if (SGX_SUCCESS != ret) {
                    ret = -1;
                    fprintf(OUTPUT, "\nError, call sgx_create_enclave fail [%s].",
                            __FUNCTION__);
                    goto CLEANUP;
                }
                fprintf(OUTPUT, "\nCall sgx_create_enclave success.");

                Utilities::startTimer(99);

                // Preparation for remote attestation by configuring extended epid group id.
                {
                    uint32_t extended_epid_group_id = 0;
                    ret = sgx_get_extended_epid_group_id(&extended_epid_group_id);
                    if (SGX_SUCCESS != ret) {
                        ret = -1;
                        fprintf(OUTPUT, "\nError, call sgx_get_extended_epid_group_id fail [%s].",
                                __FUNCTION__);
                        return ret;
                    }
                    fprintf(OUTPUT, "\nCall sgx_get_extended_epid_group_id success.");

                    p_msg0_full = (ra_samp_request_header_t*)
                            malloc(sizeof (ra_samp_request_header_t)
                            + sizeof (uint32_t));
                    if (NULL == p_msg0_full) {
                        ret = -1;
                        goto CLEANUP;
                    }
                    p_msg0_full->type = TYPE_RA_MSG0;
                    p_msg0_full->size = sizeof (uint32_t);

                    *(uint32_t*) ((uint8_t*) p_msg0_full + sizeof (ra_samp_request_header_t)) = extended_epid_group_id;
                    {

                        fprintf(OUTPUT, "\nMSG0 body generated -\n");

                        PRINT_BYTE_ARRAY(OUTPUT, p_msg0_full->body, p_msg0_full->size);

                    }

                    //-----------------------------------------------------------------------------------
                    // The ISV application sends msg0 to the SP.
                    // The ISV decides whether to support this extended epid group id.
                    fprintf(OUTPUT, "\nSending msg0 to remote attestation service provider.\n");

                    ret = ra_network_send_receive("http://SampleServiceProvider.intel.com/",
                            p_msg0_full,
                            &p_msg0_resp_full);
                    if (ret != 0) {
                        fprintf(OUTPUT, "\nError, ra_network_send_receive for msg0 failed "
                                "[%s].", __FUNCTION__);
                        goto CLEANUP;
                    }
                    fprintf(OUTPUT, "\nSent MSG0 to remote attestation service.\n");
                    //-----------------------------------------------------------------------------------
                }
                ret = enclave_init_ra(global_eid,
                        &status,
                        false,
                        &context);
                //Ideally, this check would be around the full attestation flow.
            } while (SGX_ERROR_ENCLAVE_LOST == ret && enclave_lost_retry_time--);

            if (SGX_SUCCESS != ret || status) {
                ret = -1;
                fprintf(OUTPUT, "\nError, call enclave_init_ra fail [%s].",
                        __FUNCTION__);
                goto CLEANUP;
            }
            fprintf(OUTPUT, "\nCall enclave_init_ra success.");

            // isv application call uke sgx_ra_get_msg1
            p_msg1_full = (ra_samp_request_header_t*)
                    malloc(sizeof (ra_samp_request_header_t)
                    + sizeof (sgx_ra_msg1_t));
            if (NULL == p_msg1_full) {
                ret = -1;
                goto CLEANUP;
            }
            p_msg1_full->type = TYPE_RA_MSG1;
            p_msg1_full->size = sizeof (sgx_ra_msg1_t);
            do {
                ret = sgx_ra_get_msg1(context, global_eid, sgx_ra_get_ga,
                        (sgx_ra_msg1_t*) ((uint8_t*) p_msg1_full
                        + sizeof (ra_samp_request_header_t)));
                if (SGX_ERROR_BUSY == ret) {
                    sleep(3); // Wait 3s between retries
                }
            } while (SGX_ERROR_BUSY == ret && busy_retry_time--);
            if (SGX_SUCCESS != ret) {
                ret = -1;
                fprintf(OUTPUT, "\nError, call sgx_ra_get_msg1 fail [%s].",
                        __FUNCTION__);
                goto CLEANUP;
            } else {
                fprintf(OUTPUT, "\nCall sgx_ra_get_msg1 success.\n");

                fprintf(OUTPUT, "\nMSG1 body generated -\n");

                PRINT_BYTE_ARRAY(OUTPUT, p_msg1_full->body, p_msg1_full->size);

            }

            if (VERIFICATION_INDEX_IS_VALID()) {

                memcpy_s(p_msg1_full->body, p_msg1_full->size,
                        msg1_samples[GET_VERIFICATION_ARRAY_INDEX()],
                        p_msg1_full->size);

                fprintf(OUTPUT, "\nInstead of using the recently generated MSG1, "
                        "we will use the following precomputed MSG1 -\n");

                PRINT_BYTE_ARRAY(OUTPUT, p_msg1_full->body, p_msg1_full->size);
            }


            // The ISV application sends msg1 to the SP to get msg2,
            // msg2 needs to be freed when no longer needed.
            // The ISV decides whether to use linkable or unlinkable signatures.
            fprintf(OUTPUT, "\nSending msg1 to remote attestation service provider."
                    "Expecting msg2 back.\n");

            //-------------------------------------------------------------------
            ret = ra_network_send_receive("http://SampleServiceProvider.intel.com/",
                    p_msg1_full,
                    &p_msg2_full);

            if (ret != 0 || !p_msg2_full) {
                fprintf(OUTPUT, "\nError, ra_network_send_receive for msg1 failed "
                        "[%s].", __FUNCTION__);
                if (VERIFICATION_INDEX_IS_VALID()) {
                    fprintf(OUTPUT, "\nBecause we are in verification mode we will "
                            "ignore this error.\n");
                    fprintf(OUTPUT, "\nInstead, we will pretend we received the "
                            "following MSG2 - \n");

                    SAFE_FREE(p_msg2_full);
                    ra_samp_response_header_t* precomputed_msg2 =
                            (ra_samp_response_header_t*) msg2_samples[
                            GET_VERIFICATION_ARRAY_INDEX()];
                    const size_t msg2_full_size = sizeof (ra_samp_response_header_t)
                            + precomputed_msg2->size;
                    p_msg2_full =
                            (ra_samp_response_header_t*) malloc(msg2_full_size);
                    if (NULL == p_msg2_full) {
                        ret = -1;
                        goto CLEANUP;
                    }
                    memcpy_s(p_msg2_full, msg2_full_size, precomputed_msg2,
                            msg2_full_size);

                    PRINT_BYTE_ARRAY(OUTPUT, p_msg2_full,
                            (uint32_t)sizeof (ra_samp_response_header_t)
                            + p_msg2_full->size);
                } else {
                    goto CLEANUP;
                }
            } else {
                // Successfully sent msg1 and received a msg2 back.
                // Time now to check msg2.
                if (TYPE_RA_MSG2 != p_msg2_full->type) {

                    fprintf(OUTPUT, "\nError, didn't get MSG2 in response to MSG1. "
                            "[%s].", __FUNCTION__);

                    if (VERIFICATION_INDEX_IS_VALID()) {
                        fprintf(OUTPUT, "\nBecause we are in verification mode we "
                                "will ignore this error.");
                    } else {
                        goto CLEANUP;
                    }
                }

                fprintf(OUTPUT, "\nSent MSG1 to remote attestation service "
                        "provider. Received the following MSG2:\n");
                PRINT_BYTE_ARRAY(OUTPUT, p_msg2_full,
                        (uint32_t)sizeof (ra_samp_response_header_t)
                        + p_msg2_full->size);

                fprintf(OUTPUT, "\nA more descriptive representation of MSG2:\n");
                PRINT_ATTESTATION_SERVICE_RESPONSE(OUTPUT, p_msg2_full);

                if (VERIFICATION_INDEX_IS_VALID()) {
                    // The response should match the precomputed MSG2:
                    ra_samp_response_header_t* precomputed_msg2 =
                            (ra_samp_response_header_t *)
                            msg2_samples[GET_VERIFICATION_ARRAY_INDEX()];
                    if (MSG2_BODY_SIZE !=
                            sizeof (ra_samp_response_header_t) + p_msg2_full->size ||
                            memcmp(precomputed_msg2, p_msg2_full,
                            sizeof (ra_samp_response_header_t) + p_msg2_full->size)) {
                        fprintf(OUTPUT, "\nVerification ERROR. Our precomputed "
                                "value for MSG2 does NOT match.\n");
                        fprintf(OUTPUT, "\nPrecomputed value for MSG2:\n");
                        PRINT_BYTE_ARRAY(OUTPUT, precomputed_msg2,
                                (uint32_t)sizeof (ra_samp_response_header_t)
                                + precomputed_msg2->size);
                        fprintf(OUTPUT, "\nA more descriptive representation "
                                "of precomputed value for MSG2:\n");
                        PRINT_ATTESTATION_SERVICE_RESPONSE(OUTPUT,
                                precomputed_msg2);
                    } else {
                        fprintf(OUTPUT, "\nVerification COMPLETE. Remote "
                                "attestation service provider generated a "
                                "matching MSG2.\n");
                    }
                }

            }
            //--------------------------------------------------------------------------------

            sgx_ra_msg2_t* p_msg2_body = (sgx_ra_msg2_t*) ((uint8_t*) p_msg2_full
                    + sizeof (ra_samp_response_header_t));


            uint32_t msg3_size = 0;
            //-------------------------------------------------------------------------
            if (VERIFICATION_INDEX_IS_VALID()) {
                // We cannot generate a valid MSG3 using the precomputed messages
                // we have been using. We will use the precomputed msg3 instead.
                msg3_size = MSG3_BODY_SIZE;
                p_msg3 = (sgx_ra_msg3_t*) malloc(msg3_size);
                if (NULL == p_msg3) {
                    ret = -1;
                    goto CLEANUP;
                }
                memcpy_s(p_msg3, msg3_size,
                        msg3_samples[GET_VERIFICATION_ARRAY_INDEX()], msg3_size);
                fprintf(OUTPUT, "\nBecause MSG1 was a precomputed value, the MSG3 "
                        "we use will also be. PRECOMPUTED MSG3 - \n");
            } else {
                //------------------------------------------------------------------------
                busy_retry_time = 2;
                // The ISV app now calls uKE sgx_ra_proc_msg2,
                // The ISV app is responsible for freeing the returned p_msg3!!
                do {
                    ret = sgx_ra_proc_msg2(context,
                            global_eid,
                            sgx_ra_proc_msg2_trusted,
                            sgx_ra_get_msg3_trusted,
                            p_msg2_body,
                            p_msg2_full->size,
                            &p_msg3,
                            &msg3_size);
                } while (SGX_ERROR_BUSY == ret && busy_retry_time--);
                if (!p_msg3) {
                    fprintf(OUTPUT, "\nError, call sgx_ra_proc_msg2 fail. "
                            "p_msg3 = 0x%p [%s].", p_msg3, __FUNCTION__);
                    ret = -1;
                    goto CLEANUP;
                }
                if (SGX_SUCCESS != (sgx_status_t) ret) {
                    fprintf(OUTPUT, "\nError, call sgx_ra_proc_msg2 fail. "
                            "ret = 0x%08x [%s].", ret, __FUNCTION__);
                    ret = -1;
                    goto CLEANUP;
                } else {
                    fprintf(OUTPUT, "\nCall sgx_ra_proc_msg2 success.\n");
                    fprintf(OUTPUT, "\nMSG3 - \n");
                }
            }


            PRINT_BYTE_ARRAY(OUTPUT, p_msg3, msg3_size);

            p_msg3_full = (ra_samp_request_header_t*) malloc(
                    sizeof (ra_samp_request_header_t) + msg3_size);
            if (NULL == p_msg3_full) {
                ret = -1;
                goto CLEANUP;
            }
            p_msg3_full->type = TYPE_RA_MSG3;
            p_msg3_full->size = msg3_size;
            if (memcpy_s(p_msg3_full->body, msg3_size, p_msg3, msg3_size)) {
                fprintf(OUTPUT, "\nError: INTERNAL ERROR - memcpy failed in [%s].",
                        __FUNCTION__);
                ret = -1;
                goto CLEANUP;
            }

            //---------------------------------------------------------------------

            // The ISV application sends msg3 to the SP to get the attestation
            // result message, attestation result message needs to be freed when
            // no longer needed. The ISV service provider decides whether to use
            // linkable or unlinkable signatures. The format of the attestation
            // result is up to the service provider. This format is used for
            // demonstration.  Note that the attestation result message makes use
            // of both the MK for the MAC and the SK for the secret. These keys are
            // established from the SIGMA secure channel binding.
            ret = ra_network_send_receive("http://SampleServiceProvider.intel.com/",
                    p_msg3_full,
                    &p_att_result_msg_full);
            if (ret || !p_att_result_msg_full) {
                ret = -1;
                fprintf(OUTPUT, "\nError, sending msg3 failed [%s].", __FUNCTION__);
                goto CLEANUP;
            }

            //-------------------------------------------------------
            sample_ra_att_result_msg_t * p_att_result_msg_body =
                    (sample_ra_att_result_msg_t *) ((uint8_t*) p_att_result_msg_full
                    + sizeof (ra_samp_response_header_t));

            //-------------------------------------------------------
            if (TYPE_RA_ATT_RESULT != p_att_result_msg_full->type) {
                ret = -1;
                fprintf(OUTPUT, "\nError. Sent MSG3 successfully, but the message "
                        "received was NOT of type att_msg_result. Type = "
                        "%d. [%s].", p_att_result_msg_full->type,
                        __FUNCTION__);
                goto CLEANUP;
            } else {
                fprintf(OUTPUT, "\nSent MSG3 successfully. Received an attestation "
                        "result message back\n.");
                if (VERIFICATION_INDEX_IS_VALID()) {
                    if (ATTESTATION_MSG_BODY_SIZE != p_att_result_msg_full->size ||
                            memcmp(p_att_result_msg_full->body,
                            attestation_msg_samples[GET_VERIFICATION_ARRAY_INDEX()],
                            p_att_result_msg_full->size)) {
                        fprintf(OUTPUT, "\nSent MSG3 successfully. Received an "
                                "attestation result message back that did "
                                "NOT match the expected value.\n");
                        fprintf(OUTPUT, "\nEXPECTED ATTESTATION RESULT -");
                        PRINT_BYTE_ARRAY(OUTPUT,
                                attestation_msg_samples[GET_VERIFICATION_ARRAY_INDEX()],
                                ATTESTATION_MSG_BODY_SIZE);
                    }
                }
            }

            fprintf(OUTPUT, "\nATTESTATION RESULT RECEIVED - ");
            PRINT_BYTE_ARRAY(OUTPUT, p_att_result_msg_full->body,
                    p_att_result_msg_full->size);


            if (VERIFICATION_INDEX_IS_VALID()) {
                fprintf(OUTPUT, "\nBecause we used precomputed values for the "
                        "messages, the attestation result message will "
                        "not pass further verification tests, so we will "
                        "skip them.\n");
                goto CLEANUP;
            }

            //--------------------------------------------------------------------
            // Check the MAC using MK on the attestation result message.
            // The format of the attestation result message is ISV specific.
            // This is a simple form for demonstration. In a real product,
            // the ISV may want to communicate more information.
            ret = verify_att_result_mac(global_eid,
                    &status,
                    context,
                    (uint8_t*) & p_att_result_msg_body->platform_info_blob,
                    sizeof (ias_platform_info_blob_t),
                    (uint8_t*) & p_att_result_msg_body->mac,
                    sizeof (sgx_mac_t));
            if ((SGX_SUCCESS != ret) ||
                    (SGX_SUCCESS != status)) {
                ret = -1;
                fprintf(OUTPUT, "\nError: INTEGRITY FAILED - attestation result "
                        "message MK based cmac failed in [%s].",
                        __FUNCTION__);
                goto CLEANUP;
            }

            bool attestation_passed = true;
            // Check the attestation result for pass or fail.
            // Whether attestation passes or fails is a decision made by the ISV Server.
            // When the ISV server decides to trust the enclave, then it will return success.
            // When the ISV server decided to not trust the enclave, then it will return failure.
            if (0 != p_att_result_msg_full->status[0]
                    || 0 != p_att_result_msg_full->status[1]) {
                fprintf(OUTPUT, "\nError, attestation result message MK based cmac "
                        "failed in [%s].", __FUNCTION__);
                attestation_passed = false;
            }

            // The attestation result message should contain a field for the Platform
            // Info Blob (PIB).  The PIB is returned by attestation server in the attestation report.
            // It is not returned in all cases, but when it is, the ISV app
            // should pass it to the blob analysis API called sgx_report_attestation_status()
            // along with the trust decision from the ISV server.
            // The ISV application will take action based on the update_info.
            // returned in update_info by the API.  
            // This call is stubbed out for the sample.
            // 
            // sgx_update_info_bit_t update_info;
            // ret = sgx_report_attestation_status(
            //     &p_att_result_msg_body->platform_info_blob,
            //     attestation_passed ? 0 : 1, &update_info);

            // Get the shared secret sent by the server using SK (if attestation
            // passed)
            if (attestation_passed) {
                ret = put_secret_data(global_eid,
                        &status,
                        context,
                        p_att_result_msg_body->secret.payload,
                        p_att_result_msg_body->secret.payload_size,
                        p_att_result_msg_body->secret.payload_tag);
                if ((SGX_SUCCESS != ret) || (SGX_SUCCESS != status)) {
                    fprintf(OUTPUT, "\nError, attestation result message secret "
                            "using SK based AESGCM failed in [%s]. ret = "
                            "0x%0x. status = 0x%0x", __FUNCTION__, ret,
                            status);
                    goto CLEANUP;
                }
            }
            fprintf(OUTPUT, "\nSecret successfully received from server.");
            fprintf(OUTPUT, "\nRemote attestation success!");
        }

CLEANUP:
        // Clean-up
        // Need to close the RA key state.
        if (INT_MAX != context) {
            int ret_save = ret;
            ret = enclave_ra_close(global_eid, &status, context);
            if (SGX_SUCCESS != ret || status) {
                ret = -1;
                fprintf(OUTPUT, "\nError, call enclave_ra_close fail [%s].",
                        __FUNCTION__);
            } else {
                // enclave_ra_close was successful, let's restore the value that
                // led us to this point in the code.
                ret = ret_save;
            }
            fprintf(OUTPUT, "\nCall enclave_ra_close success.");
        }

        ra_free_network_response_buffer(p_msg0_resp_full);
        ra_free_network_response_buffer(p_msg2_full);
        ra_free_network_response_buffer(p_att_result_msg_full);

        // p_msg3 is malloc'd by the untrusted KE library. App needs to free.
        SAFE_FREE(p_msg3);
        SAFE_FREE(p_msg3_full);
        SAFE_FREE(p_msg1_full);
        SAFE_FREE(p_msg0_full);
        //------------------------------------------------------------------------------------------
        //-------------------END OF REMOTE ATTESTATION-----------------------------------------------------------
        //------------------------------------------------------------------------------------------
        //------------------------------------------------------------------------------------------
        //-https://software.intel.com/content/www/us/en/develop/articles/code-sample-intel-software-guard-extensions-remote-attestation-end-to-end-example.html
        //------------------------------------------------------------------------------------------
        //------------------------------------------------------------------------------------------    
        t = Utilities::stopTimer(99);
        printf("\nRemote Attestation Time is:%f\n", t);
    } else {
        /* Initialize the enclave */
        if (initialize_enclave() < 0) {
            printf("Enter a character before exit ...\n");
            getchar();
            return -1;
        }
    }



    /* My Codes */
    int size = 0;
    string filename = "";
    string alg = "";
    if (argc > 1) {
        filename = string(argv[1]);
        alg = string(argv[2]);
    } else {
        filename = "datasets/V13E-256.in";
        alg = "OBLIVIOUS-BFS";
    }
    size = stoi(exec(string("wc -l " + filename + " | cut -d ' ' -f 1").c_str()));

    std::ifstream infile((filename).c_str());


    bytes<Key> secretkey{0};
    map<Bid, string> pairs;
    vector<block> ciphertexts;
    vector<GraphNode> edgeList;

    int node_numebr = 0;
    int testEdgesrc, testEdgeDst;

    for (int i = 0; i < size; i++) {
        int src, dst, weight;
        infile >> src >> dst >> weight;
        GraphNode node;
        node.src_id = src;
        node.dst_id = dst;
        node.weight = weight;
        if (src == dst) {
            node_numebr++;
        } else {
            if (node.weight == 0) {
                node.weight = 1;
            }
            edgeList.push_back(node);
        }
    }

    int encryptionSize = IV + AES::GetCiphertextLength(sizeof (GraphNode)); //SGX_AESGCM_IV_SIZE + SGX_AESGCM_MAC_SIZE = 28     sizeof(GraphNode)=64
    int maxPad = (int) pow(2, ceil(log2(edgeList.size())));
    char* edges = new char[maxPad * encryptionSize];
    long long maxSize = node_numebr;
    int depth = (int) (ceil(log2(maxSize)) - 1) + 1;
    int maxOfRandom = (long long) (pow(2, depth));
    unsigned long long bucketCount = maxOfRandom * 2 - 1;
    unsigned long long blockSize = sizeof (Node); // B  
    size_t blockCount = (size_t) (Z * bucketCount);
    unsigned long long storeBlockSize = (size_t) (IV + AES::GetCiphertextLength((int) (Z * (blockSize))));

    initializeEdgeList(secretkey, &edgeList, &edges);
    int edgeNumner = edgeList.size();
    edgeList.clear();

    for (int i = 1; i <= node_numebr; i++) {
        string omapKey = "?" + to_string(i);
        std::array< uint8_t, ID_SIZE > keyArray;
        keyArray.fill(0);
        std::copy(omapKey.begin(), omapKey.end(), std::begin(keyArray));
        std::array<byte_t, ID_SIZE> id;
        std::memcpy(id.data(), (const char*) keyArray.data(), ID_SIZE);
        Bid inputBid(id);
        pairs[inputBid] = "0-0";
    }

    initializeCiphertexts(secretkey, &pairs, &ciphertexts);

    setupMode = true;

    ocall_setup_ramStore(blockCount, storeBlockSize);
    ocall_nwrite_raw_ramStore(&ciphertexts);
    Utilities::startTimer(1);

    int op = -1;
    if (alg == "OBLIVIOUS-SSSP-OBLIVM") {
        op = 3;
    } else if (alg == "OBLIVIOUS-MST") {
        op = 2;
    } else if (alg == "OBLIVIOUS-BFS") {
        op = 1;
    }
    ecall_setup_with_small_memory(global_eid, edgeNumner, node_numebr, (const char*) secretkey.data(), &edges, op);

   

    auto timer = Utilities::stopTimer(1);
    cout << "Setup Time:" << timer + t << " Microseconds" << endl;

    Utilities::startTimer(5);
    if (alg == "SEARCH-VERTEX" || alg == "search-vertex") {
        cout << "Running Search Vertex" << endl;
        ecall_search_node(global_eid, string("1-1-0").c_str());
    } else if (alg == "DEL-VERTEX" || alg == "del-vertex") {
        cout << "Running Delete Vertex" << endl;
        ecall_del_node(global_eid, string("1-1-0").c_str());
    } else if (alg == "DEL-EDGE" || alg == "del-edge") {
        cout << "Running Delete Edge" << endl;
        ecall_del_node(global_eid, string(to_string(testEdgesrc) + "-" + to_string(testEdgeDst) + "-1").c_str());
    } else if (alg == "ADD-EDGE" || alg == "add-edge") {
        cout << "Running Add Edge" << endl;
        ecall_add_node(global_eid, string("1-2-1").c_str(), &edges);
    } else if (alg == "PAGERANK" || alg == "pagerank") {
        cout << "Running PageRank" << endl;
        ecall_PageRank(global_eid);
    } else if (alg == "BFS" || alg == "bfs") {
        cout << "Running BFS" << endl;
        ecall_BFS(global_eid, 1);
    } else if (alg == "OBLIVIOUS-BFS" || alg == "oblivious-bfs") {
        cout << "Running oblivious BFS" << endl;
        ecall_oblivious_BFS(global_eid, 1);
    } else if (alg == "NON-OBLIVIOUS-BFS" || alg == "non-oblivious-bfs") {
        cout << "Running non oblivious BFS" << endl;
        ecall_non_oblivious_BFS(global_eid, 1);
    } else if (alg == "DFS" || alg == "dfs") {
        cout << "Running DFS" << endl;
        ecall_DFS(global_eid, 1);
    } else if (alg == "MST" || alg == "mst") {
        cout << "Running MST" << endl;
        ecall_kruskal_minimum_spanning_tree(global_eid, &edges);
    } else if (alg == "OBLIVIOUS-MST" || alg == "oblivious-mst") {
        cout << "Running Oblivious MST" << endl;
        ecall_oblivious_kruskal_minimum_spanning_tree(global_eid, &edges);
    } else if (alg == "SSSP" || alg == "sssp") {
        cout << "Running SSSP" << endl;
        ecall_efficient_single_source_shortest_path(global_eid, 1);
    } else if (alg == "BFS-OBLIVM" || alg == "bfs-oblivm") {
        cout << "Running BFS-OBLIVM" << endl;
        int maxPad = (int) pow(2, ceil(log2(node_numebr * 2)));
        char* tovisit = new char[ 2 * maxPad * (28 + 8)]; //SGX_AESGCM_IV_SIZE + SGX_AESGCM_MAC_SIZE = 28     sizeof(pair<int,int>)=8
        char* adddata = new char[ 2 * maxPad * (28 + 8)];
        ecall_oblivm_BFS(global_eid, 1, &tovisit, &adddata);
        delete tovisit;
        delete adddata;
    } else if (alg == "DFS-OBLIVM" || alg == "dfs-oblivm") {
        cout << "Running DFS-OBLIVM" << endl;
        int maxPad = (int) pow(2, ceil(log2(node_numebr * 2)));
        unsigned long long pairBlockSize = sizeof (pair<int, int>);
        unsigned long long pairClenSize = AES::GetCiphertextLength((int) (pairBlockSize));
        unsigned long long pairStoreSingleBlockSize = pairClenSize + IV;
        char* tovisit = new char[ 2 * maxPad * pairStoreSingleBlockSize]; //SGX_AESGCM_IV_SIZE + SGX_AESGCM_MAC_SIZE = 28     sizeof(pair<int,int>)=8
        char* adddata = new char[ 2 * maxPad * pairStoreSingleBlockSize];
        ecall_oblivm_DFS(global_eid, 1, &tovisit, &adddata);
        delete tovisit;
        delete adddata;
    } else if (alg == "MST-OBLIVM" || alg == "mst-oblivm") {
        ecall_oblivm_kruskal_minimum_spanning_tree(global_eid, &edges);
    } else if (alg == "SSSP-OBLIVM" || alg == "sssp-oblivm") {
        cout << "Running SSSP-OBLIVM" << endl;
        ecall_oblivm_single_source_shortest_path(global_eid, 1);
    } else if (alg == "OBLIVIOUS-SSSP-OBLIVM" || alg == "oblivious-sssp-oblivm") {
        cout << "Running Oblivious SSSP-OBLIVM" << endl;
        ecall_oblivious_oblivm_single_source_shortest_path(global_eid, 1);
    } else {
        cout << "unknown algorithm" << endl;
    }
    auto exectime = Utilities::stopTimer(5);
    cout << "Time:" << exectime << " Microseconds" << endl;



    /* Destroy the enclave */
    //------------------------------------------------------------------------------------------
    //------------------------------------------------------------------------------------------
    //------------------------------------------------------------------------------------------
    //------------------------------------------------------------------------------------------
    //------------------------------------------------------------------------------------------
    sgx_destroy_enclave(global_eid);





    return 0;
}

