#include "OHeap.h"

OHeap::OHeap(vector<OMAP*> omapInstances) {
    size = 0;
    this->omaps = omapInstances;
    this->partitions = (int) omaps.size();
}

OHeap::~OHeap() {
}

string OHeap::readOMAP(string omapKey) {
    std::array< uint8_t, ID_SIZE > keyArray;
    keyArray.fill(0);
    std::copy(omapKey.begin(), omapKey.end(), std::begin(keyArray));
    Bid inputBid(keyArray);
    int index = 0;
    char* value = new char[16];
    unsigned char tmp[SGX_AESGCM_IV_SIZE + SGX_AESGCM_MAC_SIZE + ID_SIZE];
    AES::PRF(omapKey, (int) omapKey.length(), tmp);
    index = tmp[0] % partitions;
    string result = omaps[index]->find(inputBid);
    delete value;
    modifiedOMAPs[index] = true;
    return result;
}

void OHeap::writeOMAP(string omapKey, string omapValue) {
    std::array< uint8_t, ID_SIZE > keyArray;
    keyArray.fill(0);
    std::copy(omapKey.begin(), omapKey.end(), std::begin(keyArray));
    Bid inputBid(keyArray);
    int index = 0;
    unsigned char tmp[SGX_AESGCM_IV_SIZE + SGX_AESGCM_MAC_SIZE + ID_SIZE];
    AES::PRF(omapKey, (int) omapKey.length(), tmp);
    index = tmp[0] % partitions;
    omaps[index]->insert(inputBid, omapValue);
    modifiedOMAPs[index] = true;
}

string OHeap::readAndSetDistInOMAP(string omapKey, string omapValue) {
    std::array< uint8_t, ID_SIZE > keyArray;
    keyArray.fill(0);
    std::copy(omapKey.begin(), omapKey.end(), std::begin(keyArray));
    Bid inputBid(keyArray);
    int index = 0;
    unsigned char tmp[SGX_AESGCM_IV_SIZE + SGX_AESGCM_MAC_SIZE + ID_SIZE];
    AES::PRF(omapKey, (int) omapKey.length(), tmp);
    index = tmp[0] % partitions;
    string result = omaps[index]->readAndSetDist(inputBid, omapValue);
    modifiedOMAPs[index] = true;
    return result;
}

void OHeap::setNewMinHeapNode(int newMinHeapNodeV, int dist) {
    modifiedOMAPs.clear();
    for (auto omap : omaps) {
        //        omap->treeHandler->startOperation();
        omap->treeHandler->oram->shutdownEvictBucket = true;
    }
    size++;

    int i = size - 1;

    writeOMAP("&" + to_string(i), to_string(newMinHeapNodeV) + "-" + to_string(dist));

    string arrTmp = readOMAP("&" + to_string(((i - 1) / 2)));

    int tmpdist, tmpV = 0;

    if (arrTmp != "") {
        auto tmpparts = splitData(arrTmp, "-");
        tmpdist = stoi(tmpparts[1]);
        tmpV = stoi(tmpparts[0]);
    }

    while (i && dist < tmpdist) {
        writeOMAP("=" + to_string(newMinHeapNodeV), to_string(((i - 1) / 2)));
        writeOMAP("=" + to_string(tmpV), to_string(i));

        swapMinHeapNode("&" + to_string(i), to_string(newMinHeapNodeV) + "-" + to_string(dist), "&" + to_string(((i - 1) / 2)), arrTmp);

        i = ((i - 1) / 2);
        arrTmp = readOMAP("&" + to_string(((i - 1) / 2)));

        auto tmpparts = splitData(arrTmp, "-");
        tmpdist = stoi(tmpparts[1]);
        tmpV = stoi(tmpparts[0]);
    }


    for (auto item : modifiedOMAPs) {
        omaps[item.first]->treeHandler->oram->shutdownEvictBucket = false;
        omaps[item.first]->treeHandler->oram->EvictBuckets();
        //        omaps[item.first]->treeHandler->finishOperation();
    }
}

vector<string> OHeap::splitData(const string& str, const string& delim) {
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

void OHeap::swapMinHeapNode(string a, string aValue, string b, string bValue) {
    writeOMAP(a, bValue);
    writeOMAP(b, aValue);
}

void OHeap::minHeapify(int idx) {
    int smallest, left, right;
    smallest = idx;
    left = 2 * idx + 1;
    right = 2 * idx + 2;

    string arrLeft, arrSmallest, arrRight;
    int leftDist, smallestDist = -1, rightDist;

    if (left < size) {
        arrLeft = readOMAP("&" + to_string(left));
        leftDist = stoi(splitData(arrLeft, "-")[1]);
        arrSmallest = readOMAP("&" + to_string(smallest));
        smallestDist = stoi(splitData(arrSmallest, "-")[1]);

        if (leftDist < smallestDist) {
            smallest = left;
        }
    }
    if (right < size) {
        if (smallestDist == -1) {
            arrSmallest = readOMAP("&" + to_string(smallest));
            smallestDist = stoi(splitData(arrSmallest, "-")[1]);
        }
        arrRight = readOMAP("&" + to_string(right));
        rightDist = stoi(splitData(arrRight, "-")[1]);

        if (rightDist < smallestDist) {
            smallest = right;
        }
    }
    if (smallest != idx) {
        string fsmall = readOMAP("&" + to_string(smallest));
        string fidxNode = readOMAP("&" + to_string(idx));

        int fsmallV = stoi(splitData(fsmall, "-")[0]);
        int fidxNodeV = stoi(splitData(fidxNode, "-")[0]);

        writeOMAP("=" + to_string(fsmallV), to_string(idx));
        writeOMAP("=" + to_string(fidxNodeV), to_string(smallest));

        swapMinHeapNode("&" + to_string(smallest), fsmall, "&" + to_string(idx), fidxNode);

        minHeapify(smallest);
    }
}

bool OHeap::isEmpty() {
    return size == 0;
}

void OHeap::dummyOperation() {
    modifiedOMAPs.clear();
    //    for (auto omap : omaps) {
    //        omap->treeHandler->startOperation();
    //    }
    readOMAP("&0");
    //    for (auto item : modifiedOMAPs) {
    //        omaps[item.first]->treeHandler->finishOperation();
    //    }
}

void OHeap::extractMinID(int& id, int& dist) {
    if (isEmpty()) {
        dummyOperation();
        return;
    }

    modifiedOMAPs.clear();
    for (auto omap : omaps) {
        //        omap->treeHandler->startOperation();
        omap->treeHandler->oram->shutdownEvictBucket = true;
    }


    string root = readOMAP("&0");
    int rootV = stoi(splitData(root, "-")[0]);

    string lastNode = readOMAP("&" + to_string(size - 1));
    int lastNodeV = stoi(splitData(lastNode, "-")[0]);

    writeOMAP("&0", lastNode);

    writeOMAP("=" + to_string(rootV), to_string(size - 1));
    writeOMAP("=" + to_string(lastNodeV), "0");

    --size;
    minHeapify(0);
    for (auto item : modifiedOMAPs) {
        omaps[item.first]->treeHandler->oram->shutdownEvictBucket = false;
        omaps[item.first]->treeHandler->oram->EvictBuckets();
        //        omaps[item.first]->treeHandler->finishOperation();
    }
    id = rootV;
    dist = stoi(splitData(root, "-")[1]);
}

void OHeap::decreaseKey(int v, int dist) {
    modifiedOMAPs.clear();
    for (auto omap : omaps) {
        //        omap->treeHandler->startOperation();
        omap->treeHandler->oram->shutdownEvictBucket = true;
    }
    int i = stoi(readOMAP("=" + to_string(v)));

    string arrI = readAndSetDistInOMAP("&" + to_string(i), to_string(dist));
    auto parts = splitData(arrI, "-");
    int arrIV = stoi(parts[0]);

    string arrTmp = readOMAP("&" + to_string(((i - 1) / 2)));

    auto tmpparts = splitData(arrTmp, "-");
    int tmpdist = stoi(tmpparts[1]);
    int tmpV = stoi(tmpparts[0]);

    while (i && dist < tmpdist) {
        writeOMAP("=" + to_string(arrIV), to_string(((i - 1) / 2)));
        writeOMAP("=" + to_string(tmpV), to_string(i));

        swapMinHeapNode("&" + to_string(i), parts[0] + "-" + to_string(dist), "&" + to_string(((i - 1) / 2)), arrTmp);

        i = ((i - 1) / 2);
        arrTmp = readOMAP("&" + to_string(((i - 1) / 2)));

        tmpparts = splitData(arrTmp, "-");
        tmpdist = stoi(tmpparts[1]);
        tmpV = stoi(tmpparts[0]);
    }
    for (auto item : modifiedOMAPs) {
        omaps[item.first]->treeHandler->oram->shutdownEvictBucket = false;
        omaps[item.first]->treeHandler->oram->EvictBuckets();
        //        omaps[item.first]->treeHandler->finishOperation();
    }
}

bool OHeap::isInMinHeap(int v) {
    modifiedOMAPs.clear();
    //    for (auto omap : omaps) {
    //        omap->treeHandler->startOperation();
    //    }
    int pos = stoi(readOMAP("=" + to_string(v)));
    //    for (auto item : modifiedOMAPs) {
    //        omaps[item.first]->treeHandler->finishOperation();
    //    }
    if (pos < size)
        return true;
    return false;
} 