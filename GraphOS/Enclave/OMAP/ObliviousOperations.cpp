#include "ObliviousOperations.h"
#include "../Enclave.h"
#include "Enclave_t.h"

long long ObliviousOperations::storeSingleBlockSize;
long long ObliviousOperations::single_block_clen_size;
unsigned long long ObliviousOperations::blockSize;
size_t ObliviousOperations::single_block_plaintext_size;
bytes<Key> ObliviousOperations::key;

vector<Node*> ObliviousOperations::setupCache1;
vector<Node*> ObliviousOperations::setupCache2;
unsigned long long ObliviousOperations::currentBatchBegin1;
unsigned long long ObliviousOperations::currentBatchBegin2;
bool ObliviousOperations::isLeftBatchUpdated;
unsigned long long ObliviousOperations::totalNumberOfNodes;

ObliviousOperations::ObliviousOperations() {
}

ObliviousOperations::~ObliviousOperations() {
}

void ObliviousOperations::oblixmergesort(std::vector<Node*>* data) {
    if (data->size() == 0 || data->size() == 1) {
        return;
    }
    int len = data->size();
    int t = ceil(log2(len));
    long long p = 1 << (t - 1);

    while (p > 0) {
        long long q = 1 << (t - 1);
        long long r = 0;
        long long d = p;

        while (d > 0) {
            long long i = 0;
            while (i < len - d) {
                if ((i & p) == r) {
                    long long j = i + d;
                    if (i != j) {
                        int node_cmp = Node::CTcmp((*data)[j]->evictionNode, (*data)[i]->evictionNode);
                        int dummy_blocks_last = Node::CTcmp((*data)[i]->isDummy, (*data)[j]->isDummy);
                        int same_nodes = Node::CTeq(node_cmp, 0);
                        bool cond = Node::CTeq(Node::conditional_select(dummy_blocks_last, node_cmp, same_nodes), -1);
                        Node::conditional_swap((*data)[i], (*data)[j], cond);
                    }
                }
                i += 1;
            }
            d = q - p;
            q /= 2;
            r = p;
        }
        p /= 2;
    }
    std::reverse(data->begin(), data->end());
}

int ObliviousOperations::greatest_power_of_two_less_than(int n) {
    int k = 1;
    while (k > 0 && k < n) {
        k = k << 1;
    }
    return k >> 1;
}

void ObliviousOperations::bitonicSort(vector<Node*>* nodes) {
    int len = nodes->size();
    bitonic_sort(nodes, 0, len, 1);
}

void ObliviousOperations::bitonicSort(unsigned long long len) {
    bitonic_sort(0, len, 1);
    flushCache();
}

void ObliviousOperations::bitonic_sort(vector<Node*>* nodes, int low, int n, int dir) {
    if (n > 1) {
        int middle = n / 2;
        bitonic_sort(nodes, low, middle, !dir);
        bitonic_sort(nodes, low + middle, n - middle, dir);
        bitonic_merge(nodes, low, n, dir);
    }
}

void ObliviousOperations::bitonic_sort(int low, int n, int dir) {
    if (n > 1) {
        int middle = n / 2;
        bitonic_sort(low, middle, !dir);
        bitonic_sort(low + middle, n - middle, dir);
        bitonic_merge(low, n, dir);
    }
}

void ObliviousOperations::bitonic_merge(vector<Node*>* nodes, int low, int n, int dir) {
    if (n > 1) {
        int m = greatest_power_of_two_less_than(n);

        for (int i = low; i < (low + n - m); i++) {
            if (i != (i + m)) {
                compare_and_swap((*nodes)[i], (*nodes)[i + m], dir);
            }
        }

        bitonic_merge(nodes, low, m, dir);
        bitonic_merge(nodes, low + m, n - m, dir);
    }
}

void ObliviousOperations::bitonic_merge(int low, int n, int dir) {
    if (n > 1) {
        int m = greatest_power_of_two_less_than(n);

        for (int i = low; i < (low + n - m); i++) {
            if (i != (i + m)) {
                Node* left = getNode(i);
                Node* right = getNode(i + m);
                compare_and_swap(left, right, dir);
                setNode(i, left);
                setNode(i + m, right);
            }
        }

        bitonic_merge(low, m, dir);
        bitonic_merge(low + m, n - m, dir);
    }
}

void ObliviousOperations::compare_and_swap(Node* item_i, Node* item_j, int dir) {
    int res = Node::CTcmp(item_i->evictionNode, item_j->evictionNode);
    int cmp = Node::CTeq(res, 1);
    Node::conditional_swap(item_i, item_j, Node::CTeq(cmp, dir));
}

void ObliviousOperations::setNode(int index, Node* node) {
    //        size_t readSize;
    //        unsigned long long plaintext_size = sizeof (Node);
    //    
    //        char* tmp = new char[storeSingleBlockSize];
    //    
    //        std::array<byte_t, sizeof (Node) > data;
    //    
    //        const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*node)));
    //        const byte_t* end = begin + sizeof (Node);
    //        std::copy(begin, end, std::begin(data));
    //    
    //        block buffer(data.begin(), data.end());
    //    block ciphertext = AES::Encrypt(key, buffer, single_block_clen_size, single_block_plaintext_size);
    //        std::memcpy(tmp, ciphertext.data(), storeSingleBlockSize);
    //        delete node;
    //    
    //        ocall_write_rawRamStore(index, (const char*) tmp, storeSingleBlockSize);
    //        delete tmp;
}

Node* ObliviousOperations::getNode(int index) {
    //            char* tmp = new char[storeSingleBlockSize];
    //            size_t readSize;
    //            ocall_read_rawRamStore(&readSize, index, tmp, storeSingleBlockSize);
    //            block ciphertext(tmp, tmp + storeSingleBlockSize);
    //    block buffer = AES::Decrypt(key, ciphertext, single_block_clen_size);
    //        
    //            Node* node = new Node();
    //            std::array<byte_t, sizeof (Node) > arr;
    //            std::copy(buffer.begin(), buffer.begin() + sizeof (Node), arr.begin());
    //            from_bytes(arr, *node);
    //            delete tmp;
    //            return node;


    if ((index >= currentBatchBegin1 && index < currentBatchBegin1 + BATCH_SIZE) && (setupCache1.size() != 0)) {
        return setupCache1[index - currentBatchBegin1];
    } else if ((index >= currentBatchBegin2 && index < currentBatchBegin2 + BATCH_SIZE) && (setupCache2.size() != 0)) {
        return setupCache2[index - currentBatchBegin2];
    } else {
        int pageBegin = (index / BATCH_SIZE) * BATCH_SIZE;
        if (isLeftBatchUpdated == false) {
            isLeftBatchUpdated = true;
            fetchBatch1(pageBegin);
            return setupCache1[index - currentBatchBegin1];
        } else {
            isLeftBatchUpdated = false;
            fetchBatch2(pageBegin);
            return setupCache2[index - currentBatchBegin2];
        }
    }
}

void ObliviousOperations::flushCache() {
    size_t readSize;

    char* tmp = new char[setupCache1.size() * storeSingleBlockSize];
    vector<long long> indexes;
    for (int j = 0; j < setupCache1.size(); j++) {
        indexes.push_back(currentBatchBegin1 + j);

        std::array<byte_t, sizeof (Node) > data;

        const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*(setupCache1[j]))));
        const byte_t* end = begin + sizeof (Node);
        std::copy(begin, end, std::begin(data));

        block buffer(data.begin(), data.end());
        block ciphertext = AES::Encrypt(key, buffer, single_block_clen_size, single_block_plaintext_size);
        std::memcpy(tmp + j * ciphertext.size(), ciphertext.data(), storeSingleBlockSize);
        delete setupCache1[j];
    }

    if (setupCache1.size() != 0) {
        ocall_nwrite_rawRamStore(setupCache1.size(), indexes.data(), (const char*) tmp, storeSingleBlockSize * setupCache1.size());
    }

    setupCache1.clear();
    delete tmp;
    indexes.clear();

    tmp = new char[setupCache2.size() * storeSingleBlockSize];
    for (int j = 0; j < setupCache2.size(); j++) {
        indexes.push_back(currentBatchBegin2 + j);

        std::array<byte_t, sizeof (Node) > data;

        const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*(setupCache2[j]))));
        const byte_t* end = begin + sizeof (Node);
        std::copy(begin, end, std::begin(data));

        block buffer(data.begin(), data.end());
        block ciphertext = AES::Encrypt(key, buffer, single_block_clen_size, single_block_plaintext_size);
        std::memcpy(tmp + j * ciphertext.size(), ciphertext.data(), storeSingleBlockSize);
        delete setupCache2[j];
    }

    if (setupCache2.size() != 0) {
        ocall_nwrite_rawRamStore(setupCache2.size(), indexes.data(), (const char*) tmp, storeSingleBlockSize * setupCache2.size());
    }

    setupCache2.clear();
    delete tmp;
}

void ObliviousOperations::fetchBatch1(int beginIndex) {
    size_t readSize;

    char* tmp = new char[setupCache1.size() * storeSingleBlockSize];
    vector<long long> indexes;
    for (int j = 0; j < setupCache1.size(); j++) {
        indexes.push_back(currentBatchBegin1 + j);

        std::array<byte_t, sizeof (Node) > data;

        const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*(setupCache1[j]))));
        const byte_t* end = begin + sizeof (Node);
        std::copy(begin, end, std::begin(data));

        block buffer(data.begin(), data.end());
        block ciphertext = AES::Encrypt(key, buffer, single_block_clen_size, single_block_plaintext_size);
        std::memcpy(tmp + j * ciphertext.size(), ciphertext.data(), storeSingleBlockSize);
        delete setupCache1[j];
    }

    if (setupCache1.size() != 0) {
        ocall_nwrite_rawRamStore(setupCache1.size(), indexes.data(), (const char*) tmp, storeSingleBlockSize * setupCache1.size());
    }
    delete tmp;

    setupCache1.clear();


    tmp = new char[BATCH_SIZE * storeSingleBlockSize];
    ocall_nread_rawRamStore(&readSize, BATCH_SIZE, beginIndex, tmp, BATCH_SIZE * storeSingleBlockSize);
    for (unsigned int i = 0; i < min((unsigned long long) BATCH_SIZE, totalNumberOfNodes - beginIndex); i++) {
        block ciphertext(tmp + i*storeSingleBlockSize, tmp + (i + 1) * storeSingleBlockSize);
        block buffer = AES::Decrypt(key, ciphertext, single_block_clen_size);

        Node* node = new Node();
        std::array<byte_t, sizeof (Node) > arr;
        std::copy(buffer.begin(), buffer.begin() + sizeof (Node), arr.begin());
        from_bytes(arr, *node);
        setupCache1.push_back(node);
    }
    currentBatchBegin1 = beginIndex;
    delete tmp;
}

void ObliviousOperations::fetchBatch2(int beginIndex) {
    size_t readSize;

    char* tmp = new char[setupCache2.size() * storeSingleBlockSize];
    vector<long long> indexes;
    for (int j = 0; j < setupCache2.size(); j++) {
        indexes.push_back(currentBatchBegin2 + j);

        std::array<byte_t, sizeof (Node) > data;

        const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*(setupCache2[j]))));
        const byte_t* end = begin + sizeof (Node);
        std::copy(begin, end, std::begin(data));

        block buffer(data.begin(), data.end());
        block ciphertext = AES::Encrypt(key, buffer, single_block_clen_size, single_block_plaintext_size);
        std::memcpy(tmp + j * ciphertext.size(), ciphertext.data(), storeSingleBlockSize);
        delete setupCache2[j];
    }

    if (setupCache2.size() != 0) {
        ocall_nwrite_rawRamStore(setupCache2.size(), indexes.data(), (const char*) tmp, storeSingleBlockSize * setupCache2.size());
    }
    delete tmp;

    setupCache2.clear();


    tmp = new char[BATCH_SIZE * storeSingleBlockSize];
    ocall_nread_rawRamStore(&readSize, BATCH_SIZE, beginIndex, tmp, BATCH_SIZE * storeSingleBlockSize);
    for (unsigned int i = 0; i < min((unsigned long long) BATCH_SIZE, totalNumberOfNodes - beginIndex); i++) {
        block ciphertext(tmp + i*storeSingleBlockSize, tmp + (i + 1) * storeSingleBlockSize);
        block buffer = AES::Decrypt(key, ciphertext, single_block_clen_size);

        Node* node = new Node();
        std::array<byte_t, sizeof (Node) > arr;
        std::copy(buffer.begin(), buffer.begin() + sizeof (Node), arr.begin());
        from_bytes(arr, *node);
        setupCache2.push_back(node);
    }
    currentBatchBegin2 = beginIndex;
    delete tmp;
}