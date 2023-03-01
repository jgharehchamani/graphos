#include "ObliviousMinHeap.h"

ObliviousMinHeap::ObliviousMinHeap(int V) {
    size = V;
}

ObliviousMinHeap::~ObliviousMinHeap() {
}

string ObliviousMinHeap::readOMAP(string omapKey) {
    std::array< uint8_t, ID_SIZE > keyArray;
    keyArray.fill(0);
    std::copy(omapKey.begin(), omapKey.end(), std::begin(keyArray));

    char* value = new char[16];
    ecall_read_node((const char*) keyArray.data(), value, 0);
    string result(value);
    delete value;
    return result;
}

void ObliviousMinHeap::writeOMAP(string omapKey, string omapValue) {
    std::array< uint8_t, ID_SIZE > keyArray;
    keyArray.fill(0);
    std::copy(omapKey.begin(), omapKey.end(), std::begin(keyArray));

    std::array< uint8_t, 16 > valueArray;
    valueArray.fill(0);
    std::copy(omapValue.begin(), omapValue.end(), std::begin(valueArray));

    ecall_write_node((const char*) keyArray.data(), (const char*) valueArray.data(), 0);
}

string ObliviousMinHeap::readAndSetDistInOMAP(string omapKey, string omapValue) {
    std::array< uint8_t, ID_SIZE > keyArray;
    keyArray.fill(0);
    std::copy(omapKey.begin(), omapKey.end(), std::begin(keyArray));

    std::array< uint8_t, 16 > valueArray;
    valueArray.fill(0);
    std::copy(omapValue.begin(), omapValue.end(), std::begin(valueArray));

    char* value = new char[16];
    ecall_read_and_set_dist_node((const char*) keyArray.data(), (const char*) valueArray.data(), value, 0);

    string result(value);
    delete value;
    return result;
}

void ObliviousMinHeap::setNewMinHeapNode(int arrayIndex, int newMinHeapNodeV, int newMinHeapNodeDist) {
    writeOMAP("&" + to_string(arrayIndex), to_string(newMinHeapNodeV) + "-" + to_string(newMinHeapNodeDist));
    writeOMAP("=" + to_string(arrayIndex), to_string(arrayIndex));
}

vector<string> ObliviousMinHeap::splitData(const string& str, const string& delim) {
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

void ObliviousMinHeap::swapMinHeapNode(string a, string aValue, string b, string bValue) {
    writeOMAP(a, bValue);
    writeOMAP(b, aValue);
}

void ObliviousMinHeap::minHeapify(int idx) {
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

int ObliviousMinHeap::isEmpty() {
    return size == 0;
}

int ObliviousMinHeap::extractMinID() {
    if (isEmpty())
        return NULL;

    string root = readOMAP("&0");
    int rootV = stoi(splitData(root, "-")[0]);

    string lastNode = readOMAP("&" + to_string(size - 1));
    int lastNodeV = stoi(splitData(lastNode, "-")[0]);

    writeOMAP("&0", lastNode);

    writeOMAP("=" + to_string(rootV), to_string(size - 1));
    writeOMAP("=" + to_string(lastNodeV), "0");

    --size;
    minHeapify(0);

    return rootV;
}

void ObliviousMinHeap::decreaseKey(int v, int dist) {
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
}

bool ObliviousMinHeap::isInMinHeap(int v) {
    int pos = stoi(readOMAP("=" + to_string(v)));
    if (pos < size)
        return true;
    return false;
} 