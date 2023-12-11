#include "OMAP.h"
#include "Enclave.h"
#include "Enclave_t.h"
using namespace std;

OMAP::OMAP(int maxSize, bytes<Key> secretKey) {
    treeHandler = new AVLTree(maxSize, secretKey,true);
    rootKey = 0;
}

OMAP::OMAP(int maxSize, bytes<Key> secretKey, map<Bid, string>* pairs, map<unsigned long long, unsigned long long>* permutation) {
    treeHandler = new AVLTree(maxSize, secretKey, rootKey, rootPos, pairs, permutation);
}

OMAP::OMAP(int maxSize, Bid rootBid,long long rootPos,bytes<Key> secretKey){
    treeHandler = new AVLTree(maxSize, secretKey,false);
    this->rootKey = rootBid;
    this->rootPos = rootPos;
}

OMAP::~OMAP() {

}

string OMAP::find(Bid omapKey) {
    double y;
    if (treeHandler->logTime) {
        ocall_start_timer(950);
    }
    if (rootKey == 0) {
        return "";
    }
    treeHandler->startOperation(false);
    Node* node = new Node();
    node->key = rootKey;
    node->pos = rootPos;
    string res = treeHandler->search(node, omapKey);
    rootPos = node->pos;
    delete node;
    if (treeHandler->logTime) {
        ocall_stop_timer(&y, 950);
        treeHandler->times[2].push_back(y);
        ocall_start_timer(950);
    }
    treeHandler->finishOperation();
    if (treeHandler->logTime) {
        ocall_stop_timer(&y, 950);
        treeHandler->times[3].push_back(y);
    }
    return res;
}

void OMAP::insert(Bid omapKey, string value) {
    if (treeHandler->logTime) {
        treeHandler->times[0].push_back(0);
    }
    treeHandler->totheight = 0;
    int height;
    treeHandler->startOperation(false);
    if (rootKey == 0) {
        rootKey = treeHandler->insert(0, rootPos, omapKey, value, height, omapKey, false);
    } else {
        rootKey = treeHandler->insert(rootKey, rootPos, omapKey, value, height, omapKey, false);
    }
    double y;
    if (treeHandler->logTime) {
        ocall_start_timer(898);
    }
    treeHandler->finishOperation();
    if (treeHandler->logTime) {
        ocall_stop_timer(&y, 898);
        treeHandler->times[1].push_back(y);
    }
}

void OMAP::printTree() {
    Node* node = new Node();
    node->key = rootKey;
    node->pos = rootPos;
    treeHandler->printTree(node, 0);
}

/**
 * This function is used for batch insert which is used at the end of setup phase.
 */
void OMAP::batchInsert(map<Bid, string> pairs) {
    treeHandler->startOperation(true);
    int cnt = 0, height;
    for (auto pair : pairs) {
        cnt++;
        if (rootKey == 0) {
            rootKey = treeHandler->insert(0, rootPos, pair.first, pair.second, height, 0, false);
        } else {
            rootKey = treeHandler->insert(rootKey, rootPos, pair.first, pair.second, height, 0, false);
        }
    }
    treeHandler->finishOperation();
}

/**
 * This function is used for batch search which is used in the real search procedure
 */
vector<string> OMAP::batchSearch(vector<Bid> keys) {
    vector<string> result;
    treeHandler->startOperation(false);
    Node* node = new Node();
    node->key = rootKey;
    node->pos = rootPos;

    vector<Node*> resNodes;
    treeHandler->batchSearch(node, keys, &resNodes);
    for (Node* n : resNodes) {
        string res;
        if (n != NULL) {
            res.assign(n->value.begin(), n->value.end());
            result.push_back(res);
        } else {
            result.push_back("");
        }
    }
    treeHandler->finishOperation();
    return result;
}
