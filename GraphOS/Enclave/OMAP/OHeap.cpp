#include "OHeap.h"
#include "Enclave.h"

OHeap::OHeap(OMAP* omapInstance, int maxSize) {
    size = 0;
    this->omap = omapInstance;
    this->maxSize = maxSize;
}

OHeap::~OHeap() {
}

string OHeap::readOMAP(string omapKey) {
    std::array< uint8_t, ID_SIZE > keyArray;
    keyArray.fill(0);
    std::copy(omapKey.begin(), omapKey.end(), std::begin(keyArray));
    Bid inputBid(keyArray);
    string value = omap->find(inputBid);
    return value;
}

void OHeap::writeOMAP(string omapKey, string omapValue) {
    std::array< uint8_t, ID_SIZE > keyArray;
    keyArray.fill(0);
    std::copy(omapKey.begin(), omapKey.end(), std::begin(keyArray));
    Bid inputBid(keyArray);
    omap->insert(inputBid, omapValue);
}

string OHeap::readWriteOMAP(string omapKey, string omapValue) {
    std::array< uint8_t, ID_SIZE > keyArray;
    keyArray.fill(0);
    std::copy(omapKey.begin(), omapKey.end(), std::begin(keyArray));
    Bid inputBid(keyArray);
    return omap->searchInsert(inputBid, omapValue);
}

vector<string> OHeap::splitData(const string& str, const string& delim) {
    vector<string> tokens = {"", ""};
    int pos = 0;
    for (int i = 0; i < str.length(); i++) {
        bool cond = Node::CTeq(str.at(i), '-');
        pos = Node::conditional_select(i, pos, cond);
    }
    string token = str.substr(0, pos);
    tokens[0] = token;
    int begin = Node::conditional_select(pos, pos + 1, Node::CTeq(Node::CTcmp(pos + 1, str.length()), 1));
    token = str.substr(begin, str.length());
    tokens[1] = token;
    return tokens;
    //    vector<string> tokens;
    //    size_t prev = 0, pos = 0;
    //    do {
    //        pos = str.find(delim, prev);
    //        if (pos == string::npos) pos = str.length();
    //        string token = str.substr(prev, pos - prev);
    //        if (!token.empty()) tokens.push_back(token);
    //        prev = pos + delim.length();
    //    } while (pos < str.length() && prev < str.length());
    //    return tokens;
}

void OHeap::setNewMinHeapNode(int newMinHeapNodeV, int dist) {
    omap->treeHandler->oram->shutdownEvictBucket = true;
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

        swapMinHeapNode("&" + to_string(i), to_string(newMinHeapNodeV) + "-" + to_string(dist), "&" + to_string(((i - 1) / 2)), arrTmp);

        i = ((i - 1) / 2);
        arrTmp = readOMAP("&" + to_string(((i - 1) / 2)));

        auto tmpparts = splitData(arrTmp, "-");
        tmpdist = stoi(tmpparts[1]);
        tmpV = stoi(tmpparts[0]);
    }

    omap->treeHandler->oram->shutdownEvictBucket = false;
    omap->treeHandler->oram->EvictBuckets();
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

        printf("swapping %d with %d\n", smallest, idx);
        swapMinHeapNode("&" + to_string(smallest), fsmall, "&" + to_string(idx), fidxNode);

        minHeapify(smallest);
    }
}

void OHeap::dummyOperation() {
    //    omap->treeHandler->startOperation();
    readOMAP("&0");
    //    omap->treeHandler->finishOperation();
}

void OHeap::extractMinID(int& id, int& dist) {
    if (size == 0) {
        dummyOperation();
        return;
    }

    omap->treeHandler->oram->shutdownEvictBucket = true;

    //    string root = readOMAP("&0");
    //    int rootV = stoi(splitData(root, "-")[0]);
    //
    //    string lastNode = readOMAP("&" + to_string(size - 1));
    //    int lastNodeV = stoi(splitData(lastNode, "-")[0]);
    //
    //    writeOMAP("&0", lastNode);

    string lastNode = readOMAP("&" + to_string(size - 1));
    int lastNodeV = stoi(splitData(lastNode, "-")[0]);


    string root = readWriteOMAP("&0", lastNode);
    int rootV = stoi(splitData(root, "-")[0]);



    --size;
    minHeapify(0);

    omap->treeHandler->oram->shutdownEvictBucket = false;
    omap->treeHandler->oram->EvictBuckets();

    id = rootV;
    dist = stoi(splitData(root, "-")[1]);
}

/**
 * Execute extract-min, insert, and dummy
 * @param id
 * @param dist
 * @param OP:1 extract-min  2:insert    3: dummy
 */
void OHeap::execute2(int& id, int& dist, int op) {
    if ((size == 0 && op == 1) || op == 3) {
        omap->treeHandler->oram->shutdownEvictBucket = true;
        readOMAP("&0");
        omap->treeHandler->oram->shutdownEvictBucket = false;
        omap->treeHandler->oram->EvictBuckets();

    } else if (op == 1) {
        omap->treeHandler->oram->shutdownEvictBucket = true;



        string root = readOMAP("&0");
        id = stoi(splitData(root, "-")[0]);
        dist = stoi(splitData(root, "-")[1]);

        string lastNode = readOMAP("&" + to_string(size - 1));
        int lastNodeV = stoi(splitData(lastNode, "-")[0]);

        writeOMAP("&0", lastNode);
        //        writeOMAP("=" + to_string(lastNodeV), "0");
        --size;


        minHeapify2(0);

        omap->treeHandler->oram->shutdownEvictBucket = false;
        omap->treeHandler->oram->EvictBuckets();

    } else if (op == 2) {
        omap->treeHandler->oram->shutdownEvictBucket = true;

        size++;
        int i = size - 1;

        writeOMAP("&" + to_string(i), to_string(id) + "-" + to_string(dist));




        for (int j = 0; j <= log2(maxSize); j++) {
            string arrTmp = readOMAP("&" + to_string(((i - 1) / 2)));
            vector<string> tmpparts = splitData(arrTmp, "-");
            int tmpdist = stoi(tmpparts[1]);

            int neighborId = (i % 2 == 0) ? i - 1 : i + 1;
            string dummy = readOMAP("&" + to_string(neighborId));

            if (dist < tmpdist) {
                writeOMAP("&" + to_string(i), arrTmp);
                writeOMAP("&" + to_string(((i - 1) / 2)), to_string(id) + "-" + to_string(dist));
            } else {
                writeOMAP("=", "=");
                writeOMAP("=", "=");
            }
            i = ((i - 1) / 2);

        }


        //        string arrTmp = readOMAP("&" + to_string(((i - 1) / 2)));
        //
        //        int tmpdist, tmpV = 0;
        //
        //        auto tmpparts = splitData(arrTmp, "-");
        //        tmpdist = stoi(tmpparts[1]);
        //        tmpV = stoi(tmpparts[0]);
        //
        //        while (i && dist < tmpdist) {
        //            swapMinHeapNode("&" + to_string(i), to_string(id) + "-" + to_string(dist), "&" + to_string(((i - 1) / 2)), arrTmp);
        //
        //            i = ((i - 1) / 2);
        //            arrTmp = readOMAP("&" + to_string(((i - 1) / 2)));
        //
        //            auto tmpparts = splitData(arrTmp, "-");
        //            tmpdist = stoi(tmpparts[1]);
        //            tmpV = stoi(tmpparts[0]);
        //        }

        omap->treeHandler->oram->shutdownEvictBucket = false;
        omap->treeHandler->oram->EvictBuckets();
    }
}

void OHeap::minHeapify2(int idx) {
    int curIndex = 0, leftIndex = 1, rightIndex = 2;
    string curNode = readOMAP("&" + to_string(curIndex));
    int curDist = stoi(splitData(curNode, "-")[1]);
    string arrLeft, arrRight;
    int leftDist, rightDist;


    for (int j = 0; j <= log2(maxSize); j++) {
        if (leftIndex < size) {
            arrLeft = readOMAP("&" + to_string(leftIndex));
            leftDist = stoi(splitData(arrLeft, "-")[1]);
        } else {
            readOMAP("=");
        }
        if (rightIndex < size) {
            arrRight = readOMAP("&" + to_string(rightIndex));
            rightDist = stoi(splitData(arrRight, "-")[1]);
        } else {
            readOMAP("=");
        }

        //        printf("leftIndex:%d rightIndex:%d curIndex:%d size:%d leftDist:%d rightDist:%d curDist:%d\n",leftIndex,rightIndex,curIndex,size,leftDist,rightDist,curDist);
        if (leftIndex < size && leftDist <= rightDist && leftDist < curDist) {
            //            printf("log1\n");
            //left should be swpped
            printf("swapping %d with %d\n", leftIndex, curIndex);
            writeOMAP("&" + to_string(leftIndex), curNode);
            writeOMAP("&" + to_string(curIndex), arrLeft);
            //            curNode = arrLeft;
            curIndex = leftIndex;
        } else if (rightIndex < size && leftDist > rightDist && rightDist < curDist) {
            //swap with right
            //            printf("log2\n");
            printf("swapping %d with %d\n", leftIndex, curIndex);
            writeOMAP("&" + to_string(rightIndex), curNode);
            writeOMAP("&" + to_string(curIndex), arrRight);
            //            curNode = arrRight;
            curIndex = rightIndex;
        } else {
            //dummy swap
            writeOMAP("=", "=");
            writeOMAP("=", "=");
            //            curNode = curNode;
            curIndex = curIndex;
        }

        //curIndex = left or right
        leftIndex = 2 * curIndex + 1;
        rightIndex = 2 * curIndex + 2;
    }

    //--------------------------------------------------------------------------------------- 



    //    int smallest, left, right;
    //    smallest = idx;
    //    left = 2 * idx + 1;
    //    right = 2 * idx + 2;
    //
    //    string arrLeft, arrSmallest, arrRight;
    //    int leftDist, smallestDist = -1, rightDist;
    //
    //    if (left < size) {
    //        arrLeft = readOMAP("&" + to_string(left));
    //        leftDist = stoi(splitData(arrLeft, "-")[1]);
    //        arrSmallest = readOMAP("&" + to_string(smallest));
    //        smallestDist = stoi(splitData(arrSmallest, "-")[1]);
    //
    //        if (leftDist < smallestDist) {
    //            smallest = left;
    //        }
    //    }
    //    if (right < size) {
    //        if (smallestDist == -1) {
    //            arrSmallest = readOMAP("&" + to_string(smallest));
    //            smallestDist = stoi(splitData(arrSmallest, "-")[1]);
    //        }
    //        arrRight = readOMAP("&" + to_string(right));
    //        rightDist = stoi(splitData(arrRight, "-")[1]);
    //
    //        if (rightDist < smallestDist) {
    //            smallest = right;
    //        }
    //    }
    //    if (smallest != idx) {
    //        string fsmall = readOMAP("&" + to_string(smallest));
    //        string fidxNode = readOMAP("&" + to_string(idx));
    //
    //        swapMinHeapNode("&" + to_string(smallest), fsmall, "&" + to_string(idx), fidxNode);
    //        printf("swapping %d with %d\n",smallest,idx);
    //
    //        minHeapify2(smallest);
    //    }
}

/**
 * Execute extract-min, insert, and dummy
 * @param id
 * @param dist
 * @param OP:1 extract-min  2:insert    3: dummy
 */
void OHeap::execute(int& id, int& dist, int op) {
    op = Node::conditional_select(3, op, Node::CTeq(size, 0) && Node::CTeq(op, 1));

    int i = size;
    size = Node::conditional_select(size - 1, size, Node::CTeq(op, 1));
    i = Node::conditional_select(i - 1, i, Node::CTeq(op, 1));
    size = Node::conditional_select(size + 1, size, Node::CTeq(op, 2));


    string lastNode = readOMAP("&" + to_string(i));

    string omapKey = "=";
    omapKey = CTString("&0", omapKey, Node::CTeq(op, 1));
    omapKey = CTString("&" + to_string(i), omapKey, Node::CTeq(op, 2));

    string omapValue = "=";
    omapValue = CTString(lastNode, omapValue, Node::CTeq(op, 1));
    omapValue = CTString(to_string(id) + "-" + to_string(dist), omapValue, Node::CTeq(op, 2));

    string root = readWriteOMAP(omapKey, omapValue);
    root = CTString("0-0", root, !Node::CTeq(op, 1));
    int firstPart = stoi(splitData(root, "-")[0]);
    int secondPart = stoi(splitData(root, "-")[1]);
    id = Node::conditional_select(firstPart, id, Node::CTeq(op, 1));
    dist = Node::conditional_select(secondPart, dist, Node::CTeq(op, 1));

    int curIndex = 0, leftIndex = 1, rightIndex = 2;
    string curNode = "0-0";
    curNode = CTString(lastNode, curNode, Node::CTeq(op, 1));
    int curDist = stoi(splitData(curNode, "-")[1]);
    string arrLeft, arrRight;
    int leftDist, rightDist;
    for (int j = 0; j < log2(maxSize); j++) {
        string omapKey = "=";
        omapKey = CTString("&" + to_string(leftIndex), omapKey, Node::CTeq(op, 1) && Node::CTeq(Node::CTcmp(leftIndex, size), -1));
        omapKey = CTString("&" + to_string(((i - 1) / 2)), omapKey, Node::CTeq(op, 2));

        string tmp = readOMAP(omapKey);
        string arrTmp = "0-0";
        arrLeft = "0-0";

        arrLeft = CTString(tmp, arrLeft, Node::CTeq(op, 1) && Node::CTeq(Node::CTcmp(leftIndex, size), -1));
        leftDist = stoi(splitData(arrLeft, "-")[1]);

        arrTmp = CTString(tmp, arrTmp, Node::CTeq(op, 2));
        vector<string> tmpparts = splitData(arrTmp, "-");

        int tmpdist = stoi(tmpparts[1]);

        omapKey = "=";
        omapKey = CTString("&" + to_string(rightIndex), omapKey, Node::CTeq(op, 1) && Node::CTeq(Node::CTcmp(rightIndex, size), -1));

        tmp = readOMAP(omapKey);
        arrRight = "0-0";

        arrRight = CTString(tmp, arrRight, Node::CTeq(op, 1) && Node::CTeq(Node::CTcmp(rightIndex, size), -1));
        rightDist = stoi(splitData(arrRight, "-")[1]);

        bool cond1 = Node::CTeq(op, 1) && (Node::CTeq(-1, Node::CTcmp(leftIndex, size)) && Node::CTeq(-1, Node::CTcmp(leftDist, rightDist)) && Node::CTeq(-1, Node::CTcmp(leftDist, curDist)));
        bool cond2 = Node::CTeq(op, 1) && (Node::CTeq(-1, Node::CTcmp(rightIndex, size)) && !Node::CTeq(-1, Node::CTcmp(leftDist, rightDist)) && Node::CTeq(-1, Node::CTcmp(rightDist, curDist)));
        bool cond3 = Node::CTeq(op, 2) && Node::CTeq(-1, Node::CTcmp(dist, tmpdist));

        omapKey = "=";
        omapKey = CTString("&" + to_string(leftIndex), omapKey, cond1);
        omapKey = CTString("&" + to_string(rightIndex), omapKey, !cond1 && cond2);
        omapKey = CTString("&" + to_string(i), omapKey, !cond1 && !cond2 && cond3);

        string omapValue = "=";
        omapValue = CTString(curNode, omapValue, cond1 || cond2);
        omapValue = CTString(arrTmp, omapValue, cond3);

        readWriteOMAP(omapKey, omapValue);

        omapKey = "=";
        omapKey = CTString("&" + to_string(curIndex), omapKey, cond1 || cond2);
        omapKey = CTString("&" + to_string(((i - 1) / 2)), omapKey, cond3);

        omapValue = "=";
        omapValue = CTString(arrLeft, omapValue, cond1);
        omapValue = CTString(arrRight, omapValue, !cond1 && cond2);
        omapValue = CTString(to_string(id) + "-" + to_string(dist), omapValue, cond3);

        readWriteOMAP(omapKey, omapValue);

        curIndex = Node::conditional_select(leftIndex, curIndex, cond1);
        curIndex = Node::conditional_select(rightIndex, curIndex, !cond1 && cond2);

        leftIndex = 2 * curIndex + 1;
        rightIndex = 2 * curIndex + 2;
        i = ((i - 1) / 2);
    }
}