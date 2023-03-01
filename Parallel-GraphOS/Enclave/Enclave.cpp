#include "Enclave.h"
#include <assert.h>
#include "Enclave_t.h"
#include "sgx_tkey_exchange.h"
#include "sgx_tcrypto.h"
#include "string.h"
#include"ObliviousMinHeap.h"
#include <algorithm>
#include <math.h>
#include "OMAP/AES.hpp"
#include "sgx_thread.h"

// This is the public EC key of the SP. The corresponding private EC key is
// used by the SP to sign data used in the remote attestation SIGMA protocol
// to sign channel binding data in MSG2. A successful verification of the
// signature confirms the identity of the SP to the ISV app in remote
// attestation secure channel binding. The public EC key should be hardcoded in
// the enclave or delivered in a trustworthy manner. The use of a spoofed public
// EC key in the remote attestation with secure channel binding session may lead
// to a security compromise. Every different SP the enlcave communicates to
// must have a unique SP public key. Delivery of the SP public key is
// determined by the ISV. The TKE SIGMA protocl expects an Elliptical Curve key
// based on NIST P-256
static const sgx_ec256_public_t g_sp_pub_key = {
    {
        0x72, 0x12, 0x8a, 0x7a, 0x17, 0x52, 0x6e, 0xbf,
        0x85, 0xd0, 0x3a, 0x62, 0x37, 0x30, 0xae, 0xad,
        0x3e, 0x3d, 0xaa, 0xee, 0x9c, 0x60, 0x73, 0x1d,
        0xb0, 0x5b, 0xe8, 0x62, 0x1c, 0x4b, 0xeb, 0x38
    },
    {
        0xd4, 0x81, 0x40, 0xd9, 0x50, 0xe2, 0x57, 0x7b,
        0x26, 0xee, 0xb7, 0x41, 0xe7, 0xc6, 0x14, 0xe2,
        0x24, 0xb7, 0xbd, 0xc9, 0x03, 0xf2, 0x9a, 0x28,
        0xa8, 0x3c, 0xc8, 0x10, 0x11, 0x14, 0x5e, 0x06
    }

};

// Used to store the secret passed by the SP in the sample code. The
// size is forced to be 8 bytes. Expected value is
// 0x01,0x02,0x03,0x04,0x0x5,0x0x6,0x0x7
uint8_t g_secret[8] = {0};


#ifdef SUPPLIED_KEY_DERIVATION

#pragma message ("Supplied key derivation function is used.")

typedef struct _hash_buffer_t {
    uint8_t counter[4];
    sgx_ec256_dh_shared_t shared_secret;
    uint8_t algorithm_id[4];
} hash_buffer_t;

const char ID_U[] = "SGXRAENCLAVE";
const char ID_V[] = "SGXRASERVER";

// Derive two keys from shared key and key id.

bool derive_key(
        const sgx_ec256_dh_shared_t *p_shared_key,
        uint8_t key_id,
        sgx_ec_key_128bit_t *first_derived_key,
        sgx_ec_key_128bit_t *second_derived_key) {
    sgx_status_t sgx_ret = SGX_SUCCESS;
    hash_buffer_t hash_buffer;
    sgx_sha_state_handle_t sha_context;
    sgx_sha256_hash_t key_material;

    memset(&hash_buffer, 0, sizeof (hash_buffer_t));
    /* counter in big endian  */
    hash_buffer.counter[3] = key_id;

    /*convert from little endian to big endian */
    for (size_t i = 0; i < sizeof (sgx_ec256_dh_shared_t); i++) {
        hash_buffer.shared_secret.s[i] = p_shared_key->s[sizeof (p_shared_key->s) - 1 - i];
    }

    sgx_ret = sgx_sha256_init(&sha_context);
    if (sgx_ret != SGX_SUCCESS) {
        return false;
    }
    sgx_ret = sgx_sha256_update((uint8_t*) & hash_buffer, sizeof (hash_buffer_t), sha_context);
    if (sgx_ret != SGX_SUCCESS) {
        sgx_sha256_close(sha_context);
        return false;
    }
    sgx_ret = sgx_sha256_update((uint8_t*) & ID_U, sizeof (ID_U), sha_context);
    if (sgx_ret != SGX_SUCCESS) {
        sgx_sha256_close(sha_context);
        return false;
    }
    sgx_ret = sgx_sha256_update((uint8_t*) & ID_V, sizeof (ID_V), sha_context);
    if (sgx_ret != SGX_SUCCESS) {
        sgx_sha256_close(sha_context);
        return false;
    }
    sgx_ret = sgx_sha256_get_hash(sha_context, &key_material);
    if (sgx_ret != SGX_SUCCESS) {
        sgx_sha256_close(sha_context);
        return false;
    }
    sgx_ret = sgx_sha256_close(sha_context);

    assert(sizeof (sgx_ec_key_128bit_t)* 2 == sizeof (sgx_sha256_hash_t));
    memcpy(first_derived_key, &key_material, sizeof (sgx_ec_key_128bit_t));
    memcpy(second_derived_key, (uint8_t*) & key_material + sizeof (sgx_ec_key_128bit_t), sizeof (sgx_ec_key_128bit_t));

    // memset here can be optimized away by compiler, so please use memset_s on
    // windows for production code and similar functions on other OSes.
    memset(&key_material, 0, sizeof (sgx_sha256_hash_t));

    return true;
}

//isv defined key derivation function id
#define ISV_KDF_ID 2

typedef enum _derive_key_type_t {
    DERIVE_KEY_SMK_SK = 0,
    DERIVE_KEY_MK_VK,
} derive_key_type_t;

sgx_status_t key_derivation(const sgx_ec256_dh_shared_t* shared_key,
        uint16_t kdf_id,
        sgx_ec_key_128bit_t* smk_key,
        sgx_ec_key_128bit_t* sk_key,
        sgx_ec_key_128bit_t* mk_key,
        sgx_ec_key_128bit_t* vk_key) {
    bool derive_ret = false;

    if (NULL == shared_key) {
        return SGX_ERROR_INVALID_PARAMETER;
    }

    if (ISV_KDF_ID != kdf_id) {
        //fprintf(stderr, "\nError, key derivation id mismatch in [%s].", __FUNCTION__);
        return SGX_ERROR_KDF_MISMATCH;
    }

    derive_ret = derive_key(shared_key, DERIVE_KEY_SMK_SK,
            smk_key, sk_key);
    if (derive_ret != true) {
        //fprintf(stderr, "\nError, derive key fail in [%s].", __FUNCTION__);
        return SGX_ERROR_UNEXPECTED;
    }

    derive_ret = derive_key(shared_key, DERIVE_KEY_MK_VK,
            mk_key, vk_key);
    if (derive_ret != true) {
        //fprintf(stderr, "\nError, derive key fail in [%s].", __FUNCTION__);
        return SGX_ERROR_UNEXPECTED;
    }
    return SGX_SUCCESS;
}
#else
#pragma message ("Default key derivation function is used.")
#endif

// This ecall is a wrapper of sgx_ra_init to create the trusted
// KE exchange key context needed for the remote attestation
// SIGMA API's. Input pointers aren't checked since the trusted stubs
// copy them into EPC memory.
//
// @param b_pse Indicates whether the ISV app is using the
//              platform services.
// @param p_context Pointer to the location where the returned
//                  key context is to be copied.
//
// @return Any error return from the create PSE session if b_pse
//         is true.
// @return Any error returned from the trusted key exchange API
//         for creating a key context.

sgx_status_t enclave_init_ra(
        int b_pse,
        sgx_ra_context_t *p_context) {
    // isv enclave call to trusted key exchange library.
    sgx_status_t ret;
    if (b_pse) {
        int busy_retry_times = 2;
        do {
            ret = sgx_create_pse_session();
        } while (ret == SGX_ERROR_BUSY && busy_retry_times--);
        if (ret != SGX_SUCCESS)
            return ret;
    }
#ifdef SUPPLIED_KEY_DERIVATION
    ret = sgx_ra_init_ex(&g_sp_pub_key, b_pse, key_derivation, p_context);
#else
    ret = sgx_ra_init(&g_sp_pub_key, b_pse, p_context);
#endif
    if (b_pse) {
        sgx_close_pse_session();
        return ret;
    }
    return ret;
}


// Closes the tKE key context used during the SIGMA key
// exchange.
//
// @param context The trusted KE library key context.
//
// @return Return value from the key context close API

sgx_status_t SGXAPI enclave_ra_close(
        sgx_ra_context_t context) {
    sgx_status_t ret;
    ret = sgx_ra_close(context);
    return ret;
}


// Verify the mac sent in att_result_msg from the SP using the
// MK key. Input pointers aren't checked since the trusted stubs
// copy them into EPC memory.
//
//
// @param context The trusted KE library key context.
// @param p_message Pointer to the message used to produce MAC
// @param message_size Size in bytes of the message.
// @param p_mac Pointer to the MAC to compare to.
// @param mac_size Size in bytes of the MAC
//
// @return SGX_ERROR_INVALID_PARAMETER - MAC size is incorrect.
// @return Any error produced by tKE  API to get SK key.
// @return Any error produced by the AESCMAC function.
// @return SGX_ERROR_MAC_MISMATCH - MAC compare fails.

sgx_status_t verify_att_result_mac(sgx_ra_context_t context,
        uint8_t* p_message,
        size_t message_size,
        uint8_t* p_mac,
        size_t mac_size) {
    sgx_status_t ret;
    sgx_ec_key_128bit_t mk_key;

    if (mac_size != sizeof (sgx_mac_t)) {
        ret = SGX_ERROR_INVALID_PARAMETER;
        return ret;
    }
    if (message_size > UINT32_MAX) {
        ret = SGX_ERROR_INVALID_PARAMETER;
        return ret;
    }

    do {
        uint8_t mac[SGX_CMAC_MAC_SIZE] = {0};

        ret = sgx_ra_get_keys(context, SGX_RA_KEY_MK, &mk_key);
        if (SGX_SUCCESS != ret) {
            break;
        }
        ret = sgx_rijndael128_cmac_msg(&mk_key,
                p_message,
                (uint32_t) message_size,
                &mac);
        if (SGX_SUCCESS != ret) {
            break;
        }
        if (0 == consttime_memequal(p_mac, mac, sizeof (mac))) {
            ret = SGX_ERROR_MAC_MISMATCH;
            break;
        }

    } while (0);

    return ret;
}


// Generate a secret information for the SP encrypted with SK.
// Input pointers aren't checked since the trusted stubs copy
// them into EPC memory.
//
// @param context The trusted KE library key context.
// @param p_secret Message containing the secret.
// @param secret_size Size in bytes of the secret message.
// @param p_gcm_mac The pointer the the AESGCM MAC for the
//                 message.
//
// @return SGX_ERROR_INVALID_PARAMETER - secret size if
//         incorrect.
// @return Any error produced by tKE  API to get SK key.
// @return Any error produced by the AESGCM function.
// @return SGX_ERROR_UNEXPECTED - the secret doesn't match the
//         expected value.

sgx_status_t put_secret_data(
        sgx_ra_context_t context,
        uint8_t *p_secret,
        uint32_t secret_size,
        uint8_t *p_gcm_mac) {
    sgx_status_t ret = SGX_SUCCESS;
    sgx_ec_key_128bit_t sk_key;

    do {
        if (secret_size != 8) {
            ret = SGX_ERROR_INVALID_PARAMETER;
            break;
        }

        ret = sgx_ra_get_keys(context, SGX_RA_KEY_SK, &sk_key);
        if (SGX_SUCCESS != ret) {
            break;
        }

        uint8_t aes_gcm_iv[12] = {0};
        ret = sgx_rijndael128GCM_decrypt(&sk_key,
                p_secret,
                secret_size,
                &g_secret[0],
                &aes_gcm_iv[0],
                12,
                NULL,
                0,
                (const sgx_aes_gcm_128bit_tag_t *)
                (p_gcm_mac));

        uint32_t i;
        bool secret_match = true;
        for (i = 0; i < secret_size; i++) {
            if (g_secret[i] != i) {
                secret_match = false;
            }
        }

        if (!secret_match) {
            ret = SGX_ERROR_UNEXPECTED;
        }

        // Once the server has the shared secret, it should be sealed to
        // persistent storage for future use. This will prevents having to
        // perform remote attestation until the secret goes stale. Once the
        // enclave is created again, the secret can be unsealed.
    } while (0);
    return ret;
}

int PLAINTEXT_LENGTH = sizeof (GraphNode);
int PLAINTEXT_LENGTH2 = sizeof (pair<int, int>);
int CIPHER_LENGTH = SGX_AESGCM_IV_SIZE + SGX_AESGCM_MAC_SIZE + PLAINTEXT_LENGTH;
int CIPHER_LENGTH2 = SGX_AESGCM_IV_SIZE + SGX_AESGCM_MAC_SIZE + PLAINTEXT_LENGTH2;

sgx_thread_mutex_t global_mutex = SGX_THREAD_MUTEX_INITIALIZER;
sgx_thread_mutex_t omap1_mutex = SGX_THREAD_MUTEX_INITIALIZER;
sgx_thread_mutex_t omap2_mutex = SGX_THREAD_MUTEX_INITIALIZER;
sgx_thread_mutex_t omap3_mutex = SGX_THREAD_MUTEX_INITIALIZER;
sgx_thread_mutex_t omap4_mutex = SGX_THREAD_MUTEX_INITIALIZER;

sgx_thread_mutex_t heap_mutex = SGX_THREAD_MUTEX_INITIALIZER;


int vertexNumber = 0;
int edgeNumber = 0;
int partitions = 0;
#define MY_MAX 9999999

void printf(const char *fmt, ...) {
    char buf[BUFSIZ] = {'\0'};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print_string(buf);
}

void ecall_setup_and_partition_orams(int size, int number) {
    int partSize = size / number;
    partitions = number;
    for (int i = 0; i < number; i++) {
        ecall_setup_oram(partSize);
    }
}

void ecall_start_setup() {
    ecall_start_oram_setup();
}

void ecall_end_setup() {
    for (int i = 0; i < partitions; i++) {
        ecall_end_oram_setup(i);
    }
}

string readOMAP(string omapKey, int id = -1) {
    id = id; //dummy operation to remove compile warning
    std::array< uint8_t, ID_SIZE > keyArray;
    keyArray.fill(0);
    std::copy(omapKey.begin(), omapKey.end(), std::begin(keyArray));
    int index = 0;
    char* value = new char[16];
    unsigned char tmp[SGX_AESGCM_IV_SIZE + SGX_AESGCM_MAC_SIZE + ID_SIZE];
    AES::PRF(omapKey, (int) omapKey.length(), tmp);
    index = tmp[0] % partitions;
    if (index == 0) {
        sgx_thread_mutex_lock(&omap1_mutex);
        ecall_read_node((const char*) keyArray.data(), value, index);
        sgx_thread_mutex_unlock(&omap1_mutex);
    } else if (index == 1) {
        sgx_thread_mutex_lock(&omap2_mutex);
        ecall_read_node((const char*) keyArray.data(), value, index);
        sgx_thread_mutex_unlock(&omap2_mutex);
    } else if (index == 2) {
        sgx_thread_mutex_lock(&omap3_mutex);
        ecall_read_node((const char*) keyArray.data(), value, index);
        sgx_thread_mutex_unlock(&omap3_mutex);
    } else if (index == 3) {
        sgx_thread_mutex_lock(&omap4_mutex);
        ecall_read_node((const char*) keyArray.data(), value, index);
        sgx_thread_mutex_unlock(&omap4_mutex);
    }
    string result(value);
    delete value;
    return result;
}

void writeOMAP(string omapKey, string omapValue, int id = -1) {
    id = id; //dummy operation to remove compile warning
    std::array< uint8_t, ID_SIZE > keyArray;
    keyArray.fill(0);
    std::copy(omapKey.begin(), omapKey.end(), std::begin(keyArray));

    std::array< uint8_t, 16 > valueArray;
    valueArray.fill(0);
    std::copy(omapValue.begin(), omapValue.end(), std::begin(valueArray));

    int index = 0;
    unsigned char tmp[SGX_AESGCM_IV_SIZE + SGX_AESGCM_MAC_SIZE + ID_SIZE];
    AES::PRF(omapKey, (int) omapKey.length(), tmp);
    index = tmp[0] % partitions;

    if (index == 0) {
        sgx_thread_mutex_lock(&omap1_mutex);
        ecall_write_node((const char*) keyArray.data(), (const char*) valueArray.data(), index);
        sgx_thread_mutex_unlock(&omap1_mutex);
    } else if (index == 1) {
        sgx_thread_mutex_lock(&omap2_mutex);
        ecall_write_node((const char*) keyArray.data(), (const char*) valueArray.data(), index);
        sgx_thread_mutex_unlock(&omap2_mutex);
    } else if (index == 2) {
        sgx_thread_mutex_lock(&omap3_mutex);
        ecall_write_node((const char*) keyArray.data(), (const char*) valueArray.data(), index);
        sgx_thread_mutex_unlock(&omap3_mutex);
    } else if (index == 3) {
        sgx_thread_mutex_lock(&omap4_mutex);
        ecall_write_node((const char*) keyArray.data(), (const char*) valueArray.data(), index);
        sgx_thread_mutex_unlock(&omap4_mutex);
    }
}

string readSetOMAP(string omapKey) {
    std::array< uint8_t, ID_SIZE > keyArray;
    keyArray.fill(0);
    std::copy(omapKey.begin(), omapKey.end(), std::begin(keyArray));

    int index = 0;
    char* value = new char[16];
    unsigned char tmp[SGX_AESGCM_IV_SIZE + SGX_AESGCM_MAC_SIZE + ID_SIZE];
    AES::PRF(omapKey, (int) omapKey.length(), tmp);
    index = tmp[0] % partitions;
    if (index == 0) {
        sgx_thread_mutex_lock(&omap1_mutex);
        ecall_read_and_set_node((const char*) keyArray.data(), value, index);
        sgx_thread_mutex_unlock(&omap1_mutex);
    } else if (index == 1) {
        sgx_thread_mutex_lock(&omap2_mutex);
        ecall_read_and_set_node((const char*) keyArray.data(), value, index);
        sgx_thread_mutex_unlock(&omap2_mutex);
    } else if (index == 2) {
        sgx_thread_mutex_lock(&omap3_mutex);
        ecall_read_and_set_node((const char*) keyArray.data(), value, index);
        sgx_thread_mutex_unlock(&omap3_mutex);
    } else if (index == 3) {
        sgx_thread_mutex_lock(&omap4_mutex);
        ecall_read_and_set_node((const char*) keyArray.data(), value, index);
        sgx_thread_mutex_unlock(&omap4_mutex);
    }
    string result(value);
    delete value;
    return result;
}

vector<string> splitData(const string& str, const string& delim) {
    vector<string> tokens;
    size_t prev = 0, pos = 0;
    do {
        pos = str.find(delim, prev);
        if (pos == string::npos) pos = str.length();
        string token = str.substr(prev, pos - prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delim.length();
    } while (pos < str.length() && prev < str.length());
    return tokens;
}

/**
 *  (#-cnt) -> V1,V2,...,Vn
 *  ($-src-cnt) -> dst
 *  (?-id) -> (Number of outgoing edges,Number of incoming edges)
 *  (*-dst-cnt) -> src
 *  (!-src-dst) -> (weight,src_cnt,dst_cnt)
 * @param data
 */
void ecall_del_node(const char* data) {
    string inputData(data);
    vector<string> nodesInfo = splitData(inputData, "-");
    if (nodesInfo[0] == nodesInfo[1]) {
        string vertex = nodesInfo[0];
        string res = readOMAP("?" + vertex);
        vector<string> parts = splitData(res, "-");
        int in_u = stoi(parts[0]);
        int out_u = stoi(parts[1]);
        writeOMAP("?" + vertex, "");

        for (int i = 1; i <= in_u; i++) {
            string src = vertex;
            res = readOMAP("$" + src + "-" + to_string(i));
            string dst = splitData(res, "-")[0];

            res = readOMAP("!" + src + "-" + dst);
            parts = splitData(res, "-");
            string cnt_dst = parts[2];
            writeOMAP("!" + src + "-" + dst, "");

            res = readOMAP("?" + dst);
            parts = splitData(res, "-");
            string in_dst = parts[0];
            string out_dst = parts[1];

            writeOMAP("*" + dst + "-" + cnt_dst, readOMAP("*" + dst + "-" + out_dst));

            in_dst = to_string(stoi(in_dst) - 1);
            writeOMAP("?" + dst, in_dst + "-" + out_dst);
        }

        for (int i = 1; i <= out_u; i++) {
            string dst = vertex;
            res = readOMAP("*" + dst + "-" + to_string(i));
            string src = splitData(res, "-")[0];


            res = readOMAP("!" + src + "-" + dst);
            parts = splitData(res, "-");
            string cnt_src = parts[1];
            writeOMAP("!" + src + "-" + dst, "");

            res = readOMAP("?" + src);
            parts = splitData(res, "-");
            string in_src = parts[0];
            string out_src = parts[1];

            writeOMAP("$" + src + "-" + cnt_src, readOMAP("$" + src + "-" + out_src));

            out_src = to_string(stoi(out_src) - 1);
            writeOMAP("?" + src, in_src + "-" + out_src);
        }


    } else {
        string src = nodesInfo[0];
        string dst = nodesInfo[1];

        string res = readOMAP("!" + src + "-" + dst);
        vector<string> parts = splitData(res, "-");
        string cnt_src = parts[1];
        string cnt_dst = parts[2];
        writeOMAP("!" + src + "-" + dst, "");

        res = readOMAP("?" + src);
        parts = splitData(res, "-");
        string in_src = parts[1];
        string out_src = parts[0];

        res = readOMAP("?" + dst);
        parts = splitData(res, "-");
        string in_dst = parts[1];
        string out_dst = parts[0];

        string tmp1 = readOMAP("$" + src + "-" + out_src);
        string tmp2 = readOMAP("*" + dst + "-" + out_dst);
        writeOMAP("$" + src + "-" + cnt_src, tmp1);
        writeOMAP("*" + dst + "-" + cnt_dst, tmp2);

        out_src = to_string(stoi(out_src) - 1);
        in_dst = to_string(stoi(in_dst) - 1);

        writeOMAP("?" + src, out_src + "-" + in_src);
        writeOMAP("?" + dst, out_dst + "-" + in_dst);

    }
}

void ecall_search_node(const char* data) {
    string inputData(data);
    vector<string> nodesInfo = splitData(inputData, "-");
    if (nodesInfo[0] == nodesInfo[1]) {
        string vertex = nodesInfo[0];
        string res = readOMAP("?" + vertex);
    } else {
        string src = nodesInfo[0];
        string dst = nodesInfo[1];

        string res = readOMAP("!" + src + "-" + dst);
    }
}

void ecall_pad_nodes(char** edgeList) {
    int maxPad = (int) pow(2, ceil(log2(edgeNumber)));
    for (int i = edgeNumber; i < maxPad; i++) {
        GraphNode* node = new GraphNode();
        node->src_id = "-1";
        node->dst_id = "-1";
        node->weight = MY_MAX;

        block b = GraphNode::convertNodeToBlock(node);
        encrypt(b.data(), PLAINTEXT_LENGTH, (uint8_t*) (*edgeList) + i * CIPHER_LENGTH);
    }
}

/**
 *  (#-cnt) -> V1,V2,...,Vn
 *  ($-src-cnt) -> dst
 *  (?-id) -> (Number of outgoing edges,Number of incoming edges)
 *  (*-dst-cnt) -> src
 *  (!-src-dst) -> (weight,src_cnt,dst_cnt)
 * @param data
 */
void ecall_add_node(const char* data, char** edgeList) {
    string inputData(data);
    vector<string> nodesInfo = splitData(inputData, "-");
    if (nodesInfo[0] == nodesInfo[1]) {
        string vertex = nodesInfo[0];
        vertexNumber++;
    } else {
        string src = nodesInfo[0];
        string dst = nodesInfo[1];
        string weight = nodesInfo[2];

        GraphNode* node = new GraphNode();
        node->src_id = src;
        node->dst_id = dst;
        node->weight = stoi(weight);

        block b = GraphNode::convertNodeToBlock(node);
        encrypt(b.data(), PLAINTEXT_LENGTH, (uint8_t*) (*edgeList) + edgeNumber * CIPHER_LENGTH);

        string srcVertex = readOMAP("?" + src);
        int outGoingEdges = 1;
        int incomingEdges = 0;
        if (srcVertex != "") {
            vector<string> parts = splitData(srcVertex, "-");
            outGoingEdges = stoi(parts[0]) + 1;
            incomingEdges = stoi(parts[1]);
        }
        writeOMAP("?" + src, to_string(outGoingEdges) + "-" + to_string(incomingEdges));
        writeOMAP("$" + src + "-" + to_string(outGoingEdges), dst + "-" + weight);

        string dstVertex = readOMAP("?" + dst);
        outGoingEdges = 0;
        incomingEdges = 1;

        if (dstVertex != "") {
            vector<string> parts = splitData(dstVertex, "-");
            outGoingEdges = stoi(parts[0]);
            incomingEdges = stoi(parts[1]) + 1;
        }
        writeOMAP("?" + dst, to_string(outGoingEdges) + "-" + to_string(incomingEdges));
        writeOMAP("*" + dst + "-" + to_string(incomingEdges), src + "-" + weight);
        edgeNumber++;

        writeOMAP("!" + src + "-" + dst, weight + "-" + to_string(outGoingEdges) + "-" + to_string(incomingEdges));

        delete node;
    }
}

void ecall_PageRank() {
    int vertexCounter = 1;
    string vertex = "";
    do {
        string omapKey = "#" + to_string(vertexCounter);
        vertex = readOMAP(omapKey);
        if (vertex != "") {
            double weightedRank = 0;
            int cnt = 1;
            omapKey = "*" + vertex + "-" + to_string(cnt);
            string currentEdge = readOMAP(omapKey);
            while (currentEdge != "") {
                weightedRank += 1;
                cnt++;
                omapKey = "*" + vertex + "-" + to_string(cnt);
                currentEdge = readOMAP(omapKey);
            }
            weightedRank = weightedRank * 0.85 + 0.15;
            //printf("Vertex:%s Rank:%f\n",vertex,weightedRank);
            //writeOMAP(string("@" + vertex), to_string(weightedRank));
        }
        vertexCounter++;
    } while (vertex != "");
}

int globalQCnt = 0;

void ecall_put_in_queue(int id, int src, int from, int to) {
    int cnt = from;
    string source = to_string(src);
    string omapKey = "$" + source + "-" + to_string(cnt);
    string dstStr = readOMAP(omapKey, id);
    while (cnt <= to) {
        vector<string> parts = splitData(dstStr, "-");
        string dst = parts[0];
        string visited = readOMAP(string("%") + dst, id);
        if (visited == "") {
            int localCounter;
            sgx_thread_mutex_lock(&global_mutex);
            localCounter = globalQCnt;
            globalQCnt++;
            sgx_thread_mutex_unlock(&global_mutex);
            writeOMAP(string("@" + to_string(localCounter)), dst, id);
            writeOMAP(string("%") + dst, to_string(localCounter), id);
        } else {
            writeOMAP("&0", "", id);
            writeOMAP("&0", "", id);
            globalQCnt = globalQCnt;
        }
        cnt++;
        omapKey = "$" + source + "-" + to_string(cnt);
        dstStr = readOMAP(omapKey, id);
    }
}

void ecall_parallel_BFS(int src) {
    globalQCnt = 1;
    int curQCnt = 1;
    string source = to_string(src);
    writeOMAP(string("@" + to_string(globalQCnt)), source);
    writeOMAP(string("%") + source, to_string(globalQCnt));
    globalQCnt++;
    while (curQCnt != globalQCnt) {
        source = readOMAP(string("@" + to_string(curQCnt)));
        curQCnt++;
        printf("Node:%s Visisted\n", source.c_str());
        int numOfNeighbor = 0;
        string tmp = readOMAP(string("?" + source));
        numOfNeighbor = stoi(splitData(tmp, "-")[0]);

        for (int i = 0; i < partitions; i++) {
            if (i == partitions - 1) {
                ocall_run_bfs_worker(i, stoi(source), ((numOfNeighbor / partitions) * i) + 1, numOfNeighbor);
            } else {
                ocall_run_bfs_worker(i, stoi(source), ((numOfNeighbor / partitions) * i) + 1, ((numOfNeighbor / partitions)*(i + 1)));
            }
        }
        ocall_wait_for_bfs_join();
    }
}

void ecall_BFS(int src) {
    int Qcnt = 1;
    int curQCnt = 1;
    string source = to_string(src);
    writeOMAP(string("@" + to_string(Qcnt)), source);
    writeOMAP(string("%") + source, to_string(Qcnt));
    Qcnt++;
    while (curQCnt != Qcnt) {
        source = readOMAP(string("@" + to_string(curQCnt)));
        curQCnt++;
        printf("Node:%s Visisted\n", source.c_str());
        int cnt = 1;
        string omapKey = "$" + source + "-" + to_string(cnt);
        string dstStr = readOMAP(omapKey);
        while (dstStr != "") {
            vector<string> parts = splitData(dstStr, "-");
            string dst = parts[0];
            string visited = readOMAP(string("%") + dst);
            if (visited == "") {
                writeOMAP(string("@" + to_string(Qcnt)), dst);
                writeOMAP(string("%") + dst, to_string(Qcnt));
                Qcnt++;
            } else {
                writeOMAP("&0", "");
                writeOMAP("&0", "");
                Qcnt = Qcnt;
            }
            cnt++;
            omapKey = "$" + source + "-" + to_string(cnt);
            dstStr = readOMAP(omapKey);
        }
    }
}

void ecall_DFS(int src) {
    int Qcnt = 1;
    string source = to_string(src);
    writeOMAP(string("@" + to_string(Qcnt)), source);
    writeOMAP(string("%") + source, to_string(Qcnt));
    while (0 != Qcnt) {
        source = readOMAP(string("@" + to_string(Qcnt)));
        Qcnt--;
        printf("Node:%s Visisted\n", source);
        int cnt = 1;
        string omapKey = "$" + source + "-" + to_string(cnt);
        string dstStr = readOMAP(omapKey);
        while (dstStr != "") {
            vector<string> parts = splitData(dstStr, "-");
            string dst = parts[0];
            string visited = readOMAP(string("%") + dst);
            if (visited == "") {
                Qcnt++;
                writeOMAP(string("@" + to_string(Qcnt)), dst);
                writeOMAP(string("%") + dst, to_string(Qcnt));
            } else {
                Qcnt = Qcnt;
                writeOMAP("&0", "");
                writeOMAP("&0", "");
            }
            cnt++;
            omapKey = "$" + source + "-" + to_string(cnt);
            dstStr = readOMAP(omapKey);
        }
    }
}

pair<int, int> getToVisit(char** tovisit, int index) {
    uint8_t plaintext[PLAINTEXT_LENGTH2];
    decrypt((uint8_t*) (*tovisit) + index*CIPHER_LENGTH2, CIPHER_LENGTH2, plaintext);
    block c1(plaintext, plaintext + PLAINTEXT_LENGTH2);
    pair<int, int> pairValue;
    std::array<byte_t, sizeof (pair<int, int>) > arr;
    std::copy(c1.begin(), c1.begin() + sizeof (pair<int, int>), arr.begin());
    from_bytes(arr, pairValue);
    return pairValue;
}

void setToVisit(char** tovisit, int index, pair<int, int> data) {
    std::array<byte_t, sizeof (pair<int, int>) > data1 = to_bytes(data);
    block b1(data1.begin(), data1.end());
    encrypt(b1.data(), sizeof (pair<int, int>), (uint8_t*) (*tovisit) + index * CIPHER_LENGTH2);
}

void setAdd(char** add, int index, pair<int, int> data) {
    std::array<byte_t, sizeof (pair<int, int>) > data1 = to_bytes(data);
    block b1(data1.begin(), data1.end());
    encrypt(b1.data(), sizeof (pair<int, int>), (uint8_t*) (*add) + index * CIPHER_LENGTH2);
}

void mergeAddAndTovisit(char** add, char** tovisit) {
    memcpy((*tovisit) + CIPHER_LENGTH2*vertexNumber, (*add), vertexNumber * CIPHER_LENGTH2);
}

void compAndSwap2(char** tovisit, int i, int j, bool dir, int step, bool BFS) {
    pair<int, int> lhs = getToVisit(tovisit, i);
    pair<int, int> rhs = getToVisit(tovisit, j);

    if (step == 1) {
        if (dir == ((lhs.first > rhs.first) || (BFS == false && lhs.first == rhs.first && rhs.second > lhs.second)
                || (BFS == true && lhs.first == rhs.first && rhs.second < lhs.second))) {
            setToVisit(tovisit, i, rhs);
            setToVisit(tovisit, j, lhs);

        } else {
            setToVisit(tovisit, i, lhs);
            setToVisit(tovisit, j, rhs);
        }
    } else {
        if (dir == ((BFS == false && lhs.first != MY_MAX && rhs.first != MY_MAX && lhs.second != MY_MAX && rhs.second != MY_MAX && lhs.second < rhs.second) ||
                (BFS == true && lhs.first != MY_MAX && rhs.first != MY_MAX && lhs.second != MY_MAX && rhs.second != MY_MAX && lhs.second > rhs.second) ||
                (lhs.first != MY_MAX && rhs.first != MY_MAX && lhs.second == MY_MAX && rhs.second != MY_MAX) ||
                (lhs.first == MY_MAX && rhs.first != MY_MAX))) {
            setToVisit(tovisit, i, rhs);
            setToVisit(tovisit, j, lhs);

        } else {
            setToVisit(tovisit, i, lhs);
            setToVisit(tovisit, j, rhs);
        }
    }
}

void bitonicMerge2(char** tovisit, int low, int cnt, bool dir, int step, bool BFS) {
    if (cnt > 1) {
        int k = cnt / 2;
        for (int i = low; i < low + k; i++) {
            compAndSwap2(tovisit, i, i + k, dir, step, BFS);
        }
        bitonicMerge2(tovisit, low, k, dir, step, BFS);
        bitonicMerge2(tovisit, low + k, k, dir, step, BFS);
    }
}

void bitonicSort2(char** tovisit, int low, int cnt, bool dir, int step, bool BFS) {
    if (cnt > 1) {
        int k = cnt / 2;
        bitonicSort2(tovisit, low, k, true, step, BFS);
        bitonicSort2(tovisit, low + k, k, false, step, BFS);
        bitonicMerge2(tovisit, low, cnt, dir, step, BFS);
    }
}

void ecall_oblivm_DFS(int src, char** tovisit, char** add) {
    for (int i = 0; i < vertexNumber; i++) {
        if (i == src - 1) {
            setToVisit(tovisit, i, pair<int, int>(src, 0));
        } else {
            setToVisit(tovisit, i, pair<int, int>(MY_MAX, MY_MAX));
        }
        setAdd(add, i, pair<int, int>(MY_MAX, MY_MAX));
    }

    int maxPad = (int) pow(2, ceil(log2(vertexNumber * 2)));
    for (int i = vertexNumber; i < maxPad; i++) {
        setToVisit(tovisit, i, pair<int, int>(MY_MAX, MY_MAX));
    }

    for (int i = 0; i < vertexNumber; i++) {
        pair<int, int> curPair = getToVisit(tovisit, 0);
        int u = curPair.first;
        setToVisit(tovisit, 0, pair<int, int>(u, MY_MAX));

        printf("Node:%d Visisted\n", u);

        for (int j = 0; j < vertexNumber; j++) {
            string omapKey = "!" + to_string(u) + "-" + to_string(j + 1);
            string weight = readOMAP(omapKey);
            if (weight != "") {
                setAdd(add, j, pair<int, int>(j + 1, i + 1));
            } else {
                setAdd(add, j, pair<int, int>(MY_MAX, MY_MAX));
            }
        }

        mergeAddAndTovisit(add, tovisit);

        bitonicSort2(tovisit, 0, maxPad, true, 1, false);

        int lastV = -1;
        for (int j = 0; j < vertexNumber * 2; j++) {
            if (lastV != getToVisit(tovisit, j).first) {
                lastV = getToVisit(tovisit, j).first;
                continue;
            } else {
                setToVisit(tovisit, j, pair<int, int>(MY_MAX, MY_MAX));
            }
        }

        bitonicSort2(tovisit, 0, maxPad, true, 2, false);
    }
}

void ecall_oblivm_BFS(int src, char** tovisit, char** add) {
    for (int i = 0; i < vertexNumber; i++) {
        if (i == src - 1) {
            setToVisit(tovisit, i, pair<int, int>(src, 0));
        } else {
            setToVisit(tovisit, i, pair<int, int>(MY_MAX, MY_MAX));
        }
        setAdd(add, i, pair<int, int>(MY_MAX, MY_MAX));
    }

    int maxPad = (int) pow(2, ceil(log2(vertexNumber * 2)));
    for (int i = vertexNumber; i < maxPad; i++) {
        setToVisit(tovisit, i, pair<int, int>(MY_MAX, MY_MAX));
    }

    for (int i = 0; i < vertexNumber; i++) {
        pair<int, int> curPair = getToVisit(tovisit, 0);
        int u = curPair.first;
        setToVisit(tovisit, 0, pair<int, int>(u, MY_MAX));

        //        printf("Node:%d Visisted\n", u);

        for (int j = 0; j < vertexNumber; j++) {
            string omapKey = "!" + to_string(u) + "-" + to_string(j + 1);
            string weight = readOMAP(omapKey);
            if (weight != "") {
                setAdd(add, j, pair<int, int>(j + 1, i + 1));
            } else {
                setAdd(add, j, pair<int, int>(MY_MAX, MY_MAX));
            }
        }

        mergeAddAndTovisit(add, tovisit);

        bitonicSort2(tovisit, 0, maxPad, true, 1, true);

        int lastV = -1;
        for (int j = 0; j < vertexNumber * 2; j++) {
            if (lastV != getToVisit(tovisit, j).first) {
                lastV = getToVisit(tovisit, j).first;
                continue;
            } else {
                setToVisit(tovisit, j, pair<int, int>(MY_MAX, MY_MAX));
            }
        }

        bitonicSort2(tovisit, 0, maxPad, true, 2, true);
    }
}

void compAndSwap(char** edgeList, int i, int j, bool dir) {
    uint8_t* plaintext = new uint8_t[PLAINTEXT_LENGTH];
    decrypt((uint8_t*) (*edgeList) + i*CIPHER_LENGTH, CIPHER_LENGTH, plaintext);
    block c1(plaintext, plaintext + PLAINTEXT_LENGTH);
    GraphNode* left = GraphNode::convertBlockToNode(c1);

    decrypt((uint8_t*) (*edgeList) + j * CIPHER_LENGTH, CIPHER_LENGTH, plaintext);
    block c2(plaintext, plaintext + PLAINTEXT_LENGTH);
    GraphNode* right = GraphNode::convertBlockToNode(c2);

    if (dir == (left->weight > right->weight)) {
        block b1 = GraphNode::convertNodeToBlock(right);
        encrypt(b1.data(), PLAINTEXT_LENGTH, (uint8_t*) (*edgeList) + i * CIPHER_LENGTH);
        block b2 = GraphNode::convertNodeToBlock(left);
        encrypt(b2.data(), PLAINTEXT_LENGTH, (uint8_t*) (*edgeList) + j * CIPHER_LENGTH);
    } else {
        block b1 = GraphNode::convertNodeToBlock(left);
        encrypt(b1.data(), PLAINTEXT_LENGTH, (uint8_t*) (*edgeList) + i * CIPHER_LENGTH);
        block b2 = GraphNode::convertNodeToBlock(right);
        encrypt(b2.data(), PLAINTEXT_LENGTH, (uint8_t*) (*edgeList) + j * CIPHER_LENGTH);
    }
    delete left;
    delete right;
    delete[] plaintext;
}

void bitonicMerge(char** edgeList, int low, int cnt, bool dir) {
    if (cnt > 1) {
        int k = cnt / 2;
        for (int i = low; i < low + k; i++)
            compAndSwap(edgeList, i, i + k, dir);
        bitonicMerge(edgeList, low, k, dir);
        bitonicMerge(edgeList, low + k, k, dir);
    }
}

void bitonicSort(char** edgeList, int low, int cnt, bool dir) {
    if (cnt > 1) {
        int k = cnt / 2;
        bitonicSort(edgeList, low, k, true);
        bitonicSort(edgeList, low + k, k, false);
        bitonicMerge(edgeList, low, cnt, dir);
    }
}

string root(string x) {
    string id = readOMAP(string("/" + x));
    while (id != x) {
        string newId = readOMAP(string("/" + id));
        writeOMAP("/" + x, newId);
        id = newId;
        x = id;
        id = readOMAP(string("/" + x));
    }
    return x;
}

void ecall_oblivm_kruskal_minimum_spanning_tree(char** edgeList) {
    int maxPad = (int) pow(2, ceil(log2(edgeNumber)));
    bitonicSort(edgeList, 0, maxPad, true);
    int cnt = 0;

    for (int i = 1; i <= vertexNumber; i++) {
        writeOMAP("/" + to_string(i), to_string(i));
    }

    int state = 0;
    string x, id, srcRoot, dstRoot;
    GraphNode* node = NULL;

    while (cnt < edgeNumber) {
        if (state == 0) {
            uint8_t* plaintext = new uint8_t[PLAINTEXT_LENGTH];
            decrypt((uint8_t*) (*edgeList) + cnt*CIPHER_LENGTH, CIPHER_LENGTH, plaintext);
            block c1(plaintext, plaintext + PLAINTEXT_LENGTH);
            node = GraphNode::convertBlockToNode(c1);
            delete[] plaintext;
            x = node->src_id;
            id = readOMAP(string("/" + x));
            readOMAP(string("/" + x));
            state = 1;
        } else if (state == 1) {
            if (id != x) {
                string newId = readOMAP(string("/" + id));
                writeOMAP("/" + x, newId);
                id = newId;
                x = id;
                id = readOMAP(string("/" + x));
                state = 1;
            } else {
                srcRoot = x;
                x = node->dst_id;
                id = readOMAP(string("/" + x));
                readOMAP(string("/" + x));
                state = 2;
            }
        } else if (state == 2) {
            if (id != x) {
                string newId = readOMAP(string("/" + id));
                writeOMAP("/" + x, newId);
                id = newId;
                x = id;
                id = readOMAP(string("/" + x));
                state = 2;
            } else {
                dstRoot = x;
                readOMAP(string("/0"));
                readOMAP(string("/0"));
                state = 3;
            }
        } else if (state == 3) {
            if (srcRoot != dstRoot) {
                id = readOMAP(string("/" + dstRoot));
                writeOMAP("/" + srcRoot, id);
                //            printf("%s to %s with weight %d\n", node->src_id, node->dst_id, node->weight);
                state = 0;
                cnt++;
                delete node;
            } else {
                readOMAP("/" + srcRoot);
                readOMAP("/" + srcRoot);
                state = 0;
                cnt++;
                delete node;
            }
        }
    }
}

void ecall_kruskal_minimum_spanning_tree(char** edgeList) {
    int maxPad = (int) pow(2, ceil(log2(edgeNumber)));
    bitonicSort(edgeList, 0, maxPad, true);
    int cnt = 0;

    for (int i = 1; i <= vertexNumber; i++) {
        writeOMAP("/" + to_string(i), to_string(i));
    }

    while (cnt < edgeNumber) {
        uint8_t* plaintext = new uint8_t[PLAINTEXT_LENGTH];
        decrypt((uint8_t*) (*edgeList) + cnt*CIPHER_LENGTH, CIPHER_LENGTH, plaintext);
        block c1(plaintext, plaintext + PLAINTEXT_LENGTH);
        GraphNode* node = GraphNode::convertBlockToNode(c1);
        delete[] plaintext;

        string srcRoot = root(node->src_id);
        string dstRoot = root(node->dst_id);

        if (srcRoot != dstRoot) {
            string id = readOMAP(string("/" + dstRoot));
            writeOMAP("/" + srcRoot, id);
            //            printf("%s to %s with weight %d\n", node->src_id, node->dst_id, node->weight);
        } else {
            readOMAP("/" + srcRoot);
        }
        cnt++;
        delete node;
    }
}

int minDistance() {
    // Initialize min value 
    int min = INT_MAX, min_index = -1;

    for (int v = 1; v <= vertexNumber; v++) {
        auto parts = splitData(readOMAP("/" + to_string(v)), "-");
        string sptSet = parts[1];
        int distV = stoi(parts[0]);
        if (sptSet == "0" && distV <= min) {
            min = distV;
            min_index = v;
        }
    }
    return min_index;
}

//SSSP with scn of vertext list

void ecall_single_source_shortest_path(int src) {
    for (int i = 1; i <= vertexNumber; i++) {
        writeOMAP("/" + to_string(i), to_string(INT_MAX) + "-0");
    }

    writeOMAP("/" + to_string(src), "0-0"); //dist[src] = 0

    for (int count = 1; count < vertexNumber; count++) {
        int u = minDistance();

        readSetOMAP("/" + to_string(u));

        for (int v = 1; v <= vertexNumber; v++) {

            string vData = readOMAP("/" + to_string(v));
            auto tmpParts = splitData(vData, "-");
            string sptSet = tmpParts[1];
            int distU = stoi(splitData(readOMAP("/" + to_string(u)), "-")[0]);
            int distV = stoi(tmpParts[0]);
            string edge = readOMAP("!" + to_string(u) + "-" + to_string(v));
            int weight = -1;
            if (edge != "") {
                vector<string> parts = splitData(edge, "-");
                weight = stoi(parts[0]);
            }

            if (sptSet == "0" && edge != "" && distU != INT_MAX && distU + weight < distV) {
                writeOMAP("/" + to_string(v), to_string(distU + weight) + "-0");
            }
        }
    }

    //    printf("Vertex   Distance from Source\n");
    //    for (int i = 1; i <= vertexNumber; i++){
    //        printf("%d tt %s\n", i, readOMAP("/" + to_string(i)).c_str());
    //    }
}

//SSSP with min heap

void ecall_efficient_single_source_shortest_path(int src) {
    ObliviousMinHeap* minHeap = new ObliviousMinHeap(vertexNumber);

    for (int i = 1; i <= vertexNumber; i++) {
        writeOMAP("/" + to_string(i), to_string(MY_MAX));
        minHeap->setNewMinHeapNode(i - 1, i - 1, MY_MAX);
    }

    writeOMAP("/" + to_string(src), "0");
    minHeap->decreaseKey(src - 1, 0);

    while (!minHeap->isEmpty()) {
        int u = (minHeap->extractMinID() + 1);
        int cnt = 1;
        string omapKey = "$" + to_string(u) + "-" + to_string(cnt);
        string dstStr = readOMAP(omapKey);

        while (dstStr != "") {
            auto parts = splitData(dstStr, "-");
            int v = stoi(parts[0]);
            int weight = stoi(parts[1]);

            int distU = stoi(readOMAP("/" + to_string(u)));
            int distV = stoi(readOMAP("/" + to_string(v)));

            if (minHeap->isInMinHeap(v - 1) && distU != MY_MAX && weight + distU < distV) {
                writeOMAP("/" + to_string(v), to_string(distU + weight));
                minHeap->decreaseKey(v - 1, distU + weight);
            }
            cnt++;
            omapKey = "$" + to_string(u) + "-" + to_string(cnt);
            dstStr = readOMAP(omapKey);
        }
    }

    //    printf("Vertex   Distance from Source\n");
    //    for (int i = 1; i <= vertexNumber; i++) {
    //        printf("%d tt %s\n", i, readOMAP("/" + to_string(i)).c_str());
    //    }
}

//SSSP with oblivm version min heap

void ecall_oblivm_single_source_shortest_path(int src) {
    ecall_setup_oheap();

    ocall_start_timer(34);
    for (int i = 1; i <= vertexNumber; i++) {
        writeOMAP("/" + to_string(i), to_string(MY_MAX));
    }

    writeOMAP("/" + to_string(src), "0");
    ecall_set_new_minheap_node(src - 1, 0);

    bool innerloop = false;
    string dstStr, omapKey;
    int u = -1, cnt = 1, distu = -1, finaldistU = -1;

    for (int i = 0; i < vertexNumber + edgeNumber; i++) {
        if (i % 10 == 0) {
            printf("%d/%d\n", i, vertexNumber + edgeNumber);
        }
        if (innerloop == false) {
            u = -1;
            distu = -1;
            ecall_extract_min_id(&u, &distu);
            if (u == -1) {
                u = u;
                finaldistU = -2;
                writeOMAP("/-", "");
            } else {
                u++;
                string readData = readOMAP("/" + to_string(u));
                finaldistU = stoi(readData);
            }

            if (finaldistU == distu) {
                cnt = 1;
                omapKey = "$" + to_string(u) + "-" + to_string(cnt);
                dstStr = readOMAP(omapKey);
                if (dstStr != "") {
                    innerloop = true;
                } else {
                    innerloop = false;
                }
            } else {
                writeOMAP("/-", "");
            }
            writeOMAP("/-", "");
        } else {
            auto parts = splitData(dstStr, "-");
            int v = stoi(parts[0]);
            int weight = stoi(parts[1]);
            int distU = finaldistU;
            int distV = stoi(readOMAP("/" + to_string(v)));

            if (weight + distU < distV) {
                writeOMAP("/" + to_string(v), to_string(distU + weight));
                ecall_set_new_minheap_node(v - 1, distU + weight);
            } else {
                writeOMAP("/-", "");
                ecall_dummy_heap_op();
            }
            cnt++;
            omapKey = "$" + to_string(u) + "-" + to_string(cnt);
            dstStr = readOMAP(omapKey);
            if (dstStr != "") {
                innerloop = true;
            } else {
                innerloop = false;
            }
        }
    }

    //    printf("Vertex   Distance from Source\n");
    //    for (int i = 1; i <= vertexNumber; i++) {
    //        printf("%d tt %s\n", i, readOMAP("/" + to_string(i)).c_str());
    //    }
}

vector<pair<int, int> > heapUpdates;

void ecall_check_neighbor(int id, int finaldistU, int u, int frm, int to) {
    int cnt = frm;
    string omapKey = "$" + to_string(u) + "-" + to_string(cnt);
    string dstStr = readOMAP(omapKey, id);
    while (cnt <= to) {
        auto parts = splitData(dstStr, "-");
        int v = stoi(parts[0]);
        int weight = stoi(parts[1]);
        int distU = finaldistU;
        int distV = stoi(readOMAP("/" + to_string(v)));

        if (weight + distU < distV) {
            writeOMAP("/" + to_string(v), to_string(distU + weight));
            sgx_thread_mutex_lock(&heap_mutex);
            heapUpdates.push_back(pair<int, int>(v - 1, distU + weight));
            sgx_thread_mutex_unlock(&heap_mutex);
        } else {
            writeOMAP("/-", "");
            sgx_thread_mutex_lock(&heap_mutex);
            heapUpdates.push_back(pair<int, int>(-1, -1));
            sgx_thread_mutex_unlock(&heap_mutex);
        }
        cnt++;
        omapKey = "$" + to_string(u) + "-" + to_string(cnt);
        dstStr = readOMAP(omapKey);
    }
}

void ecall_parallel_SSSP(int src) {
    ecall_setup_oheap();

    ocall_start_timer(34);
    for (int i = 1; i <= vertexNumber; i++) {
        writeOMAP("/" + to_string(i), to_string(MY_MAX));
    }

    writeOMAP("/" + to_string(src), "0");
    ecall_set_new_minheap_node(src - 1, 0);

    string dstStr, omapKey;
    int u = -1, cnt = 1, distu = -1, finaldistU = -1;

    for (int i = 0; i < (2 * vertexNumber + edgeNumber); i++) {
        if (i % 10 == 0) {
            printf("%d/%d\n", i, vertexNumber + edgeNumber);
        }
        u = -1;
        distu = -1;
        ecall_extract_min_id(&u, &distu);
        if (u == -1) {
            u = u;
            finaldistU = -2;
            writeOMAP("/-", "");
        } else {
            u++;
            string readData = readOMAP("/" + to_string(u));
            finaldistU = stoi(readData);
        }

        if (finaldistU == distu) {
            cnt = 1;
            omapKey = "$" + to_string(u) + "-" + to_string(cnt);

            int numOfNeighbor = 0;
            string tmp = readOMAP(string("?" + to_string(u)));
            numOfNeighbor = stoi(splitData(tmp, "-")[0]);

            for (int j = 0; j < partitions; j++) {
                if (j == partitions - 1) {
                    ocall_run_sssp_worker(j, finaldistU, u, ((numOfNeighbor / partitions) * j) + 1, numOfNeighbor);
                } else {
                    ocall_run_sssp_worker(j, finaldistU, u, ((numOfNeighbor / partitions) * j) + 1, ((numOfNeighbor / partitions)*(j + 1)));
                }
            }
            ocall_wait_for_sssp_join();
            for (pair<int, int> item : heapUpdates) {
                int vert = item.first;
                int dis = item.second;
                if (vert == -1 && dis == -1) {
                    ecall_dummy_heap_op();
                } else {
                    ecall_set_new_minheap_node(vert, dis);
                }
            }
            heapUpdates.clear();
            writeOMAP("/-", "");
        }
    }

    //    printf("Vertex   Distance from Source\n");
    //    for (int i = 1; i <= vertexNumber; i++) {
    //        printf("%d tt %s\n", i, readOMAP("/" + to_string(i)).c_str());
    //    }
}