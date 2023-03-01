#include "AVLTree.h"
#include "Enclave.h"

AVLTree::AVLTree(int maxSize, bytes<Key> secretkey, int instance) {
    oram = new ORAM(maxSize, secretkey, instance);
    int depth = (int) (ceil(log2(maxSize)) - 1) + 1;
    maxOfRandom = (long long) (pow(2, depth));
}

AVLTree::~AVLTree() {
    delete oram;
}

// A utility function to get maximum of two integers

int AVLTree::max(int a, int b) {
    int res = CTcmp(a, b);
    return CTeq(res, 1) ? a : b;
}

Node* AVLTree::newNode(Bid omapKey, string value) {
    Node* node = new Node();
    node->key = omapKey;
    node->index = index++;
    std::fill(node->value.begin(), node->value.end(), 0);
    std::copy(value.begin(), value.end(), node->value.begin());
    node->leftID = 0;
    node->leftPos = 0;
    node->rightPos = 0;
    node->rightID = 0;
    node->pos = 0;
    node->isDummy = false;
    node->height = 1; // new node is initially added at leaf
    return node;
}

// A utility function to right rotate subtree rooted with leftNode
// See the diagram given above.

void AVLTree::rotate(Node* node, Node* oppositeNode, int targetHeight, bool right, bool dummyOp) {
    Node* T2 = nullptr;
    Node* tmpDummyNode = new Node();
    tmpDummyNode->isDummy = true;
    T2 = newNode(0, "");
    Node* tmp = nullptr;
    bool left = !right;
    unsigned long long newPos = RandomPath();
    unsigned long long dumyPos;
    Bid dummy;
    dummy.setValue(oram->nextDummyCounter++);

    bool cond1 = !dummyOp && ((right && oppositeNode->rightID.isZero()) || (left && oppositeNode->leftID.isZero()));
    bool cond2 = !dummyOp && right;
    bool cond3 = !dummyOp && left;


    T2->height = Node::conditional_select(0, T2->height, cond1);

    Bid readKey = dummy;
    readKey = Bid::conditional_select(oppositeNode->rightID, readKey, !cond1 && cond2);
    readKey = Bid::conditional_select(oppositeNode->leftID, readKey, !cond1 && !cond2 && cond3);

    unsigned long long readPos = newPos;
    readPos = Node::conditional_select(oppositeNode->rightPos, readPos, !cond1 && cond2);
    readPos = Node::conditional_select(oppositeNode->leftPos, readPos, !cond1 && !cond2 && cond3);
    bool isDummy = !(!cond1 && (cond2 || cond3));

    tmp = readWriteCacheNode(readKey, tmpDummyNode, true, isDummy); //READ

    Node::conditional_assign(T2, tmp, !cond1 && (cond2 || cond3));
    delete tmp;

    // Perform rotation
    cond1 = !dummyOp && right;
    cond2 = !dummyOp;
    oppositeNode->rightID = Bid::conditional_select(node->key, oppositeNode->rightID, cond1);
    oppositeNode->rightPos = Bid::conditional_select(node->pos, oppositeNode->rightPos, cond1);
    node->leftID = Bid::conditional_select(T2->key, node->leftID, cond1);
    node->leftPos = Bid::conditional_select(T2->pos, node->leftPos, cond1);

    oppositeNode->leftID = Bid::conditional_select(node->key, oppositeNode->leftID, !cond1 && cond2);
    oppositeNode->leftPos = Bid::conditional_select(node->pos, oppositeNode->leftPos, !cond1 && cond2);
    node->rightID = Bid::conditional_select(T2->key, node->rightID, !cond1 && cond2);
    node->rightPos = Bid::conditional_select(T2->pos, node->rightPos, !cond1 && cond2);


    // Update heights    
    int curNodeHeight = max(T2->height, targetHeight) + 1;
    node->height = Node::conditional_select(curNodeHeight, node->height, !dummyOp);

    Bid writeKey = dummy;
    writeKey = Bid::conditional_select(node->key, writeKey, !dummyOp);

    Node* writeNodeData = Node::clone(tmpDummyNode);
    Node::conditional_assign(writeNodeData, node, !dummyOp);

    delete writeNodeData;


    int oppositeOppositeHeight = 0;
    unsigned long long newOppositePos = RandomPath();
    Node* oppositeOppositeNode = nullptr;

    cond1 = !dummyOp && right && !oppositeNode->leftID.isZero();
    cond2 = !dummyOp && left && !oppositeNode->rightID.isZero();

    readKey = dummy;
    readKey = Bid::conditional_select(oppositeNode->leftID, readKey, cond1);
    readKey = Bid::conditional_select(oppositeNode->rightID, readKey, !cond1 && cond2);

    readPos = newOppositePos;
    readPos = Node::conditional_select(oppositeNode->leftPos, readPos, cond1);
    readPos = Node::conditional_select(oppositeNode->rightPos, readPos, !cond1 && cond2);
    isDummy = !(cond1 || cond2);

    oppositeOppositeNode = readWriteCacheNode(readKey, tmpDummyNode, true, isDummy); //READ

    oppositeOppositeHeight = Node::conditional_select(oppositeOppositeNode->height, oppositeOppositeHeight, cond1 || cond2);
    delete oppositeOppositeNode;


    int maxValue = max(oppositeOppositeHeight, curNodeHeight) + 1;
    oppositeNode->height = Node::conditional_select(maxValue, oppositeNode->height, !dummyOp);

    delete T2;
    delete tmpDummyNode;
}

//void AVLTree::rotate(Node* node, Node* oppositeNode, int targetHeight, bool right, bool dummyOp) {
//    Node* T2 = nullptr;
//    T2 = newNode(0, "");
//    Node* tmp = nullptr;
//    bool left = !right;
//    Node* tmpDummyNode = new Node();
//    tmpDummyNode->isDummy = true;
//    unsigned long long newPos = RandomPath();
//    Bid dummy;
//    unsigned long long dumyPos;
//    dummy.setValue(oram->nextDummyCounter++);
//    if (!dummyOp && ((right && oppositeNode->rightID == 0) || (left && oppositeNode->leftID == 0))) {
//        tmp = tmp;
//        T2->height = 0;
//        delete tmp;
//        oram->ReadWrite(dummy, tmpDummyNode, newPos, newPos, true, true);
//        //                oram->ReadNode(dummy, newPos, newPos, true);
//    } else if (!dummyOp && right) {
//        tmp = T2;
//        T2->height = T2->height;
//        delete tmp;
//        T2 = oram->ReadWrite(oppositeNode->rightID, tmpDummyNode, oppositeNode->rightPos, newPos, true,  false);
//        //        T2 = oram->ReadNode(oppositeNode->rightID, oppositeNode->rightPos, newPos);
//    } else if (!dummyOp && left) {
//        tmp = T2;
//        T2->height = T2->height;
//        delete tmp;
//        T2 = oram->ReadWrite(oppositeNode->leftID, tmpDummyNode, oppositeNode->leftPos, newPos, true,  false);
//        //        T2 = oram->ReadNode(oppositeNode->leftID, oppositeNode->leftPos, newPos);
//    } else {
//        tmp = tmp;
//        T2->height = T2->height;
//        delete tmp;
//        oram->ReadWrite(dummy, tmpDummyNode, newPos, newPos, true, true);
//        //        oram->ReadNode(dummy, newPos, newPos, true);
//    }
//    // Perform rotation
//    if (!dummyOp && right) {
//        oppositeNode->rightID = node->key;
//        oppositeNode->rightPos = node->pos;
//        oppositeNode->leftID = oppositeNode->leftID;
//        oppositeNode->leftPos = oppositeNode->leftPos;
//        node->rightID = node->rightID;
//        node->rightPos = node->rightPos;
//        node->leftID = T2->key;
//        node->leftPos = T2->pos;
//    } else if (!dummyOp) {
//        oppositeNode->leftID = node->key;
//        oppositeNode->leftPos = node->pos;
//        oppositeNode->rightID = oppositeNode->rightID;
//        oppositeNode->rightPos = oppositeNode->rightPos;
//        node->rightID = T2->key;
//        node->rightPos = T2->pos;
//        node->leftID = node->leftID;
//        node->leftPos = node->leftPos;
//    } else {
//        oppositeNode->leftID = oppositeNode->leftID;
//        oppositeNode->leftPos = oppositeNode->leftPos;
//        oppositeNode->rightID = oppositeNode->rightID;
//        oppositeNode->rightPos = oppositeNode->rightPos;
//        node->rightID = node->rightID;
//        node->rightPos = node->rightPos;
//        node->leftID = node->leftID;
//        node->leftPos = node->leftPos;
//    }
//    // Update heights    
//    int curNodeHeight = max(T2->height, targetHeight) + 1;
//    if (!dummyOp) {
//        node->height = curNodeHeight;
//        oram->ReadWrite(node->key, node, node->pos, node->pos, false,  false);
//        //        oram->WriteNode(node->key, node);
//    } else {
//        node->height = node->height;
//        oram->ReadWrite(dummy, node, node->pos, node->pos, false, true);
//        //        oram->WriteNode(dummy, node, oram->evictBuckets, true);
//    }
//    int oppositeOppositeHeight = 0;
//    unsigned long long newOppositePos = RandomPath();
//    Node* oppositeOppositeNode = nullptr;
//    if (!dummyOp && right && oppositeNode->leftID != 0) {
//        oppositeOppositeNode = oram->ReadWrite(oppositeNode->leftID, tmpDummyNode, oppositeNode->leftPos, newOppositePos, true, false);
//        //        oppositeOppositeNode = oram->ReadNode(oppositeNode->leftID, oppositeNode->leftPos, newOppositePos);
//        oppositeNode->leftPos = newOppositePos;
//        oppositeOppositeHeight = oppositeOppositeNode->height;
//        delete oppositeOppositeNode;
//    } else if (!dummyOp && left && oppositeNode->rightID != 0) {
//        oppositeOppositeNode = oram->ReadWrite(oppositeNode->rightID, tmpDummyNode, oppositeNode->rightPos, newOppositePos, true, false);
//        //        oppositeOppositeNode = oram->ReadNode(oppositeNode->rightID, oppositeNode->rightPos, newOppositePos);
//        oppositeNode->rightPos = newOppositePos;
//        oppositeOppositeHeight = oppositeOppositeNode->height;
//        delete oppositeOppositeNode;
//    } else {
//        oppositeOppositeNode = oram->ReadWrite(dummy, tmpDummyNode, newOppositePos, newOppositePos, true, true);
//        //        oppositeOppositeNode = oram->ReadNode(dummy, newOppositePos, newOppositePos, true);
//        oppositeNode->rightPos = oppositeNode->rightPos;
//        oppositeOppositeHeight = oppositeOppositeHeight;
//        delete oppositeOppositeNode;
//    }
//    int maxValue = max(oppositeOppositeHeight, curNodeHeight) + 1;
//    if (!dummyOp) {
//        oppositeNode->height = maxValue;
//    } else {
//        oppositeNode->height = oppositeNode->height;
//    }
//    delete T2;
//}

//Bid AVLTree::insert(Bid rootKey, unsigned long long& rootPos, Bid omapKey, string value, int& height, Bid lastID, bool isDummyIns) {
//    totheight++;
//    if (isDummyIns && CTeq(CTcmp(totheight, oram->depth * 1.44), 1)) {
//        Node* nnode = newNode(omapKey, value);
//
//        nnode->pos = RandomPath();
//
//        height = Node::conditional_select(nnode->height, height, !exist);
//        rootPos = Node::conditional_select(nnode->pos, rootPos, !exist);
//
//        Bid dummy;
//        dummy.setValue(oram->nextDummyCounter++);
//        Bid initWriteKey = dummy;
//        initWriteKey = Bid::conditional_select(omapKey, initWriteKey, !exist);
//
//        Node* wrtNode = new Node();
//        wrtNode->isDummy = true;
//        Node::conditional_assign(wrtNode, nnode, !exist);
//
//        Node* tmp = oram->ReadWrite(initWriteKey, wrtNode, nnode->pos, nnode->pos, false, exist);
//
//        Bid retKey = lastID;
//        retKey = Bid::conditional_select(nnode->key, retKey, !exist);
//        delete tmp;
//        delete wrtNode;
//        return retKey;
//    }
//    /* 1. Perform the normal BST rotation */
//    double t;
//    bool cond1, cond2, cond3, cond4, cond5, isDummy;
//    unsigned long long rndPos = RandomPath();
//    Node* tmpDummyNode = new Node();
//    Node* tmp;
//    Node* wrtNode;
//    tmpDummyNode->isDummy = true;
//    Bid dummy;
//    Bid retKey;
//    bool remainerIsDummy = false;
//    unsigned long long dumyPos;
//    dummy.setValue(oram->nextDummyCounter++);
//    //    Node* nnode = newNode(omapKey, value);
//
//
//    cond1 = !isDummyIns && !rootKey.isZero();
//    //    cond2 = !isDummyIns;
//    //    nnode->pos = RandomPath();
//    //    height = Node::conditional_select(nnode->height, height, cond1);
//    //    rootPos = Node::conditional_select(nnode->pos, rootPos, cond1);
//    //    Bid initWriteKey = dummy;
//    //    initWriteKey = Bid::conditional_select(omapKey, initWriteKey, cond1);
//    //    wrtNode = Node::clone(tmpDummyNode);
//    //    Node::conditional_assign(wrtNode, nnode, cond1);
//    //    unsigned long long initWritePos = rndPos;
//    //    initWritePos = Node::conditional_select(nnode->pos, initWritePos, cond1);
//    //    isDummy = !cond1;
//    //    tmp = oram->ReadWrite(initWriteKey, wrtNode, initWritePos, nnode->pos, false, isDummy);
//    //    delete tmp;
//    //    delete wrtNode;
//    //    retKey = Bid::conditional_select(nnode->key, retKey, cond1);
//    remainerIsDummy = !cond1;
//
//    unsigned long long newPos = RandomPath();
//    Node* node = nullptr;
//
//    Bid initReadKey = dummy;
//    initReadKey = Bid::conditional_select(rootKey, initReadKey, !remainerIsDummy);
//    isDummy = remainerIsDummy;
//    node = oram->ReadWrite(initReadKey, tmpDummyNode, rootPos, newPos, true, isDummy);
//
//
//    int balance = -1;
//    int leftHeight = -1;
//    int rightHeight = -1;
//    Node* leftNode = new Node();
//    bool leftNodeisNull = true;
//    Node* rightNode = new Node();
//    bool rightNodeisNull = true;
//    std::array< byte_t, 16> garbage;
//
//    unsigned long long newRLPos = RandomPath();
//
//
//    cond1 = !remainerIsDummy;
//    int cmpRes = Bid::CTcmp(omapKey, node->key);
//    cond2 = Node::CTeq(cmpRes, -1);
//    bool cond2_1 = node->rightID.isZero();
//    cond3 = Node::CTeq(cmpRes, 1);
//    bool cond3_1 = node->leftID.isZero();
//    cond4 = Node::CTeq(cmpRes, 0);
//
//    Bid insRootKey = rootKey;
//    insRootKey = Bid::conditional_select(node->leftID, insRootKey, cond1 && cond2);
//    insRootKey = Bid::conditional_select(node->rightID, insRootKey, cond1 && cond3);
//
//    unsigned long long insRootPos = rootPos;
//    insRootPos = Node::conditional_select(node->leftPos, insRootPos, cond1 && cond2);
//    insRootPos = Node::conditional_select(node->rightPos, insRootPos, cond1 && cond3);
//
//    int insHeight = height;
//    insHeight = Node::conditional_select(leftHeight, insHeight, cond1 && cond2);
//    insHeight = Node::conditional_select(rightHeight, insHeight, cond1 && cond3);
//
//    Bid insLastID = dummy;
//    insLastID = Bid::conditional_select(node->key, insLastID, (cond1 && !cond2 && !cond3) || (!cond1));
//
//    isDummy = !cond1 || (cond1 && !cond2 && !cond3);
//
//    exist = Node::conditional_select(true, exist, cond1 && cond4);
//    Bid resValBid = insert(insRootKey, insRootPos, omapKey, value, insHeight, insLastID, isDummy);
//
//
//    node->leftID = Bid::conditional_select(resValBid, node->leftID, cond1 && cond2);
//    node->rightID = Bid::conditional_select(resValBid, node->rightID, cond1 && cond3);
//
//    node->leftPos = Node::conditional_select(insRootPos, node->leftPos, cond1 && cond2);
//    node->rightPos = Node::conditional_select(insRootPos, node->rightPos, cond1 && cond3);
//    rootPos = Node::conditional_select(insRootPos, rootPos, !cond1 || (cond1 && !cond2 && !cond3));
//
//    leftHeight = Node::conditional_select(insHeight, leftHeight, cond1 && cond2);
//    rightHeight = Node::conditional_select(insHeight, rightHeight, cond1 && cond3);
//    height = Node::conditional_select(insHeight, height, (cond1 && !cond2 && !cond3) || (!cond1));
//
//    totheight--;
//
//
//    Bid readKey = dummy;
//    readKey = Bid::conditional_select(node->rightID, readKey, cond1 && cond2 && !cond2_1);
//    readKey = Bid::conditional_select(node->leftID, readKey, cond1 && cond3 && !cond3_1);
//
//    unsigned long long tmpreadPos = newRLPos;
//    tmpreadPos = Bid::conditional_select(node->rightPos, tmpreadPos, cond1 && cond2 && !cond2_1);
//    tmpreadPos = Bid::conditional_select(node->leftPos, tmpreadPos, cond1 && cond3 && !cond3_1);
//
//    isDummy = !((cond1 && cond2 && !cond2_1) || (cond1 && cond3 && !cond3_1));
//
//    tmp = oram->ReadWrite(readKey, tmpDummyNode, tmpreadPos, newRLPos, true, isDummy);
//    Node::conditional_assign(rightNode, tmp, cond1 && cond2 && !cond2_1);
//    Node::conditional_assign(leftNode, tmp, cond1 && cond3 && !cond3_1);
//
//    delete tmp;
//
//    rightNodeisNull = Node::conditional_select(false, rightNodeisNull, cond1 && cond2 && !cond2_1);
//    leftNodeisNull = Node::conditional_select(false, leftNodeisNull, cond1 && cond3 && !cond3_1);
//
//    rightHeight = Node::conditional_select(0, rightHeight, cond1 && cond2 && cond2_1);
//    rightHeight = Node::conditional_select(rightNode->height, rightHeight, cond1 && cond2 && !cond2_1);
//    leftHeight = Node::conditional_select(0, leftHeight, cond1 && cond3 && cond3_1);
//    leftHeight = Node::conditional_select(leftNode->height, leftHeight, cond1 && cond3 && !cond3_1);
//
//    node->rightPos = Node::conditional_select(newRLPos, node->rightPos, cond1 && cond2 && !cond2_1);
//    node->leftPos = Node::conditional_select(newRLPos, node->leftPos, cond1 && cond3 && !cond3_1);
//
//    std::fill(garbage.begin(), garbage.end(), 0);
//    std::copy(value.begin(), value.end(), garbage.begin());
//
//    for (int i = 0; i < 16; i++) {
//        node->value[i] = Bid::conditional_select(garbage[i], node->value[i], cond1 && !cond2 && !cond3);
//    }
//
//    rootPos = Node::conditional_select(node->pos, rootPos, cond1 && !cond2 && !cond3);
//    height = Node::conditional_select(node->height, height, cond1 && !cond2 && !cond3);
//
//    retKey = Bid::conditional_select(node->key, retKey, cond1 && !cond2 && !cond3);
//    retKey = Bid::conditional_select(resValBid, retKey, !cond1);
//
//    Bid writeKey = dummy;
//    writeKey = Bid::conditional_select(rootKey, writeKey, cond1 && !cond2 && !cond3);
//
//    wrtNode = Node::clone(tmpDummyNode);
//    Node::conditional_assign(wrtNode, node, cond1 && !cond2 && !cond3);
//
//    unsigned long long tmpwritePos = dumyPos;
//    tmpwritePos = Bid::conditional_select(node->pos, tmpwritePos, cond1 && !cond2 && !cond3);
//
//    isDummy = !(cond1 && !cond2 && !cond3);
//
//    tmp = oram->ReadWrite(writeKey, wrtNode, tmpwritePos, node->pos, false, isDummy);
//    delete wrtNode;
//    delete tmp;
//    remainerIsDummy = Node::conditional_select(true, remainerIsDummy, cond1 && !cond2 && !cond3);
//    remainerIsDummy = Node::conditional_select(false, remainerIsDummy, cond1 && (cond2 || cond3));
//
//    /* 2. Update height of this ancestor node */
//
//    int tmpHeight = max(leftHeight, rightHeight) + 1;
//    node->height = Node::conditional_select(node->height, tmpHeight, remainerIsDummy);
//    height = Node::conditional_select(height, node->height, remainerIsDummy);
//
//    /* 3. Get the balance factor of this ancestor node to check whether this node became unbalanced */
//
//    balance = Node::conditional_select(balance, leftHeight - rightHeight, remainerIsDummy);
//
//    ocall_start_timer(totheight + 100000);
//    //------------------------------------------------
//    // Write Left and Right Nodes
//    //------------------------------------------------
//
//    //    cond1 = !remainerIsDummy && !CTeq(CTcmp(balance, 1), 1) && !CTeq(CTcmp(balance, -1), -1);
//    //
//    //    Bid updateWriteKey = dummy;
//    //    updateWriteKey = Bid::conditional_select(leftNode->key, updateWriteKey, cond1 && !leftNodeisNull);
//    //
//    //    wrtNode = Node::clone(tmpDummyNode);
//    //    Node::conditional_assign(wrtNode, leftNode, cond1 && !leftNodeisNull);
//    //
//    //    unsigned long long wrtPos = dumyPos;
//    //    wrtPos = Node::conditional_select(leftNode->pos, wrtPos, cond1 && !leftNodeisNull);
//    //
//    //    isDummy = !(cond1 && !leftNodeisNull);
//    //
//    //    tmp = oram->ReadWrite(updateWriteKey, wrtNode, wrtPos, wrtPos, false, isDummy);
//    //    delete tmp;
//    //    delete wrtNode;
//    //
//    //
//    //    updateWriteKey = dummy;
//    //    updateWriteKey = Bid::conditional_select(rightNode->key, updateWriteKey, cond1 && !rightNodeisNull);
//    //
//    //    wrtNode = Node::clone(tmpDummyNode);
//    //    Node::conditional_assign(wrtNode, rightNode, cond1 && !rightNodeisNull);
//    //
//    //    wrtPos = dumyPos;
//    //    wrtPos = Node::conditional_select(rightNode->pos, wrtPos, cond1 && !rightNodeisNull);
//    //
//    //    isDummy = !(cond1 && !rightNodeisNull);
//    //
//    //    tmp = oram->ReadWrite(updateWriteKey, wrtNode, wrtPos, wrtPos, false, isDummy);
//    //    delete tmp;
//    //    delete wrtNode;
//
//
//
//    cond1 = !remainerIsDummy && CTeq(CTcmp(balance, 1), 1) && Node::CTeq(Bid::CTcmp(omapKey, node->leftID), -1);
//    cond2 = !remainerIsDummy && CTeq(CTcmp(balance, -1), -1) && Node::CTeq(Bid::CTcmp(omapKey, node->rightID), 1);
//    cond3 = !remainerIsDummy && CTeq(CTcmp(balance, 1), 1) && Node::CTeq(Bid::CTcmp(omapKey, node->leftID), 1);
//    cond4 = !remainerIsDummy && CTeq(CTcmp(balance, -1), -1) && Node::CTeq(Bid::CTcmp(omapKey, node->rightID), -1);
//    cond5 = !remainerIsDummy;
//
//
//    //------------------------------------------------
//    // Left/Right Node
//    //------------------------------------------------
//
//    unsigned long long newLeftRightPos = RandomPath();
//
//    Bid leftRightReadKey = dummy;
//    leftRightReadKey = Bid::conditional_select(node->leftID, leftRightReadKey, (cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull);
//    leftRightReadKey = Bid::conditional_select(node->rightID, leftRightReadKey, ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull);
//
//    unsigned long long leftRightReadPos = dumyPos;
//    leftRightReadPos = Node::conditional_select(node->leftPos, leftRightReadPos, (cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull);
//    leftRightReadPos = Node::conditional_select(node->rightPos, leftRightReadPos, ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull);
//
//    unsigned long long newleftRightReadPos = dumyPos;
//    newleftRightReadPos = Node::conditional_select(newLeftRightPos, newleftRightReadPos, (cond1 || cond2 || cond3 || cond4));
//
//    isDummy = !(((cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull) || (((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull));
//
//    tmp = oram->ReadWrite(leftRightReadKey, tmpDummyNode, leftRightReadPos, newleftRightReadPos, true, isDummy);
//    Node::conditional_assign(leftNode, tmp, (cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull);
//    Node::conditional_assign(rightNode, tmp, ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull);
//    delete tmp;
//
//    node->leftPos = Node::conditional_select(newleftRightReadPos, node->leftPos, (cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull);
//    leftNodeisNull = Node::conditional_select(false, leftNodeisNull, (cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull);
//
//    node->rightPos = Node::conditional_select(newleftRightReadPos, node->rightPos, ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull);
//    rightNodeisNull = Node::conditional_select(false, rightNodeisNull, ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull);
//    //    //------------------------------------------------
//    //    // Left Node
//    //    //------------------------------------------------
//    //
//    //    unsigned long long newLeftPos = RandomPath();
//    //
//    //    Bid leftNodeReadWriteKey = dummy;
//    //    leftNodeReadWriteKey = Bid::conditional_select(node->leftID, leftNodeReadWriteKey, (cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull);
//    //    leftNodeReadWriteKey = Bid::conditional_select(leftNode->key, leftNodeReadWriteKey, ((!cond1 && cond2) || (!cond1 && !cond2 && !cond3 && cond4)) && !leftNodeisNull);
//    //
//    //    unsigned long long leftNodeReadWritePos = dumyPos;
//    //    leftNodeReadWritePos = Node::conditional_select(node->leftPos, leftNodeReadWritePos, (cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull);
//    //    leftNodeReadWritePos = Node::conditional_select(leftNode->pos, leftNodeReadWritePos, ((!cond1 && cond2) || (!cond1 && !cond2 && !cond3 && cond4)) && !leftNodeisNull);
//    //
//    //    unsigned long long newleftNodeReadWritePos = dumyPos;
//    //    newleftNodeReadWritePos = Node::conditional_select(newLeftPos, newleftNodeReadWritePos, (cond1 || (!cond1 && !cond2 && cond3)));
//    //    newleftNodeReadWritePos = Node::conditional_select(leftNode->pos, newleftNodeReadWritePos, ((!cond1 && cond2) || (!cond1 && !cond2 && !cond3 && cond4)) && !leftNodeisNull);
//    //
//    //
//    //    isDummy = !(((cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull) || (((!cond1 && cond2) || (!cond1 && !cond2 && !cond3 && cond4)) && !leftNodeisNull));
//    //
//    //    bool isReadOp = (cond1 || (!cond1 && !cond2 && cond3));
//    //
//    //    Node* readwriteNode = Node::clone(tmpDummyNode);
//    //    Node::conditional_assign(readwriteNode, leftNode, ((!cond1 && cond2) || (!cond1 && !cond2 && !cond3 && cond4)) && !leftNodeisNull);
//    //
//    //    tmp = oram->ReadWrite(leftNodeReadWriteKey, readwriteNode, leftNodeReadWritePos, newleftNodeReadWritePos, isReadOp, isDummy);
//    //    Node::conditional_assign(leftNode, tmp, (cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull);
//    //    delete tmp;
//    //    node->leftPos = Node::conditional_select(newLeftPos, node->leftPos, (cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull);
//    //    leftNodeisNull = Node::conditional_select(false, leftNodeisNull, (cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull);
//    //
//    //    delete readwriteNode;
//    //
//    //
//    //
//    //    //------------------------------------------------
//    //    // Right Node
//    //    //------------------------------------------------
//    //    unsigned long long newRightPos = RandomPath();
//    //
//    //    Bid rightNodeReadWriteKey = dummy;
//    //    rightNodeReadWriteKey = Bid::conditional_select(node->rightID, rightNodeReadWriteKey, ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull);
//    //    rightNodeReadWriteKey = Bid::conditional_select(rightNode->key, rightNodeReadWriteKey, (cond1 || (!cond1 && !cond2 && cond3)) && !rightNodeisNull);
//    //
//    //    unsigned long long rightNodeReadWritePos = dumyPos;
//    //    rightNodeReadWritePos = Node::conditional_select(node->rightPos, rightNodeReadWritePos, ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull);
//    //    rightNodeReadWritePos = Node::conditional_select(rightNode->pos, rightNodeReadWritePos, (cond1 || (!cond1 && !cond2 && cond3)) && !rightNodeisNull);
//    //
//    //    unsigned long long newrightNodeReadWritePos = dumyPos;
//    //    newrightNodeReadWritePos = Node::conditional_select(newRightPos, newrightNodeReadWritePos, ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)));
//    //    newrightNodeReadWritePos = Node::conditional_select(rightNode->pos, newrightNodeReadWritePos, (cond1 || (!cond1 && !cond2 && cond3)) && !rightNodeisNull);
//    //
//    //    isDummy = !((((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull) || ((cond1 || (!cond1 && !cond2 && cond3)) && !rightNodeisNull));
//    //
//    //    isReadOp = ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4));
//    //
//    //
//    //    readwriteNode = Node::clone(tmpDummyNode);
//    //    Node::conditional_assign(readwriteNode, rightNode, (cond1 || (!cond1 && !cond2 && cond3)) && !rightNodeisNull);
//    //
//    //    tmp = oram->ReadWrite(rightNodeReadWriteKey, readwriteNode, rightNodeReadWritePos, newrightNodeReadWritePos, isReadOp, isDummy);
//    //    Node::conditional_assign(rightNode, tmp, ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull);
//    //    delete tmp;
//    //
//    //    node->rightPos = Node::conditional_select(newRightPos, node->rightPos, ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull);
//    //    rightNodeisNull = Node::conditional_select(false, rightNodeisNull, ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull);
//    //
//    //    delete readwriteNode;
//    //------------------------------------------------
//    // Left-Right Right-Left Node
//    //------------------------------------------------
//
//    Bid leftRightNodeReadKey = dummy;
//    leftRightNodeReadKey = Bid::conditional_select(leftNode->rightID, leftRightNodeReadKey, !cond1 && !cond2 && cond3);
//    leftRightNodeReadKey = Bid::conditional_select(rightNode->leftID, leftRightNodeReadKey, !cond1 && !cond2 && !cond3 && cond4);
//
//
//    unsigned long long leftRightNodeReadPos = dumyPos;
//    leftRightNodeReadPos = Node::conditional_select(leftNode->rightPos, leftRightNodeReadPos, !cond1 && !cond2 && cond3);
//    leftRightNodeReadPos = Node::conditional_select(rightNode->leftPos, leftRightNodeReadPos, !cond1 && !cond2 && !cond3 && cond4);
//
//
//    isDummy = !((!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4));
//
//    unsigned long long newLRPos = RandomPath();
//    tmp = oram->ReadWrite(leftRightNodeReadKey, tmpDummyNode, leftRightNodeReadPos, newLRPos, true, isDummy);
//
//
//    Node* leftRightNode = new Node();
//    Node* rightLeftNode = new Node();
//
//    Node::conditional_assign(leftRightNode, tmp, (!cond1 && !cond2 && cond3));
//    Node::conditional_assign(rightLeftNode, tmp, (!cond1 && !cond2 && !cond3 && cond4));
//
//
//    delete tmp;
//
//
//    leftNode->rightPos = Node::conditional_select(newLRPos, leftNode->rightPos, (!cond1 && !cond2 && cond3));
//    rightNode->leftPos = Node::conditional_select(newLRPos, rightNode->leftPos, (!cond1 && !cond2 && !cond3 && cond4));
//
//
//    int leftLeftHeight = 0;
//    int rightRightHeight = 0;
//
//    unsigned long long newleftLeftPos = RandomPath();
//
//
//
//
//    Bid leftLeftRightRightKey = dummy;
//    leftLeftRightRightKey = Bid::conditional_select(leftNode->leftID, leftLeftRightRightKey, (!cond1 && !cond2 && cond3) && !leftNode->leftID.isZero());
//    leftLeftRightRightKey = Bid::conditional_select(rightNode->rightID, leftLeftRightRightKey, (!cond1 && !cond2 && !cond3 && cond4) && !rightNode->rightID.isZero());
//
//    unsigned long long leftLeftRightRightPos = dumyPos;
//    leftLeftRightRightPos = Node::conditional_select(leftNode->leftPos, leftLeftRightRightPos, (!cond1 && !cond2 && cond3) && !leftNode->leftID.isZero());
//    leftLeftRightRightPos = Node::conditional_select(rightNode->rightPos, leftLeftRightRightPos, (!cond1 && !cond2 && !cond3 && cond4) && !rightNode->rightID.isZero());
//
//    isDummy = !(((!cond1 && !cond2 && cond3) && !leftNode->leftID.isZero()) || ((!cond1 && !cond2 && !cond3 && cond4) && !rightNode->rightID.isZero()));
//
//    tmp = oram->ReadWrite(leftLeftRightRightKey, tmpDummyNode, leftLeftRightRightPos, newleftLeftPos, true, isDummy);
//
//    Node* leftLeftNode = new Node();
//    Node* rightRightNode = new Node();
//
//    Node::conditional_assign(leftLeftNode, tmp, (!cond1 && !cond2 && cond3) && !leftNode->leftID.isZero());
//    Node::conditional_assign(rightRightNode, tmp, (!cond1 && !cond2 && !cond3 && cond4) && !rightNode->rightID.isZero());
//    delete tmp;
//
//    leftNode->leftPos = Node::conditional_select(newleftLeftPos, leftNode->leftPos, (!cond1 && !cond2 && cond3) && !leftNode->leftID.isZero());
//    rightNode->rightPos = Node::conditional_select(newleftLeftPos, rightNode->rightPos, (!cond1 && !cond2 && !cond3 && cond4) && !rightNode->rightID.isZero());
//
//
//    leftLeftHeight = Node::conditional_select(leftLeftNode->height, leftLeftHeight, (!cond1 && !cond2 && cond3) && !leftNode->leftID.isZero());
//    rightRightHeight = Node::conditional_select(rightRightNode->height, rightRightHeight, (!cond1 && !cond2 && !cond3 && cond4) && !rightNode->rightID.isZero());
//    delete leftLeftNode;
//    delete rightRightNode;
//
//    //------------------------------------------------
//    // Rotate
//    //------------------------------------------------
//
//    Node* targetRotateNode = Node::clone(tmpDummyNode);
//    Node* oppositeRotateNode = Node::clone(tmpDummyNode);
//
//    Node::conditional_assign(targetRotateNode, leftNode, !cond1 && !cond2 && cond3);
//    Node::conditional_assign(targetRotateNode, rightNode, !cond1 && !cond2 && !cond3 && cond4);
//
//    Node::conditional_assign(oppositeRotateNode, leftRightNode, !cond1 && !cond2 && cond3);
//    Node::conditional_assign(oppositeRotateNode, rightLeftNode, !cond1 && !cond2 && !cond3 && cond4);
//
//    int rotateHeight = 0;
//    rotateHeight = Node::conditional_select(leftLeftHeight, rotateHeight, !cond1 && !cond2 && cond3);
//    rotateHeight = Node::conditional_select(rightRightHeight, rotateHeight, !cond1 && !cond2 && !cond3 && cond4);
//
//    bool isRightRotate = !cond1 && !cond2 && !cond3 && cond4;
//    bool isDummyRotate = !((!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4));
//
//    rotate(targetRotateNode, oppositeRotateNode, rotateHeight, isRightRotate, isDummyRotate);
//
//    Node::conditional_assign(leftNode, targetRotateNode, !cond1 && !cond2 && cond3);
//    Node::conditional_assign(rightNode, targetRotateNode, !cond1 && !cond2 && !cond3 && cond4);
//
//    delete targetRotateNode;
//
//    Node::conditional_assign(leftRightNode, oppositeRotateNode, !cond1 && !cond2 && cond3);
//    Node::conditional_assign(rightLeftNode, oppositeRotateNode, !cond1 && !cond2 && !cond3 && cond4);
//
//    delete oppositeRotateNode;
//
//    node->leftID = Bid::conditional_select(leftRightNode->key, node->leftID, !cond1 && !cond2 && cond3);
//    node->leftPos = Node::conditional_select(leftRightNode->pos, node->leftPos, !cond1 && !cond2 && cond3);
//
//    node->rightID = Bid::conditional_select(rightLeftNode->key, node->rightID, !cond1 && !cond2 && !cond3 && cond4);
//    node->rightPos = Node::conditional_select(rightLeftNode->pos, node->rightPos, !cond1 && !cond2 && !cond3 && cond4);
//
//
//    //------------------------------------------------
//    // Second Rotate
//    //------------------------------------------------
//
//    targetRotateNode = Node::clone(tmpDummyNode);
//
//    Node::conditional_assign(targetRotateNode, node, cond1 || cond2 || cond3 || cond4);
//
//    oppositeRotateNode = Node::clone(tmpDummyNode);
//
//    Node::conditional_assign(oppositeRotateNode, leftNode, cond1);
//    Node::conditional_assign(oppositeRotateNode, rightNode, !cond1 && cond2);
//    Node::conditional_assign(oppositeRotateNode, leftRightNode, !cond1 && !cond2 && cond3);
//    Node::conditional_assign(oppositeRotateNode, rightLeftNode, !cond1 && !cond2 && !cond3 && cond4);
//
//    rotateHeight = 0;
//    rotateHeight = Node::conditional_select(rightHeight, rotateHeight, cond1 || (!cond1 && !cond2 && cond3));
//    rotateHeight = Node::conditional_select(leftHeight, rotateHeight, (!cond1 && cond2) || (!cond1 && !cond2 && !cond3 && cond4));
//
//    isRightRotate = cond1 || (!cond1 && !cond2 && cond3);
//    isDummyRotate = !(cond1 || cond2 || cond3 || cond4);
//
//    rotate(node, oppositeRotateNode, rotateHeight, isRightRotate, isDummyRotate);
//
//    Node::conditional_assign(node, targetRotateNode, cond1 || cond2 || cond3 || cond4);
//
//    delete targetRotateNode;
//
//    Node::conditional_assign(leftNode, oppositeRotateNode, cond1);
//    Node::conditional_assign(rightNode, oppositeRotateNode, !cond1 && cond2);
//    Node::conditional_assign(leftRightNode, oppositeRotateNode, !cond1 && !cond2 && cond3);
//    Node::conditional_assign(rightLeftNode, oppositeRotateNode, !cond1 && !cond2 && !cond3 && cond4);
//
//    delete oppositeRotateNode;
//
//    rootPos = Node::conditional_select(leftNode->pos, rootPos, cond1);
//    rootPos = Node::conditional_select(rightNode->pos, rootPos, !cond1 && cond2);
//    rootPos = Node::conditional_select(leftRightNode->pos, rootPos, !cond1 && !cond2 && cond3);
//    rootPos = Node::conditional_select(rightLeftNode->pos, rootPos, !cond1 && !cond2 && !cond3 && cond4);
//    rootPos = Node::conditional_select(node->pos, rootPos, !cond1 && !cond2 && !cond3 && !cond4 && cond5);
//
//
//    retKey = Bid::conditional_select(leftNode->key, retKey, cond1);
//    retKey = Bid::conditional_select(rightNode->key, retKey, !cond1 && cond2);
//    retKey = Bid::conditional_select(leftRightNode->key, retKey, !cond1 && !cond2 && cond3);
//    retKey = Bid::conditional_select(rightLeftNode->key, retKey, !cond1 && !cond2 && !cond3 && cond4);
//    retKey = Bid::conditional_select(node->key, retKey, !cond1 && !cond2 && !cond3 && !cond4 && cond5);
//
//    height = Node::conditional_select(leftNode->height, height, cond1);
//    height = Node::conditional_select(rightNode->height, height, !cond1 && cond2);
//    height = Node::conditional_select(leftRightNode->height, height, !cond1 && !cond2 && cond3);
//    height = Node::conditional_select(rightLeftNode->height, height, !cond1 && !cond2 && !cond3 && cond4);
//
//
//    Bid finalWriteKey = dummy;
//    finalWriteKey = Bid::conditional_select(leftNode->key, finalWriteKey, cond1);
//    finalWriteKey = Bid::conditional_select(rightNode->key, finalWriteKey, !cond1 && cond2);
//    finalWriteKey = Bid::conditional_select(leftRightNode->key, finalWriteKey, !cond1 && !cond2 && cond3);
//    finalWriteKey = Bid::conditional_select(rightLeftNode->key, finalWriteKey, !cond1 && !cond2 && !cond3 && cond4);
//    finalWriteKey = Bid::conditional_select(node->key, finalWriteKey, !cond1 && !cond2 && !cond3 && !cond4 && cond5);
//
//
//    unsigned long long finalWritePos = dumyPos;
//    finalWritePos = Node::conditional_select(leftNode->pos, finalWritePos, cond1);
//    finalWritePos = Node::conditional_select(rightNode->pos, finalWritePos, !cond1 && cond2);
//    finalWritePos = Node::conditional_select(leftRightNode->pos, finalWritePos, !cond1 && !cond2 && cond3);
//    finalWritePos = Node::conditional_select(rightLeftNode->pos, finalWritePos, !cond1 && !cond2 && !cond3 && cond4);
//    finalWritePos = Node::conditional_select(node->pos, finalWritePos, !cond1 && !cond2 && !cond3 && !cond4 && cond5);
//
//    isDummy = !(cond1 || cond2 || cond3 || cond4 || cond5);
//
//    Node* finalWriteNode = Node::clone(tmpDummyNode);
//    Node::conditional_assign(finalWriteNode, leftNode, cond1);
//    Node::conditional_assign(finalWriteNode, rightNode, !cond1 && cond2);
//    Node::conditional_assign(finalWriteNode, leftRightNode, !cond1 && !cond2 && cond3);
//    Node::conditional_assign(finalWriteNode, rightLeftNode, !cond1 && !cond2 && !cond3 && cond4);
//    Node::conditional_assign(finalWriteNode, node, !cond1 && !cond2 && !cond3 && !cond4 && cond5);
//
//    tmp = oram->ReadWrite(finalWriteKey, finalWriteNode, finalWritePos, finalWritePos, false, isDummy);
//    delete tmp;
//
//    delete tmpDummyNode;
//    return retKey;
//}

//Bid AVLTree::insert(Bid rootKey, unsigned long long& rootPos, Bid omapKey, string value, int& height, Bid lastID, bool isDummyIns) {
//    totheight++;
//    //    printf("tot hegiht:%d\n",totheight);
//    //totheight >= oram->depth
//    //    if (isDummyIns && totheight >= oram->depth) {
//    if (isDummyIns && !CTeq(CTcmp(totheight, oram->depth), -1)) {
//        Bid resKey = lastID;
//        if (!exist) {
//            Node* nnode = newNode(omapKey, value);
//            nnode->pos = RandomPath();
//            Bid keyRes = nnode->key;
//            height = nnode->height;
//            rootPos = nnode->pos;
//            oram->ReadWrite(omapKey, nnode, nnode->pos, nnode->pos, false, false);
//            resKey = nnode->key;
//            //            printf("add new node:key:%lld\n",omapKey.getValue());
//        } else {
//            //            printf("exist key:%lld reskey:%lld\n",omapKey.getValue(),resKey.getValue());
//        }
//        //        rootPos = oram->WriteNode(omapKey, nnode);
//        return resKey;
//    }
//    /* 1. Perform the normal BST rotation */
//    unsigned long long rndPos = RandomPath();
//    Node* tmpDummyNode = new Node();
//    tmpDummyNode->isDummy = true;
//    tmpDummyNode->pos = RandomPath();
//    Bid dummy;
//    Bid retKey;
//    unsigned long long dumyPos = RandomPath();
//    bool remainerIsDummy = false;
//    dummy.setValue(oram->nextDummyCounter++);
//    if (!isDummyIns && !rootKey.isZero()) {
//        remainerIsDummy = false;
//    } else {
//        remainerIsDummy = true;
//    }
//    //    Node* dummynew = nullptr;
//    //    Node* nnode = newNode(omapKey, value);
//    //    if (!isDummyIns && rootKey == 0) {
//    //        dummynew = dummynew;
//    //        nnode->pos = RandomPath();
//    //        Bid keyRes = nnode->key;
//    //        height = nnode->height;
//    //        rootPos = nnode->pos;
//    //        oram->ReadWrite(omapKey, nnode, nnode->pos, nnode->pos, false, false);
//    //        //        rootPos = oram->WriteNode(omapKey, nnode);
//    //        remainerIsDummy = true;
//    //        delete dummynew;
//    //        retKey = keyRes;
//    //    } else if (!isDummyIns) {
//    //        dummynew = nnode;
//    //        nnode->pos = RandomPath();
//    //        Bid keyRes = nnode->key;
//    //        height = height;
//    //        oram->ReadWrite(dummy, tmpDummyNode, rndPos, nnode->pos, false, true);
//    //        //        oram->WriteNode(omapKey, nnode, oram->evictBuckets, true);
//    //        remainerIsDummy = false;
//    //        delete dummynew;
//    //        retKey = retKey;
//    //    } else {
//    //        dummynew = nnode;
//    //        nnode->pos = RandomPath();
//    //        Bid keyRes = nnode->key;
//    //        height = height;
//    //        oram->ReadWrite(dummy, tmpDummyNode, rndPos, nnode->pos, false, true);
//    //        //        oram->WriteNode(omapKey, nnode, oram->evictBuckets, true);
//    //        remainerIsDummy = true;
//    //        delete dummynew;
//    //        retKey = retKey;
//    //    }
//    unsigned long long newPos = RandomPath();
//    Node* node = nullptr;
//    if (remainerIsDummy) {
//        node = oram->ReadWrite(dummy, tmpDummyNode, rootPos, newPos, true, true);
//        //        node = oram->ReadNode(dummy, rootPos, newPos, true);
//    } else {
//        node = oram->ReadWrite(rootKey, tmpDummyNode, rootPos, newPos, true, false);
//        //        node = oram->ReadNode(rootKey, rootPos, newPos, true);
//    }
//
//    int balance = -1;
//    int leftHeight = -1;
//    int rightHeight = -1;
//    Node* leftNode = new Node();
//    bool leftNodeisNull = true;
//    Node* rightNode = new Node();
//    bool rightNodeisNull = true;
//    std::array< byte_t, 16> garbage;
//
//    unsigned long long newRLPos = RandomPath();
//
//
//    if (!remainerIsDummy) {
//        if (omapKey < node->key) {
//            node->leftID = insert(node->leftID, node->leftPos, omapKey, value, leftHeight, dummy, false);
//
//            totheight--;
//            if (node->rightID == 0) {
//                oram->ReadWrite(dummy, tmpDummyNode, newRLPos, newRLPos, true, true);
//                //                oram->ReadNode(dummy, newRLPos, newRLPos, true);
//                rightHeight = 0;
//                node->rightPos = node->rightPos;
//                std::fill(garbage.begin(), garbage.end(), 0);
//                std::copy(value.begin(), value.end(), garbage.begin());
//                Bid keyRes = node->key;
//                rootPos = rootPos;
//                height = height;
//                oram->ReadWrite(dummy, tmpDummyNode, node->pos, node->pos, false, true);
//                //                oram->WriteNode(dummy, node, oram->evictBuckets, true);
//                retKey = retKey;
//                remainerIsDummy = false;
//            } else {
//                delete rightNode;
//                rightNode = oram->ReadWrite(node->rightID, tmpDummyNode, node->rightPos, newRLPos, true, false);
//                rightNodeisNull = false;
//                //                rightNode = oram->ReadNode(node->rightID, node->rightPos, newRLPos, true);
//                rightHeight = rightNode->height;
//                node->rightPos = newRLPos;
//                std::fill(garbage.begin(), garbage.end(), 0);
//                std::copy(value.begin(), value.end(), garbage.begin());
//                Bid keyRes = node->key;
//                rootPos = rootPos;
//                height = height;
//                oram->ReadWrite(dummy, tmpDummyNode, node->pos, node->pos, false, true);
//                //                oram->WriteNode(dummy, node, oram->evictBuckets, true);
//                retKey = retKey;
//                remainerIsDummy = false;
//            }
//        } else if (omapKey > node->key) {
//            node->rightID = insert(node->rightID, node->rightPos, omapKey, value, rightHeight, dummy, false);
//            totheight--;
//            if (node->leftID == 0) {
//                oram->ReadWrite(dummy, tmpDummyNode, newRLPos, newRLPos, true, true);
//                //                oram->ReadNode(dummy, newRLPos, newRLPos, true);
//                leftHeight = 0;
//                node->leftPos = node->leftPos;
//                std::fill(garbage.begin(), garbage.end(), 0);
//                std::copy(value.begin(), value.end(), garbage.begin());
//                Bid keyRes = node->key;
//                rootPos = rootPos;
//                height = height;
//                oram->ReadWrite(dummy, tmpDummyNode, node->pos, node->pos, false, true);
//                //                oram->WriteNode(dummy, node, oram->evictBuckets, true);
//                retKey = retKey;
//                remainerIsDummy = false;
//
//            } else {
//                delete leftNode;
//                leftNode = oram->ReadWrite(node->leftID, tmpDummyNode, node->leftPos, newRLPos, true, false);
//                leftNodeisNull = false;
//                //                leftNode = oram->ReadNode(node->leftID, node->leftPos, newRLPos, true);
//                leftHeight = leftNode->height;
//                node->leftPos = newRLPos;
//                std::fill(garbage.begin(), garbage.end(), 0);
//                std::copy(value.begin(), value.end(), garbage.begin());
//                Bid keyRes = node->key;
//                rootPos = rootPos;
//                height = height;
//                oram->ReadWrite(dummy, tmpDummyNode, node->pos, node->pos, false, true);
//                //                oram->WriteNode(dummy, node, oram->evictBuckets, true);
//                retKey = retKey;
//                remainerIsDummy = false;
//            }
//        } else {
//            exist = true;
//            Bid resValBid = insert(rootKey, rootPos, omapKey, value, height, node->key, true);
//            totheight--;
//            oram->ReadWrite(dummy, tmpDummyNode, newRLPos, newRLPos, true, true);
//            //            oram->ReadNode(dummy, newRLPos, newRLPos, true);
//            leftHeight = leftHeight;
//            node->leftPos = node->leftPos;
//            std::fill(node->value.begin(), node->value.end(), 0);
//            std::copy(value.begin(), value.end(), node->value.begin());
//            Bid keyRes = node->key;
//            rootPos = node->pos;
//            height = node->height;
//            oram->ReadWrite(rootKey, node, node->pos, node->pos, false, false);
//            //            oram->WriteNode(rootKey, node);
//            retKey = keyRes;
//            remainerIsDummy = true;
//        }
//    } else {
//        Bid resValBid = insert(rootKey, rootPos, omapKey, value, height, node->key, true);
//        totheight--;
//        oram->ReadWrite(dummy, tmpDummyNode, newRLPos, newRLPos, true, true);
//        //        oram->ReadNode(dummy, newRLPos, newRLPos, true);
//        leftHeight = leftHeight;
//        node->leftPos = node->leftPos;
//        std::fill(garbage.begin(), garbage.end(), 0);
//        std::copy(value.begin(), value.end(), garbage.begin());
//        Bid keyRes = node->key;
//        rootPos = rootPos;
//        height = height;
//        oram->ReadWrite(dummy, tmpDummyNode, node->pos, node->pos, false, true);
//        //        oram->WriteNode(dummy, node, oram->evictBuckets, true);
//        retKey = resValBid;
//        remainerIsDummy = remainerIsDummy;
//    }
//    /* 2. Update height of this ancestor node */
//    if (remainerIsDummy) {
//        max(leftHeight, rightHeight) + 1;
//        node->height = node->height;
//        height = height;
//        //        printf("read node key:%lld heught:%lld\n",node->key.getValue(),node->height);
//    } else {
//        node->height = max(leftHeight, rightHeight) + 1;
//        height = node->height;
//        //        printf("read node key:%lld heught:%lld\n",node->key.getValue(),node->height);
//    }
//    /* 3. Get the balance factor of this ancestor node to check whether this node became unbalanced */
//    if (remainerIsDummy) {
//        balance = balance;
//    } else {
//        balance = leftHeight - rightHeight;
//    }
//
//    //    printf("node:%lld:%lld:%lld:%lld:%lld:%lld:%lld\n", node->key.getValue(), node->height, leftNode->pos, node->leftID.getValue(), node->leftPos, node->rightID.getValue(), node->rightPos);
//    //balance <= 1 && balance >= -1
//    //    if (remainerIsDummy == false && balance <= 1 && balance >= -1) {
//    //    if (remainerIsDummy == false && !CTeq(CTcmp(balance, 1), 1) && !CTeq(CTcmp(balance, -1), -1)) {
//    //        if (leftNodeisNull == false) {
//    //            oram->ReadWrite(leftNode->key, leftNode, leftNode->pos, leftNode->pos, false, false);
//    //            //            oram->WriteNode(leftNode->key, leftNode);
//    //        } else {
//    //            oram->ReadWrite(dummy, tmpDummyNode, dumyPos, dumyPos, false, true);
//    //            //            oram->WriteNode(dummy, NULL, oram->evictBuckets, true);
//    //        }
//    //        if (rightNodeisNull == false) {
//    //            oram->ReadWrite(rightNode->key, rightNode, rightNode->pos, rightNode->pos, false, false);
//    //            //            oram->WriteNode(rightNode->key, rightNode);
//    //        } else {
//    //            oram->ReadWrite(dummy, tmpDummyNode, dumyPos, dumyPos, false, true);
//    //            //            oram->WriteNode(dummy, NULL, oram->evictBuckets, true);
//    //        }
//    //    } else {
//    //        oram->ReadWrite(dummy, tmpDummyNode, dumyPos, dumyPos, false, true);
//    //        oram->ReadWrite(dummy, tmpDummyNode, dumyPos, dumyPos, false, true);
//    //        //        oram->WriteNode(dummy, NULL, oram->evictBuckets, true);
//    //        //        oram->WriteNode(dummy, NULL, oram->evictBuckets, true);
//    //    }
//    //printf("log6\n");
//    //balance > 1
//    //    if (remainerIsDummy == false && balance > 1 && omapKey < node->leftID) {
//    if (remainerIsDummy == false && CTeq(CTcmp(balance, 1), 1) && omapKey < node->leftID) {
//        // Left Left Case
//        if (leftNodeisNull) {
//            //            printf("1.left node is null\n");
//            unsigned long long newLeftPos = RandomPath();
//            delete leftNode;
//            leftNode = oram->ReadWrite(node->leftID, tmpDummyNode, node->leftPos, newLeftPos, true, false);
//            leftNodeisNull = false;
//            //            leftNode = oram->ReadNode(node->leftID, node->leftPos, newLeftPos, true);
//            node->leftPos = newLeftPos;
//        } else {
//            unsigned long long newLeftPos = RandomPath();
//            oram->ReadWrite(dummy, tmpDummyNode, newLeftPos, newLeftPos, true, true);
//            //            oram->ReadNode(dummy, newLeftPos, newLeftPos, true);
//            node->leftPos = newLeftPos;
//        }
//
//        //        if (rightNodeisNull == false) {
//        //            oram->ReadWrite(rightNode->key, rightNode, rightNode->pos, rightNode->pos, false, false);
//        //            //            oram->WriteNode(rightNode->key, rightNode);
//        //        } else {
//        //            oram->ReadWrite(dummy, tmpDummyNode, dumyPos, dumyPos, false, true);
//        //            //            oram->WriteNode(dummy, NULL, oram->evictBuckets, true);
//        //        }
//
//        //------------------------DUMMY---------------------------
//        unsigned long long newleftRightPos = RandomPath();
//        Node* leftRightNode = oram->ReadWrite(dummy, tmpDummyNode, newleftRightPos, newleftRightPos, true, true);
//        //        Node* leftRightNode = oram->ReadNode(dummy, newleftRightPos, newleftRightPos, true);
//        leftRightNode->rightPos = leftRightNode->rightPos;
//
//        int leftLeftHeight = 0;
//        if (leftNode->leftID != 0) {
//            unsigned long long newleftLeftPos = RandomPath();
//            Node* leftLeftNode = oram->ReadWrite(dummy, tmpDummyNode, newleftLeftPos, newleftLeftPos, true, true);
//            //            Node* leftLeftNode = oram->ReadNode(dummy, newleftLeftPos, newleftLeftPos, true);
//            leftNode->leftPos = leftNode->leftPos;
//            leftLeftHeight = leftLeftHeight;
//            delete leftLeftNode;
//        } else {
//            unsigned long long newleftLeftPos = RandomPath();
//            Node* leftLeftNode = oram->ReadWrite(dummy, tmpDummyNode, newleftLeftPos, newleftLeftPos, true, true);
//            //            Node* leftLeftNode = oram->ReadNode(dummy, newleftLeftPos, newleftLeftPos, true);
//            leftNode->leftPos = leftNode->leftPos;
//            leftLeftHeight = leftLeftHeight;
//            delete leftLeftNode;
//        }
//        rotate(tmpDummyNode, tmpDummyNode, leftLeftHeight, false, true);
//        node->rightID = node->rightID;
//        node->rightPos = node->rightPos;
//        node->leftID = node->leftID;
//        node->leftPos = node->leftPos;
//        //---------------------------------------------------
//
//        rotate(node, leftNode, rightHeight, true);
//        rootPos = leftNode->pos;
//        Bid leftKey = leftNode->key;
//        height = leftNode->height;
//        oram->ReadWrite(leftNode->key, leftNode, leftNode->pos, leftNode->pos, false, false);
//        //        oram->WriteNode(leftNode->key, leftNode);
//        retKey = leftKey;
//
//        //balance < -1
//        //    } else if (remainerIsDummy == false && balance < -1 && omapKey > node->rightID) {
//    } else if (remainerIsDummy == false && CTeq(CTcmp(balance, -1), -1) && omapKey > node->rightID) {
//        // Right Right Case
//        if (rightNodeisNull) {
//            //            printf("1.right node is null\n");
//            unsigned long long newRightPos = RandomPath();
//            delete rightNode;
//            rightNode = oram->ReadWrite(node->rightID, tmpDummyNode, node->rightPos, newRightPos, true, false);
//            rightNodeisNull = false;
//            //            rightNode = oram->ReadNode(node->rightID, node->rightPos, newRightPos, true);
//            node->rightPos = newRightPos;
//        } else {
//            unsigned long long newRightPos = RandomPath();
//            oram->ReadWrite(dummy, tmpDummyNode, newRightPos, newRightPos, true, true);
//            //            oram->ReadNode(dummy, newRightPos, newRightPos, true);
//            node->rightPos = node->rightPos;
//        }
//        //        if (leftNodeisNull == false) {
//        //            oram->ReadWrite(leftNode->key, leftNode, leftNode->pos, leftNode->pos, false, false);
//        //            //            oram->WriteNode(leftNode->key, leftNode);
//        //        } else {
//        //            oram->ReadWrite(dummy, tmpDummyNode, dumyPos, dumyPos, false, true);
//        //            //            oram->WriteNode(dummy, NULL, oram->evictBuckets, true);
//        //        }
//
//        //------------------------DUMMY---------------------------
//        unsigned long long newleftRightPos = RandomPath();
//        Node* leftRightNode = oram->ReadWrite(dummy, tmpDummyNode, newleftRightPos, newleftRightPos, true, true);
//        //        Node* leftRightNode = oram->ReadNode(dummy, newleftRightPos, newleftRightPos, true);
//        leftRightNode->rightPos = leftRightNode->rightPos;
//
//        int leftLeftHeight = 0;
//        if (rightNode->leftID != 0) {
//            unsigned long long newleftLeftPos = RandomPath();
//            Node* leftLeftNode = oram->ReadWrite(dummy, tmpDummyNode, newleftLeftPos, newleftLeftPos, true, true);
//            //            Node* leftLeftNode = oram->ReadNode(dummy, newleftLeftPos, newleftLeftPos, true);
//            rightNode->leftPos = rightNode->leftPos;
//            leftLeftHeight = leftLeftHeight;
//            delete leftLeftNode;
//        } else {
//            unsigned long long newleftLeftPos = RandomPath();
//            Node* leftLeftNode = oram->ReadWrite(dummy, tmpDummyNode, newleftLeftPos, newleftLeftPos, true, true);
//            //            Node* leftLeftNode = oram->ReadNode(dummy, newleftLeftPos, newleftLeftPos, true);
//            rightNode->leftPos = rightNode->leftPos;
//            leftLeftHeight = leftLeftHeight;
//            delete leftLeftNode;
//        }
//        rotate(tmpDummyNode, tmpDummyNode, leftLeftHeight, false, true);
//        node->rightID = node->rightID;
//        node->rightPos = node->rightPos;
//        node->leftID = node->leftID;
//        node->leftPos = node->leftPos;
//        //---------------------------------------------------
//
//        rotate(node, rightNode, leftHeight, false);
//        rootPos = rightNode->pos;
//        Bid rightKey = rightNode->key;
//        height = rightNode->height;
//        oram->ReadWrite(rightNode->key, rightNode, rightNode->pos, rightNode->pos, false, false);
//        //        oram->WriteNode(rightNode->key, rightNode);
//        retKey = rightKey;
//
//        //balance > 1
//        //    } else if (remainerIsDummy == false && balance > 1 && omapKey > node->leftID) {
//    } else if (remainerIsDummy == false && CTeq(CTcmp(balance, 1), 1) && omapKey > node->leftID) {
//        // Left Right Case
//        if (leftNodeisNull) {
//            //            printf("2.left node is null\n");
//            unsigned long long newLeftPos = RandomPath();
//            delete leftNode;
//            leftNode = oram->ReadWrite(node->leftID, tmpDummyNode, node->leftPos, newLeftPos, true, false);
//            leftNodeisNull = false;
//            //            leftNode = oram->ReadNode(node->leftID, node->leftPos, newLeftPos, true);
//            node->leftPos = newLeftPos;
//        } else {
//            unsigned long long newLeftPos = RandomPath();
//            oram->ReadWrite(dummy, tmpDummyNode, newLeftPos, newLeftPos, true, true);
//            //            oram->ReadNode(dummy, newLeftPos, newLeftPos, true);
//            node->leftPos = node->leftPos;
//        }
//        //        if (rightNodeisNull == false) {
//        //            oram->ReadWrite(rightNode->key, rightNode, rightNode->pos, rightNode->pos, false, false);
//        //            //            oram->WriteNode(rightNode->key, rightNode);
//        //        } else {
//        //            oram->ReadWrite(dummy, tmpDummyNode, dumyPos, dumyPos, false, true);
//        //            //            oram->WriteNode(dummy, NULL, oram->evictBuckets, true);
//        //        }
//
//        unsigned long long newleftRightPos = RandomPath();
//        //        printf("left node:%lld:%lld:%lld:%lld:%lld:%lld:%lld\n", leftNode->key.getValue(), leftNode->height, leftNode->pos, leftNode->leftID.getValue(), leftNode->leftPos, leftNode->rightID.getValue(), leftNode->rightPos);
//
//        Node* leftRightNode = oram->ReadWrite(leftNode->rightID, tmpDummyNode, leftNode->rightPos, newleftRightPos, true, false);
//        //        Node* leftRightNode = oram->ReadNode(leftNode->rightID, leftNode->rightPos, newleftRightPos, true);
//        leftNode->rightPos = newleftRightPos;
//
//        int leftLeftHeight = 0;
//        if (leftNode->leftID != 0) {
//            unsigned long long newleftLeftPos = RandomPath();
//            Node* leftLeftNode = oram->ReadWrite(leftNode->leftID, tmpDummyNode, leftNode->leftPos, newleftLeftPos, true, false);
//            //            Node* leftLeftNode = oram->ReadNode(leftNode->leftID, leftNode->leftPos, newleftLeftPos);
//            leftNode->leftPos = newleftLeftPos;
//            leftLeftHeight = leftLeftNode->height;
//            //            printf("retreive left left node key:%lld height:%lld\n",leftNode->leftID.getValue(),leftLeftHeight);
//            delete leftLeftNode;
//        } else {
//            unsigned long long newleftLeftPos = RandomPath();
//            Node* leftLeftNode = oram->ReadWrite(dummy, tmpDummyNode, newleftLeftPos, newleftLeftPos, true, true);
//            //            Node* leftLeftNode = oram->ReadNode(dummy, newleftLeftPos, newleftLeftPos, true);
//            leftNode->leftPos = leftNode->leftPos;
//            leftLeftHeight = leftLeftHeight;
//            delete leftLeftNode;
//        }
//
//        rotate(leftNode, leftRightNode, leftLeftHeight, false);
//        node->rightID = node->rightID;
//        node->rightPos = node->rightPos;
//        node->leftID = leftRightNode->key;
//        node->leftPos = leftRightNode->pos;
//
//        rotate(node, leftRightNode, rightHeight, true);
//        rootPos = leftRightNode->pos;
//        Bid leftRightKey = leftRightNode->key;
//        height = leftRightNode->height;
//        oram->ReadWrite(leftRightNode->key, leftRightNode, leftRightNode->pos, leftRightNode->pos, false, false);
//        //        oram->WriteNode(leftRightNode->key, leftRightNode);
//        retKey = leftRightKey;
//
//        //balance < -1
//        //    } else if (remainerIsDummy == false && balance < -1 && omapKey < node->rightID) {
//    } else if (remainerIsDummy == false && CTeq(CTcmp(balance, -1), -1) && omapKey < node->rightID) {
//        // Right Left Case
//        if (rightNodeisNull) {
//            //            printf("2.right node is null\n");
//            unsigned long long newRightPos = RandomPath();
//            delete rightNode;
//            rightNode = oram->ReadWrite(node->rightID, tmpDummyNode, node->rightPos, newRightPos, true, false);
//            rightNodeisNull = false;
//            //            rightNode = oram->ReadNode(node->rightID, node->rightPos, newRightPos, true);
//            node->rightPos = newRightPos;
//        } else {
//            unsigned long long newRightPos = RandomPath();
//            oram->ReadWrite(dummy, tmpDummyNode, newRightPos, newRightPos, true, true);
//            //            oram->ReadNode(dummy, newRightPos, newRightPos, true);
//            node->rightPos = node->rightPos;
//        }
//        //        if (leftNodeisNull == false) {
//        //            oram->ReadWrite(leftNode->key, leftNode, leftNode->pos, leftNode->pos, false, false);
//        //            //            oram->WriteNode(leftNode->key, leftNode);
//        //        } else {
//        //            oram->ReadWrite(dummy, tmpDummyNode, dumyPos, dumyPos, false, true);
//        //            //            oram->WriteNode(dummy, NULL, oram->evictBuckets, true);
//        //        }
//
//        unsigned long long newRightLeftPos = RandomPath();
//        Node* rightLeftNode = oram->ReadWrite(rightNode->leftID, tmpDummyNode, rightNode->leftPos, newRightLeftPos, true, false);
//        //        Node* rightLeftNode = oram->ReadNode(rightNode->leftID, rightNode->leftPos, newRightLeftPos, true);
//        rightNode->leftPos = newRightLeftPos;
//
//        int rightRightHeight = 0;
//        if (rightNode->rightID != 0) {
//            unsigned long long newRightRightPos = RandomPath();
//            Node* rightRightNode = oram->ReadWrite(rightNode->rightID, tmpDummyNode, rightNode->rightPos, newRightRightPos, true, false);
//            //            Node* rightRightNode = oram->ReadNode(rightNode->rightID, rightNode->rightPos, newRightRightPos);
//            rightNode->rightPos = newRightRightPos;
//            rightRightHeight = rightRightNode->height;
//            //            printf("retreive right right node key:%lld height:%lld\n",rightNode->rightID.getValue(),rightRightHeight);
//            delete rightRightNode;
//        } else {
//            unsigned long long newRightRightPos = RandomPath();
//            Node* rightRightNode = oram->ReadWrite(dummy, tmpDummyNode, newRightRightPos, newRightRightPos, true, true);
//            //            Node* rightRightNode = oram->ReadNode(dummy, newRightRightPos, newRightRightPos, true);
//            rightNode->rightPos = rightNode->rightPos;
//            rightRightHeight = rightRightHeight;
//            delete rightRightNode;
//        }
//
//        rotate(rightNode, rightLeftNode, rightRightHeight, true);
//        node->rightID = rightLeftNode->key;
//        node->rightPos = rightLeftNode->pos;
//        node->leftID = node->leftID;
//        node->leftPos = node->leftPos;
//
//        rotate(node, rightLeftNode, leftHeight, false);
//        rootPos = rightLeftNode->pos;
//        Bid rightLefttKey = rightLeftNode->key;
//        height = rightLeftNode->height;
//        oram->ReadWrite(rightLeftNode->key, rightLeftNode, rightLeftNode->pos, rightLeftNode->pos, false, false);
//        //        oram->WriteNode(rightLeftNode->key, rightLeftNode);
//        retKey = rightLefttKey;
//    } else if (remainerIsDummy == false) {
//        //------------------------DUMMY---------------------------
//        if (node == NULL) {
//            unsigned long long newRightPos = RandomPath();
//            oram->ReadWrite(dummy, tmpDummyNode, newRightPos, newRightPos, true, true);
//            //            oram->ReadNode(dummy, newRightPos, newRightPos, true);
//            node->rightPos = node->rightPos;
//        } else {
//            unsigned long long newRightPos = RandomPath();
//            oram->ReadWrite(dummy, tmpDummyNode, newRightPos, newRightPos, true, true);
//            //            oram->ReadNode(dummy, newRightPos, newRightPos, true);
//            node->rightPos = node->rightPos;
//        }
//        //        if (node != NULL) {
//        //            oram->ReadWrite(dummy, tmpDummyNode, dumyPos, dumyPos, false, true);
//        //            //            oram->WriteNode(dummy, NULL, oram->evictBuckets, true);
//        //        } else {
//        //            oram->ReadWrite(dummy, tmpDummyNode, dumyPos, dumyPos, false, true);
//        //            //            oram->WriteNode(dummy, NULL, oram->evictBuckets, true);
//        //        }
//
//        unsigned long long newleftRightPos = RandomPath();
//        Node* leftRightNode = oram->ReadWrite(dummy, tmpDummyNode, newleftRightPos, newleftRightPos, true, true);
//        //        Node* leftRightNode = oram->ReadNode(dummy, newleftRightPos, newleftRightPos, true);
//        node->rightPos = node->rightPos;
//
//        int leftLeftHeight = 0;
//        if (node->leftID != 0) {
//            unsigned long long newleftLeftPos = RandomPath();
//            Node* leftLeftNode = oram->ReadWrite(dummy, tmpDummyNode, newleftLeftPos, newleftLeftPos, true, true);
//            //            Node* leftLeftNode = oram->ReadNode(dummy, newleftLeftPos, newleftLeftPos, true);
//            node->leftPos = node->leftPos;
//            leftLeftHeight = leftLeftHeight;
//            delete leftLeftNode;
//        } else {
//            unsigned long long newleftLeftPos = RandomPath();
//            Node* leftLeftNode = oram->ReadWrite(dummy, tmpDummyNode, newleftLeftPos, newleftLeftPos, true, true);
//            //            Node* leftLeftNode = oram->ReadNode(dummy, newleftLeftPos, newleftLeftPos, true);
//            node->leftPos = node->leftPos;
//            leftLeftHeight = leftLeftHeight;
//            delete leftLeftNode;
//        }
//
//        rotate(tmpDummyNode, tmpDummyNode, 0, false, true);
//        node->rightID = node->rightID;
//        node->rightPos = node->rightPos;
//        node->leftID = node->leftID;
//        node->leftPos = node->leftPos;
//
//        rotate(tmpDummyNode, tmpDummyNode, 0, false, true);
//        //----------------------------------------------------------------------------------------
//        rootPos = node->pos;
//        Bid keyRes = node->key;
//        height = height;
//        oram->ReadWrite(node->key, node, node->pos, node->pos, false, false);
//        //        oram->WriteNode(node->key, node);
//        retKey = keyRes;
//    } else {
//        //------------------------DUMMY---------------------------
//        if (node == NULL) {
//            unsigned long long newRightPos = RandomPath();
//            oram->ReadWrite(dummy, tmpDummyNode, newRightPos, newRightPos, true, true);
//            //            oram->ReadNode(dummy, newRightPos, newRightPos, true);
//            node->rightPos = node->rightPos;
//        } else {
//            unsigned long long newRightPos = RandomPath();
//            oram->ReadWrite(dummy, tmpDummyNode, newRightPos, newRightPos, true, true);
//            //            oram->ReadNode(dummy, newRightPos, newRightPos, true);
//            node->rightPos = node->rightPos;
//        }
//        //        if (node != NULL) {
//        //            oram->ReadWrite(dummy, tmpDummyNode, dumyPos, dumyPos, false, true);
//        //            //            oram->WriteNode(dummy, NULL, oram->evictBuckets, true);
//        //        } else {
//        //            oram->ReadWrite(dummy, tmpDummyNode, dumyPos, dumyPos, false, true);
//        //            //            oram->WriteNode(dummy, NULL, oram->evictBuckets, true);
//        //        }
//        unsigned long long newleftRightPos = RandomPath();
//        Node* leftRightNode = oram->ReadWrite(dummy, tmpDummyNode, newleftRightPos, newleftRightPos, true, true);
//        //        Node* leftRightNode = oram->ReadNode(dummy, newleftRightPos, newleftRightPos, true);
//        node->rightPos = node->rightPos;
//        int leftLeftHeight = 0;
//        if (node->leftID != 0) {
//            unsigned long long newleftLeftPos = RandomPath();
//            Node* leftLeftNode = oram->ReadWrite(dummy, tmpDummyNode, newleftLeftPos, newleftLeftPos, true, true);
//            //            Node* leftLeftNode = oram->ReadNode(dummy, newleftLeftPos, newleftLeftPos, true);
//            node->leftPos = node->leftPos;
//            leftLeftHeight = leftLeftHeight;
//            delete leftLeftNode;
//        } else {
//            unsigned long long newleftLeftPos = RandomPath();
//            Node* leftLeftNode = oram->ReadWrite(dummy, tmpDummyNode, newleftLeftPos, newleftLeftPos, true, true);
//            //            Node* leftLeftNode = oram->ReadNode(dummy, newleftLeftPos, newleftLeftPos, true);
//            node->leftPos = node->leftPos;
//            leftLeftHeight = leftLeftHeight;
//            delete leftLeftNode;
//        }
//        rotate(tmpDummyNode, tmpDummyNode, 0, false, true);
//        node->rightID = node->rightID;
//        node->rightPos = node->rightPos;
//        node->leftID = node->leftID;
//        node->leftPos = node->leftPos;
//
//        rotate(tmpDummyNode, tmpDummyNode, 0, false, true);
//        rootPos = rootPos;
//        Bid keyRes = node->key;
//        height = height;
//        oram->ReadWrite(dummy, tmpDummyNode, dumyPos, dumyPos, false, true);
//        //        oram->WriteNode(dummy, NULL, oram->evictBuckets, true);
//        retKey = retKey;
//        //----------------------------------------------------------------------------------------
//    }
//    delete tmpDummyNode;
//    return retKey;
//}

Bid AVLTree::insert(Bid rootKey, unsigned long long& rootPos, Bid omapKey, string value, int& height, Bid lastID, bool isDummyIns) {
    totheight++;
    if (isDummyIns && !CTeq(CTcmp(totheight, (int) ((float) oram->depth * 1.44)), -1)) {
        Bid resKey = lastID;
        if (!exist) {
            Node* nnode = newNode(omapKey, value);
            nnode->pos = RandomPath();
            Bid keyRes = nnode->key;
            height = nnode->height;
            rootPos = nnode->pos;
            oram->ReadWrite(omapKey, nnode, nnode->pos, nnode->pos, false, false);
            readWriteCacheNode(omapKey, nnode, false, false);
            resKey = nnode->key;
        }
        return resKey;
    }
    /* 1. Perform the normal BST rotation */
    unsigned long long rndPos = RandomPath();
    std::array< byte_t, 16> tmpval;
    std::fill(tmpval.begin(), tmpval.end(), 0);
    std::copy(value.begin(), value.end(), tmpval.begin());
    Node* tmpDummyNode = new Node();
    tmpDummyNode->isDummy = true;
    Bid dummy;
    Bid retKey;
    unsigned long long dumyPos;
    bool remainerIsDummy = false;
    dummy.setValue(oram->nextDummyCounter++);
    if (!isDummyIns && !rootKey.isZero()) {
        remainerIsDummy = false;
    } else {
        remainerIsDummy = true;
    }

    unsigned long long newPos = RandomPath();
    Node* node = nullptr;
    if (remainerIsDummy) {
        node = oram->ReadWrite(dummy, tmpDummyNode, rootPos, newPos, true, true, tmpval, Bid::CTeq(Bid::CTcmp(rootKey, omapKey), 0)); //READ
        readWriteCacheNode(dummy, tmpDummyNode, false, true);
    } else {
        node = oram->ReadWrite(rootKey, tmpDummyNode, rootPos, newPos, true, false, tmpval, Bid::CTeq(Bid::CTcmp(rootKey, omapKey), 0)); //READ
        readWriteCacheNode(rootKey, node, false, false);
    }


    int balance = -1;
    int leftHeight = -1;
    int rightHeight = -1;
    Node* leftNode = new Node();
    bool leftNodeisNull = true;
    Node* rightNode = new Node();
    bool rightNodeisNull = true;
    std::array< byte_t, 16> garbage;
    bool childDirisLeft = false;


    unsigned long long newRLPos = RandomPath();

    if (!remainerIsDummy) {
        if (omapKey < node->key) {
            node->leftID = insert(node->leftID, node->leftPos, omapKey, value, leftHeight, dummy, false);
            if (!node->rightID.isZero()) {
                Node* tmprightNode = oram->ReadWrite(node->rightID, tmpDummyNode, node->rightPos, newRLPos, true, false); //READ
                readWriteCacheNode(node->rightID, tmprightNode, false, false);
            } else {
                Node* dummyright = oram->ReadWrite(dummy, tmpDummyNode, newRLPos, newRLPos, true, true);
                readWriteCacheNode(dummy, dummyright, false, true);
            }
            childDirisLeft = true;

            totheight--;
            if (node->rightID == 0) {
                readWriteCacheNode(dummy, tmpDummyNode, true, true);
                rightHeight = 0;
                node->rightPos = node->rightPos;
                std::fill(garbage.begin(), garbage.end(), 0);
                std::copy(value.begin(), value.end(), garbage.begin());
                Bid keyRes = node->key;
                rootPos = rootPos;
                height = height;
                retKey = retKey;
                remainerIsDummy = false;
            } else {
                delete rightNode;
                rightNode = readWriteCacheNode(node->rightID, tmpDummyNode, true, false);
                rightNodeisNull = false;
                rightHeight = rightNode->height;
                node->rightPos = rightNode->pos;
                std::fill(garbage.begin(), garbage.end(), 0);
                std::copy(value.begin(), value.end(), garbage.begin());
                Bid keyRes = node->key;
                rootPos = rootPos;
                height = height;
                retKey = retKey;
                remainerIsDummy = false;
            }
        } else if (omapKey > node->key) {
            node->rightID = insert(node->rightID, node->rightPos, omapKey, value, rightHeight, dummy, false);
            if (!node->leftID.isZero()) {
                Node* tmpleftNode = oram->ReadWrite(node->leftID, tmpDummyNode, node->leftPos, newRLPos, true, false); //READ
                readWriteCacheNode(node->leftID, tmpleftNode, false, false);
            } else {
                Node* dummyleft = oram->ReadWrite(dummy, tmpDummyNode, newRLPos, newRLPos, true, true);
                readWriteCacheNode(dummy, dummyleft, false, true);
            }
            childDirisLeft = false;
            totheight--;
            if (node->leftID == 0) {
                readWriteCacheNode(dummy, tmpDummyNode, true, true);
                leftHeight = 0;
                node->leftPos = node->leftPos;
                std::fill(garbage.begin(), garbage.end(), 0);
                std::copy(value.begin(), value.end(), garbage.begin());
                Bid keyRes = node->key;
                rootPos = rootPos;
                height = height;
                retKey = retKey;
                remainerIsDummy = false;

            } else {
                delete leftNode;
                leftNode = readWriteCacheNode(node->leftID, tmpDummyNode, true, false); //READ
                leftNodeisNull = false;
                leftHeight = leftNode->height;
                node->leftPos = leftNode->pos;
                std::fill(garbage.begin(), garbage.end(), 0);
                std::copy(value.begin(), value.end(), garbage.begin());
                Bid keyRes = node->key;
                rootPos = rootPos;
                height = height;
                retKey = retKey;
                remainerIsDummy = false;
            }
        } else {
            exist = true;
            Bid resValBid = insert(rootKey, rootPos, omapKey, value, height, node->key, true);
            totheight--;
            readWriteCacheNode(dummy, tmpDummyNode, true, true);
            readWriteCacheNode(dummy, tmpDummyNode, true, true);
            rootPos = node->pos;
            height = node->height;
            retKey = node->key;
            remainerIsDummy = true;
        }
    } else {
        Bid resValBid = insert(rootKey, rootPos, omapKey, value, height, node->key, true);
        totheight--;
        readWriteCacheNode(dummy, tmpDummyNode, true, true);
        readWriteCacheNode(dummy, tmpDummyNode, true, true);
        leftHeight = leftHeight;
        node->leftPos = node->leftPos;
        std::fill(garbage.begin(), garbage.end(), 0);
        std::copy(value.begin(), value.end(), garbage.begin());
        Bid keyRes = node->key;
        rootPos = rootPos;
        height = height;
        retKey = resValBid;
        remainerIsDummy = remainerIsDummy;
    }
    /* 2. Update height of this ancestor node */
    if (remainerIsDummy) {
        max(leftHeight, rightHeight) + 1;
        node->height = node->height;
        height = height;
    } else {
        node->height = max(leftHeight, rightHeight) + 1;
        height = node->height;
    }
    /* 3. Get the balance factor of this ancestor node to check whether this node became unbalanced */
    if (remainerIsDummy) {
        balance = balance;
    } else {
        balance = leftHeight - rightHeight;
    }

    if (remainerIsDummy == false && CTeq(CTcmp(balance, 1), 1) && omapKey < node->leftID) {
        // Left Left Case
        if (leftNodeisNull) {
            unsigned long long newLeftPos = RandomPath();
            delete leftNode;
            leftNode = readWriteCacheNode(node->leftID, tmpDummyNode, true, false); //READ
            //            leftNode = oram->ReadWrite(node->leftID, tmpDummyNode, node->leftPos, newLeftPos, true, false); //READ
            leftNodeisNull = false;
            node->leftPos = leftNode->pos;
        } else {
            unsigned long long newLeftPos = RandomPath();
            readWriteCacheNode(dummy, tmpDummyNode, true, true); //WRITE            
            //            oram->ReadWrite(dummy, tmpDummyNode, newLeftPos, newLeftPos, true, true); //WRITE            
        }

        //------------------------DUMMY---------------------------
        unsigned long long newleftRightPos = RandomPath();
        Node* leftRightNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
        //        Node* leftRightNode = oram->ReadWrite(dummy, tmpDummyNode, newleftRightPos, newleftRightPos, true, true); //READ
        leftRightNode->rightPos = leftRightNode->rightPos;

        int leftLeftHeight = 0;
        if (leftNode->leftID != 0) {
            unsigned long long newleftLeftPos = RandomPath();
            Node* leftLeftNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
            //            Node* leftLeftNode = oram->ReadWrite(dummy, tmpDummyNode, newleftLeftPos, newleftLeftPos, true, true); //READ
            leftNode->leftPos = leftNode->leftPos;
            leftLeftHeight = leftLeftHeight;
            delete leftLeftNode;
        } else {
            unsigned long long newleftLeftPos = RandomPath();
            Node* leftLeftNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
            //            Node* leftLeftNode = oram->ReadWrite(dummy, tmpDummyNode, newleftLeftPos, newleftLeftPos, true, true); //READ
            leftNode->leftPos = leftNode->leftPos;
            leftLeftHeight = leftLeftHeight;
            delete leftLeftNode;
        }
        rotate(tmpDummyNode, tmpDummyNode, leftLeftHeight, false, true);
        node->rightID = node->rightID;
        node->rightPos = node->rightPos;
        node->leftID = node->leftID;
        node->leftPos = node->leftPos;
        //---------------------------------------------------

        rotate(node, leftNode, rightHeight, true);
        rootPos = leftNode->pos;
        Bid leftKey = leftNode->key;
        height = leftNode->height;

        oram->ReadWrite(node->key, node, node->pos, node->pos, false, false); //WRITE
        readWriteCacheNode(node->key, node, false, false); //WRITE

        oram->ReadWrite(leftNode->key, leftNode, leftNode->pos, leftNode->pos, false, false); //WRITE
        readWriteCacheNode(leftNode->key, leftNode, false, false); //WRITE
        retKey = leftKey;
        readWriteCacheNode(dummy, tmpDummyNode, true, true);

    } else if (remainerIsDummy == false && CTeq(CTcmp(balance, -1), -1) && omapKey > node->rightID) {
        // Right Right Case
        if (rightNodeisNull) {
            unsigned long long newRightPos = RandomPath();
            delete rightNode;
            rightNode = readWriteCacheNode(node->rightID, tmpDummyNode, true, false); //READ
            //            rightNode = oram->ReadWrite(node->rightID, tmpDummyNode, node->rightPos, newRightPos, true, false); //READ
            rightNodeisNull = false;
            node->rightPos = rightNode->pos;
        } else {
            unsigned long long newRightPos = RandomPath();
            readWriteCacheNode(dummy, tmpDummyNode, true, true);
            //            oram->ReadWrite(dummy, tmpDummyNode, newRightPos, newRightPos, true, true); //READ
        }

        //------------------------DUMMY---------------------------
        unsigned long long newleftRightPos = RandomPath();
        Node* leftRightNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
        //        Node* leftRightNode = oram->ReadWrite(dummy, tmpDummyNode, newleftRightPos, newleftRightPos, true, true); //READ

        int leftLeftHeight = 0;
        if (rightNode->leftID != 0) {
            unsigned long long newleftLeftPos = RandomPath();
            Node* leftLeftNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
            //            Node* leftLeftNode = oram->ReadWrite(dummy, tmpDummyNode, newleftLeftPos, newleftLeftPos, true, true); //READ
            rightNode->leftPos = rightNode->leftPos;
            leftLeftHeight = leftLeftHeight;
            delete leftLeftNode;
        } else {
            unsigned long long newleftLeftPos = RandomPath();
            Node* leftLeftNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
            //            Node* leftLeftNode = oram->ReadWrite(dummy, tmpDummyNode, newleftLeftPos, newleftLeftPos, true, true); //READ
            rightNode->leftPos = rightNode->leftPos;
            leftLeftHeight = leftLeftHeight;
            delete leftLeftNode;
        }
        rotate(tmpDummyNode, tmpDummyNode, leftLeftHeight, false, true);
        node->rightID = node->rightID;
        node->rightPos = node->rightPos;
        node->leftID = node->leftID;
        node->leftPos = node->leftPos;
        //---------------------------------------------------

        rotate(node, rightNode, leftHeight, false);
        rootPos = rightNode->pos;
        Bid rightKey = rightNode->key;
        height = rightNode->height;

        oram->ReadWrite(node->key, node, node->pos, node->pos, false, false); //WRITE
        readWriteCacheNode(node->key, node, false, false); //WRITE

        oram->ReadWrite(rightNode->key, rightNode, rightNode->pos, rightNode->pos, false, false); //WRITE
        readWriteCacheNode(rightNode->key, rightNode, false, false); //WRITE
        retKey = rightKey;

        readWriteCacheNode(dummy, tmpDummyNode, true, true);

    } else if (remainerIsDummy == false && CTeq(CTcmp(balance, 1), 1) && omapKey > node->leftID) {
        // Left Right Case
        if (leftNodeisNull) {
            unsigned long long newLeftPos = RandomPath();
            delete leftNode;
            leftNode = readWriteCacheNode(node->leftID, tmpDummyNode, true, false); //READ
            //            leftNode = oram->ReadWrite(node->leftID, tmpDummyNode, node->leftPos, newLeftPos, true, false); //READ
            leftNodeisNull = false;
            node->leftPos = leftNode->pos;
        } else {
            unsigned long long newLeftPos = RandomPath();
            readWriteCacheNode(dummy, tmpDummyNode, true, true);
            //            oram->ReadWrite(dummy, tmpDummyNode, newLeftPos, newLeftPos, true, true); //READ
        }


        unsigned long long newleftRightPos = RandomPath();
        Node* leftRightNode = readWriteCacheNode(leftNode->rightID, tmpDummyNode, true, false); //READ
        //        Node* leftRightNode = oram->ReadWrite(leftNode->rightID, tmpDummyNode, leftNode->rightPos, newleftRightPos, true, false); //READ

        int leftLeftHeight = 0;
        if (leftNode->leftID != 0) {
            unsigned long long newleftLeftPos = RandomPath();
            Node* leftLeftNode = readWriteCacheNode(leftNode->leftID, tmpDummyNode, true, false); //READ
            //            Node* leftLeftNode = oram->ReadWrite(leftNode->leftID, tmpDummyNode, leftNode->leftPos, newleftLeftPos, true, false); //READ
            leftLeftHeight = leftLeftNode->height;
            delete leftLeftNode;
        } else {
            unsigned long long newleftLeftPos = RandomPath();
            Node* leftLeftNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
            //            Node* leftLeftNode = oram->ReadWrite(dummy, tmpDummyNode, newleftLeftPos, newleftLeftPos, true, true); //READ
            leftNode->leftPos = leftNode->leftPos;
            leftLeftHeight = leftLeftHeight;
            delete leftLeftNode;
        }

        rotate(leftNode, leftRightNode, leftLeftHeight, false);

        oram->ReadWrite(leftNode->key, leftNode, leftNode->pos, leftNode->pos, false, false); //WRITE
        readWriteCacheNode(leftNode->key, leftNode, false, false); //WRITE

        node->rightID = node->rightID;
        node->rightPos = node->rightPos;
        node->leftID = leftRightNode->key;
        node->leftPos = leftRightNode->pos;

        rotate(node, leftRightNode, rightHeight, true);
        oram->ReadWrite(node->key, node, node->pos, node->pos, false, false); //WRITE
        readWriteCacheNode(node->key, node, false, false); //WRITE


        rootPos = leftRightNode->pos;
        Bid leftRightKey = leftRightNode->key;
        height = leftRightNode->height;
        //        oram->ReadWrite(leftRightNode->key, leftRightNode, leftRightNode->pos, leftRightNode->pos, false, false); //WRITE
        readWriteCacheNode(leftRightNode->key, leftRightNode, false, false); //WRITE
        doubleRotation = true;
        retKey = leftRightKey;

    } else if (remainerIsDummy == false && CTeq(CTcmp(balance, -1), -1) && omapKey < node->rightID) {
        if (rightNodeisNull) {
            unsigned long long newRightPos = RandomPath();
            delete rightNode;
            rightNode = readWriteCacheNode(node->rightID, tmpDummyNode, true, false); //READ
            //            rightNode = oram->ReadWrite(node->rightID, tmpDummyNode, node->rightPos, newRightPos, true, false); //READ
            rightNodeisNull = false;
            node->rightPos = rightNode->pos;
        } else {
            unsigned long long newRightPos = RandomPath();
            readWriteCacheNode(dummy, tmpDummyNode, true, true);
            //            oram->ReadWrite(dummy, tmpDummyNode, newRightPos, newRightPos, true, true); //READ
            node->rightPos = node->rightPos;
        }

        unsigned long long newRightLeftPos = RandomPath();
        Node* rightLeftNode = readWriteCacheNode(rightNode->leftID, tmpDummyNode, true, false); //READ
        //        Node* rightLeftNode = oram->ReadWrite(rightNode->leftID, tmpDummyNode, rightNode->leftPos, newRightLeftPos, true, false); //READ

        int rightRightHeight = 0;
        if (rightNode->rightID != 0) {
            unsigned long long newRightRightPos = RandomPath();
            Node* rightRightNode = readWriteCacheNode(rightNode->rightID, tmpDummyNode, true, false); //READ
            //            Node* rightRightNode = oram->ReadWrite(rightNode->rightID, tmpDummyNode, rightNode->rightPos, newRightRightPos, true, false); //READ
            rightRightHeight = rightRightNode->height;
            delete rightRightNode;
        } else {
            unsigned long long newRightRightPos = RandomPath();
            Node* rightRightNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
            //            Node* rightRightNode = oram->ReadWrite(dummy, tmpDummyNode, newRightRightPos, newRightRightPos, true, true); //READ
            rightNode->rightPos = rightNode->rightPos;
            rightRightHeight = rightRightHeight;
            delete rightRightNode;
        }

        rotate(rightNode, rightLeftNode, rightRightHeight, true);

        oram->ReadWrite(rightNode->key, rightNode, rightNode->pos, rightNode->pos, false, false); //WRITE
        readWriteCacheNode(rightNode->key, rightNode, false, false); //WRITE

        node->rightID = rightLeftNode->key;
        node->rightPos = rightLeftNode->pos;
        node->leftID = node->leftID;
        node->leftPos = node->leftPos;

        rotate(node, rightLeftNode, leftHeight, false);

        oram->ReadWrite(node->key, node, node->pos, node->pos, false, false); //WRITE
        readWriteCacheNode(node->key, node, false, false); //WRITE

        rootPos = rightLeftNode->pos;
        Bid rightLefttKey = rightLeftNode->key;
        height = rightLeftNode->height;

        //        oram->ReadWrite(rightLeftNode->key, rightLeftNode, rightLeftNode->pos, rightLeftNode->pos, false, false); //WRITE
        readWriteCacheNode(rightLeftNode->key, rightLeftNode, false, false); //WRITE
        doubleRotation = true;
        retKey = rightLefttKey;
    } else if (remainerIsDummy == false) {
        //------------------------DUMMY---------------------------
        if (node == NULL) {
            unsigned long long newRightPos = RandomPath();
            readWriteCacheNode(dummy, tmpDummyNode, true, true);
            //            oram->ReadWrite(dummy, tmpDummyNode, newRightPos, newRightPos, true, true); //READ
            node->rightPos = node->rightPos;
        } else {
            unsigned long long newRightPos = RandomPath();
            readWriteCacheNode(dummy, tmpDummyNode, true, true);
            //            oram->ReadWrite(dummy, tmpDummyNode, newRightPos, newRightPos, true, true); //READ
            node->rightPos = node->rightPos;
        }

        unsigned long long newleftRightPos = RandomPath();
        readWriteCacheNode(dummy, tmpDummyNode, true, true);
        //        Node* leftRightNode = oram->ReadWrite(dummy, tmpDummyNode, newleftRightPos, newleftRightPos, true, true); //READ
        node->rightPos = node->rightPos;

        int leftLeftHeight = 0;
        if (node->leftID != 0) {
            Node* leftLeftNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
            //            Node* leftLeftNode = oram->ReadWrite(dummy, tmpDummyNode, newleftLeftPos, newleftLeftPos, true, true); //READ
            node->leftPos = node->leftPos;
            leftLeftHeight = leftLeftHeight;
            delete leftLeftNode;
        } else {
            Node* leftLeftNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
            node->leftPos = node->leftPos;
            leftLeftHeight = leftLeftHeight;
            delete leftLeftNode;
        }

        rotate(tmpDummyNode, tmpDummyNode, 0, false, true);
        node->rightID = node->rightID;
        node->rightPos = node->rightPos;
        node->leftID = node->leftID;
        node->leftPos = node->leftPos;

        rotate(tmpDummyNode, tmpDummyNode, 0, false, true);

        if (doubleRotation) {
            doubleRotation = false;
            if (childDirisLeft) {
                leftNode = readWriteCacheNode(node->leftID, tmpDummyNode, true, false);
                oram->ReadWrite(leftNode->key, leftNode, leftNode->pos, leftNode->pos, false, false); //WRITE
            } else {
                rightNode = readWriteCacheNode(node->rightID, tmpDummyNode, true, false);
                oram->ReadWrite(rightNode->key, rightNode, rightNode->pos, rightNode->pos, false, false); //WRITE
            }
        } else {
            readWriteCacheNode(dummy, tmpDummyNode, true, true);
            oram->ReadWrite(dummy, tmpDummyNode, rightNode->pos, rightNode->pos, false, true); //WRITE
        }
        //----------------------------------------------------------------------------------------
        rootPos = node->pos;
        Bid keyRes = node->key;
        height = height;
        //        node = readWriteCacheNode(node->key,tmpDummyNode,true,false);
        oram->ReadWrite(node->key, node, node->pos, node->pos, false, false); //WRITE
        readWriteCacheNode(node->key, node, false, false); //WRITE
        retKey = keyRes;
    } else {
        //------------------------DUMMY---------------------------
        if (node == NULL) {
            unsigned long long newRightPos = RandomPath();
            readWriteCacheNode(dummy, tmpDummyNode, true, true);
            //            oram->ReadWrite(dummy, tmpDummyNode, newRightPos, newRightPos, true, true); //READ
            node->rightPos = node->rightPos;
        } else {
            unsigned long long newRightPos = RandomPath();
            readWriteCacheNode(dummy, tmpDummyNode, true, true);
            //            oram->ReadWrite(dummy, tmpDummyNode, newRightPos, newRightPos, true, true); //READ
            node->rightPos = node->rightPos;
        }

        readWriteCacheNode(dummy, tmpDummyNode, true, true);
        //        Node* leftRightNode = oram->ReadWrite(dummy, tmpDummyNode, newleftRightPos, newleftRightPos, true, true); //READ
        //        node->rightPos = leftNode->rightPos;
        int leftLeftHeight = 0;
        if (node->leftID != 0) {
            Node* leftLeftNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
            //            Node* leftLeftNode = oram->ReadWrite(dummy, tmpDummyNode, newleftLeftPos, newleftLeftPos, true, true); //READ
            node->leftPos = node->leftPos;
            leftLeftHeight = leftLeftHeight;
            delete leftLeftNode;
        } else {
            Node* leftLeftNode = readWriteCacheNode(dummy, tmpDummyNode, true, true);
            //            Node* leftLeftNode = oram->ReadWrite(dummy, tmpDummyNode, newleftLeftPos, newleftLeftPos, true, true); //READ
            node->leftPos = node->leftPos;
            leftLeftHeight = leftLeftHeight;
            delete leftLeftNode;
        }
        rotate(tmpDummyNode, tmpDummyNode, 0, false, true);
        node->rightID = node->rightID;
        node->rightPos = node->rightPos;
        node->leftID = node->leftID;
        node->leftPos = node->leftPos;

        rotate(tmpDummyNode, tmpDummyNode, 0, false, true);
        if (doubleRotation) {
            doubleRotation = false;
            //printf("double rotate\n");
            if (childDirisLeft) {
                leftNode = readWriteCacheNode(node->leftID, tmpDummyNode, true, false);
                oram->ReadWrite(leftNode->key, leftNode, leftNode->pos, leftNode->pos, false, false); //WRITE
            } else {
                rightNode = readWriteCacheNode(node->rightID, tmpDummyNode, true, false);
                oram->ReadWrite(rightNode->key, rightNode, rightNode->pos, rightNode->pos, false, false); //WRITE
            }
        } else {
            readWriteCacheNode(dummy, tmpDummyNode, true, true);
            oram->ReadWrite(dummy, tmpDummyNode, rightNode->pos, rightNode->pos, false, true); //WRITE
        }
        rootPos = rootPos;
        Bid keyRes = node->key;
        height = height;
        oram->ReadWrite(dummy, tmpDummyNode, dumyPos, dumyPos, false, true); //WRITE
        readWriteCacheNode(dummy, tmpDummyNode, true, true);
        retKey = retKey;
        //----------------------------------------------------------------------------------------
    }
    delete tmpDummyNode;
    return retKey;
}

string AVLTree::search(Node* rootNode, Bid omapKey) {
    Bid curKey = rootNode->key;
    unsigned long long lastPos = rootNode->pos;
    unsigned long long newPos = RandomPath();
    rootNode->pos = newPos;
    string res = "";
    Bid dumyID = oram->nextDummyCounter;
    Node* tmpDummyNode = new Node();
    tmpDummyNode->isDummy = true;
    tmpDummyNode->pos = RandomPath();
    std::array< byte_t, 16> resVec;
    Node* head;
    int dummyState = 0;
    int upperBound = (int) (1.44 * oram->depth);
    bool found = false;
    bool notexist = false;
    unsigned long long dumyPos = RandomPath();

    do {
        unsigned long long rnd = RandomPath();
        unsigned long long rnd2 = RandomPath();
        bool isDummyAction = Node::CTeq(Node::CTcmp(dummyState, 1), 0);
        head = oram->ReadWrite(curKey, lastPos, newPos, isDummyAction, rnd2, omapKey);

        bool cond1 = Node::CTeq(Node::CTcmp(dummyState, 1), 0);
        bool cond2 = Node::CTeq(Bid::CTcmp(head->key, omapKey), 1);
        bool cond3 = Node::CTeq(Bid::CTcmp(head->key, omapKey), -1);
        bool cond4 = Node::CTeq(Bid::CTcmp(head->key, omapKey), 0);

        notexist = Node::conditional_select(true, notexist, (!cond1 && ((cond2 && head->leftID.isZero()) || (cond3 && head->rightID.isZero()))));

        lastPos = Node::conditional_select(rnd, lastPos, cond1 || (!cond1 && ((cond2 && head->leftID.isZero()) || (cond3 && head->rightID.isZero()))));
        lastPos = Node::conditional_select(head->leftPos, lastPos, !cond1 && cond2);
        lastPos = Node::conditional_select(head->rightPos, lastPos, !cond1 && !cond2 && cond3);

        curKey = dumyID;
        for (int k = 0; k < curKey.id.size(); k++) {
            curKey.id[k] = Node::conditional_select(head->leftID.id[k], curKey.id[k], !cond1 && cond2 && !head->leftID.isZero());
            curKey.id[k] = Node::conditional_select(head->rightID.id[k], curKey.id[k], !cond1 && !cond2 && cond3 && !head->rightID.isZero());
        }

        newPos = Node::conditional_select(rnd, newPos, cond1);
        newPos = Node::conditional_select(rnd2, newPos, !cond1 && cond2 && !head->leftID.isZero());
        newPos = Node::conditional_select(rnd2, newPos, !cond1 && !cond2 && cond3 && !head->rightID.isZero());

        for (int i = 0; i < 16; i++) {
            resVec[i] = Bid::conditional_select(head->value[i], resVec[i], !cond1);
        }

        dummyState = Node::conditional_select(dummyState + 1, dummyState, !cond1 && !cond2 && !cond3 && cond4 || (!cond1 && ((cond2 && head->leftID.isZero()) || (cond3 && head->rightID.isZero()))));

        for (int k = 0; k < head->key.id.size(); k++) {
            head->key.id[k] = Node::conditional_select(dumyID.id[k], head->key.id[k], cond1);
        }
        found = Node::conditional_select(true, found, !cond1 && !cond2 && !cond3 && cond4);
    } while ((!found && !notexist) || oram->readCnt <= upperBound);
    delete tmpDummyNode;
    if (found) {
        res.assign(resVec.begin(), resVec.end());
    }
    return res;
}




//string AVLTree::search(Node* rootNode, Bid omapKey) {
//    Bid curKey = rootNode->key;
//    unsigned long long lastPos = rootNode->pos;
//    unsigned long long newPos = RandomPath();
//    rootNode->pos = newPos;
//    string res = "";
//    Bid dumyID = oram->nextDummyCounter;
//    Node* tmpDummyNode = new Node();
//    tmpDummyNode->isDummy = true;
//    Node* head;
//    int dummyState = 0;
//    int upperBound = (int) (1.44 * oram->depth);
//    bool found = false;
//    unsigned long long dumyPos;
//    do {
//        head = oram->ReadWrite(curKey, tmpDummyNode, lastPos, newPos, true, dummyState > 1 ? true : false);
//        //        head = oram->ReadNode(curKey, lastPos, newPos, dummyState > 1 ? true : false);
//        unsigned long long rnd = RandomPath();
//        if (dummyState > 1) {
//            lastPos = rnd;
//            head->rightPos = head->rightPos;
//            head->leftPos = head->leftPos;
//            curKey = dumyID;
//            newPos = rnd;
//            res.assign(res.begin(), res.end());
//            dummyState = dummyState;
//            head->key = dumyID;
//            found = found;
//        } else if (head->key > omapKey) {
//            lastPos = head->leftPos;
//            head->rightPos = head->rightPos;
//            head->leftPos = rnd;
//            curKey = head->leftID;
//            newPos = head->leftPos;
//            res.assign(head->value.begin(), head->value.end());
//            dummyState = dummyState;
//            head->key = head->key;
//            found = found;
//        } else if (head->key < omapKey) {
//            lastPos = head->rightPos;
//            head->rightPos = rnd;
//            head->leftPos = head->leftPos;
//            curKey = head->rightID;
//            newPos = head->rightPos;
//            res.assign(head->value.begin(), head->value.end());
//            dummyState = dummyState;
//            head->key = head->key;
//            found = found;
//        } else {
//            lastPos = lastPos;
//            head->rightPos = head->rightPos;
//            head->leftPos = head->leftPos;
//            curKey = dumyID;
//            newPos = newPos;
//            res.assign(head->value.begin(), head->value.end());
//            dummyState = dummyState;
//            dummyState++;
//            head->key = head->key;
//            found = true;
//        }
//        oram->ReadWrite(head->key, head, head->pos, head->pos, false, dummyState <= 1 ? false : true);
//        //        oram->WriteNode(head->key, head, oram->evictBuckets, dummyState <= 1 ? false : true);
//        dummyState == 1 ? dummyState++ : dummyState;
//    } while (!found || oram->readCnt < upperBound);
//    
//    return res;
//}

/**
 * a recursive search function which traverse binary tree to find the target node
 */
void AVLTree::batchSearch(Node* head, vector<Bid> keys, vector<Node*>* results) {
    //    if (head == NULL || head->key == 0) {
    //        return;
    //    }
    //    head = oram->ReadNode(head->key, head->pos, head->pos);
    //    bool getLeft = false, getRight = false;
    //    vector<Bid> leftkeys, rightkeys;
    //    for (Bid bid : keys) {
    //        if (head->key > bid) {
    //            getLeft = true;
    //            leftkeys.push_back(bid);
    //        }
    //        if (head->key < bid) {
    //            getRight = true;
    //            rightkeys.push_back(bid);
    //        }
    //        if (head->key == bid) {
    //            results->push_back(head);
    //        }
    //    }
    //    if (getLeft) {
    //        batchSearch(oram->ReadNode(head->leftID, head->leftPos, head->leftPos), leftkeys, results);
    //    }
    //    if (getRight) {
    //        batchSearch(oram->ReadNode(head->rightID, head->rightPos, head->rightPos), rightkeys, results);
    //    }
}

void AVLTree::printTree(Node* root, int indent) {
    Node* tmpDummyNode = new Node();
    tmpDummyNode->isDummy = true;
    if (root != 0 && root->key != 0) {
        root = oram->ReadWrite(root->key, tmpDummyNode, root->pos, root->pos, true, false);
        if (root->leftID != 0)
            printTree(oram->ReadWrite(root->leftID, tmpDummyNode, root->leftPos, root->leftPos, true, false), indent + 4);
        if (indent > 0) {
            for (int i = 0; i < indent; i++) {
                printf(" ");
            }
        }

        string value;
        value.assign(root->value.begin(), root->value.end());
        printf("%d:%d:%s:%d:%d:%d:%d:%d\n", root->key.getValue(), root->height, value.c_str(), root->pos, root->leftID.getValue(), root->leftPos, root->rightID.getValue(), root->rightPos);
        if (root->rightID != 0)
            printTree(oram->ReadWrite(root->rightID, tmpDummyNode, root->rightPos, root->rightPos, true, false), indent + 4);

    }
}

/*
 * before executing each operation, this function should be called with proper arguments
 */
void AVLTree::startOperation(bool batchWrite) {
    oram->start(batchWrite);
    totheight = 0;
    exist = false;
}

/*
 * after executing each operation, this function should be called with proper arguments
 */
void AVLTree::finishOperation() {
    for (auto item : avlCache) {
        delete item;
    }
    avlCache.clear();
    oram->finilize();
}

unsigned long long AVLTree::RandomPath() {
    uint32_t val;
    sgx_read_rand((unsigned char *) &val, 4);
    return val % (maxOfRandom);
}

AVLTree::AVLTree(long long maxSize, bytes<Key> secretkey, Bid& rootKey, unsigned long long& rootPos, map<Bid, string> pairs, map<unsigned long long, unsigned long long> permutation) {
    int depth = (int) (ceil(log2(maxSize)) - 1) + 1;
    maxOfRandom = (long long) (pow(2, depth));

    vector<Node*> nodes;
    for (auto pair : pairs) {
        Node* node = newNode(pair.first, pair.second);
        nodes.push_back(node);
    }

    int nextPower2 = (int) pow(2, ceil(log2(nodes.size())));
    for (int i = (int) nodes.size(); i < nextPower2; i++) {
        Bid bid = INF + i;
        Node* node = newNode(bid, "");
        node->isDummy = false;
        nodes.push_back(node);
    }

    double t;
    printf("Creating BST of %d Nodes\n", nodes.size());
    sortedArrayToBST(&nodes, 0, nodes.size() - 1, rootPos, rootKey);
    printf("Inserting in ORAM\n");

    double gp;
    int size = (int) nodes.size();
    for (int i = size; i < maxOfRandom * Z; i++) {
        Bid bid;
        bid = INF + i;
        Node* node = newNode(bid, "");
        node->isDummy = true;
        node->pos = permutation[permutationIterator];
        permutationIterator++;
        nodes.push_back(node);
    }
    oram = new ORAM(maxSize, secretkey, &nodes);
}

void AVLTree::setupInsert(Bid& rootKey, unsigned long long& rootPos, map<Bid, string>& pairs) {
    vector<Node*> nodes;
    for (auto pair : pairs) {
        Node* node = newNode(pair.first, pair.second);
        nodes.push_back(node);
    }

    int nextPower2 = (int) pow(2, ceil(log2(nodes.size())));
    for (int i = (int) nodes.size(); i < nextPower2; i++) {
        Bid bid = INF + i;
        Node* node = newNode(bid, "");
        node->isDummy = false;
        nodes.push_back(node);
    }

    double t;
    printf("Creating BST of %d Nodes\n", nodes.size());
    sortedArrayToBST(&nodes, 0, nodes.size() - 1, rootPos, rootKey);
    printf("Inserting in ORAM\n");

    double gp;
    int size = (int) nodes.size();
    for (int i = size; i < maxOfRandom * Z; i++) {
        Bid bid;
        bid = INF + i;
        Node* node = newNode(bid, "");
        node->isDummy = true;
        node->pos = getSamplePermutation();
        permutationIterator++;
        nodes.push_back(node);
    }
    oram->setupInsert(&nodes);
}

int AVLTree::sortedArrayToBST(vector<Node*>* nodes, long long start, long long end, unsigned long long& pos, Bid& node) {
    if (start > end) {
        pos = -1;
        node = 0;
        return 0;
    }

    unsigned long long mid = (start + end) / 2;
    Node *root = (*nodes)[mid];

    int leftHeight = sortedArrayToBST(nodes, start, mid - 1, root->leftPos, root->leftID);

    int rightHeight = sortedArrayToBST(nodes, mid + 1, end, root->rightPos, root->rightID);
    root->pos = getSamplePermutation();
    root->height = max(leftHeight, rightHeight) + 1;
    pos = root->pos;
    node = root->key;
    return root->height;
}

vector<string> split(const string& str, const string& delim) {
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

void AVLTree::setSpt(Node* rootNode, Bid omapKey, string& res) {
    Bid curKey = rootNode->key;
    unsigned long long lastPos = rootNode->pos;
    unsigned long long newPos = RandomPath();
    rootNode->pos = newPos;
    Bid dumyID = oram->nextDummyCounter;
    Node* tmpDummyNode = new Node();
    tmpDummyNode->isDummy = true;
    tmpDummyNode->pos = RandomPath();
    std::array< byte_t, 16> resVec;
    Node* head;
    int dummyState = 0;
    int upperBound = (int) (1.44 * oram->depth);
    bool found = false;
    unsigned long long dumyPos = RandomPath();

    do {
        unsigned long long rnd = RandomPath();
        unsigned long long rnd2 = RandomPath();
        bool isDummyAction = Node::CTeq(Node::CTcmp(dummyState, 1), 0);
        head = oram->ReadWrite(curKey, lastPos, newPos, isDummyAction, rnd2, omapKey);

        bool cond1 = Node::CTeq(Node::CTcmp(dummyState, 1), 0);
        bool cond2 = Node::CTeq(Bid::CTcmp(head->key, omapKey), 1);
        bool cond3 = Node::CTeq(Bid::CTcmp(head->key, omapKey), -1);
        bool cond4 = Node::CTeq(Bid::CTcmp(head->key, omapKey), 0);

        lastPos = Node::conditional_select(rnd, lastPos, cond1 || (!cond1 && ((cond2 && head->leftID.isZero()) || (cond3 && head->rightID.isZero()))));
        lastPos = Node::conditional_select(head->leftPos, lastPos, !cond1 && cond2);
        lastPos = Node::conditional_select(head->rightPos, lastPos, !cond1 && !cond2 && cond3);

        curKey = dumyID;
        for (int k = 0; k < curKey.id.size(); k++) {
            curKey.id[k] = Node::conditional_select(head->leftID.id[k], curKey.id[k], !cond1 && cond2 && !head->leftID.isZero());
            curKey.id[k] = Node::conditional_select(head->rightID.id[k], curKey.id[k], !cond1 && !cond2 && cond3 && !head->rightID.isZero());
        }

        newPos = Node::conditional_select(rnd, newPos, cond1);
        newPos = Node::conditional_select(rnd2, newPos, !cond1 && cond2 && !head->leftID.isZero());
        newPos = Node::conditional_select(rnd2, newPos, !cond1 && !cond2 && cond3 && !head->rightID.isZero());

        for (int i = 0; i < 16; i++) {
            resVec[i] = Bid::conditional_select(head->value[i], resVec[i], !cond1);
        }

        string tmpRes = string(head->value.begin(), head->value.end());
        vector<string> parts = split(tmpRes, "-");
        int spt = 0;
        if (!cond1 && !cond2 && !cond3 && cond4) {
            spt = stoi(parts[1]);
        }
        string newval = parts[0] + "-" + to_string(spt + 1);
        std::array< byte_t, 16> newVec;
        std::fill(newVec.begin(), newVec.end(), 0);
        std::copy(newval.begin(), newval.end(), newVec.begin());

        dummyState = Node::conditional_select(dummyState + 1, dummyState, !cond1 && !cond2 && !cond3 && cond4 || (!cond1 && ((cond2 && head->leftID.isZero()) || (cond3 && head->rightID.isZero()))));

        for (int k = 0; k < head->key.id.size(); k++) {
            head->key.id[k] = Node::conditional_select(dumyID.id[k], head->key.id[k], cond1);
        }
        for (int i = 0; i < 16; i++) {
            head->value[i] = Bid::conditional_select(newVec[i], head->value[i], !cond1 && !cond2 && !cond3 && cond4);
        }
        found = Node::conditional_select(true, found, !cond1 && !cond2 && !cond3 && cond4);
    } while (!found || oram->readCnt <= upperBound);
    delete tmpDummyNode;
    res.assign(resVec.begin(), resVec.end());

    //    if (head == NULL || head->key == 0)
    //        return NULL;
    //    int lastPos = head->pos;
    //    if (newPos == -1) {
    //        head->pos = RandomPath();
    //        if (head->key != omapKey) {
    //            head = oram->ReadNode(head->key, lastPos, head->pos, true);
    //        } else {
    //            head = oram->ReadNode(head->key, lastPos, head->pos, true);
    //        }
    //    }
    //
    //    if (head->key > omapKey) {
    //        lastPos = head->leftPos;
    //        head->leftPos = RandomPath();
    //        Bid leftID = head->leftID;
    //        int leftPos = head->leftPos;
    //        oram->WriteNode(head->key, head);
    //        if (leftID == omapKey) {
    //            return setSpt(oram->ReadNode(leftID, lastPos, leftPos, true), omapKey, res, leftPos);
    //        } else {
    //            return setSpt(oram->ReadNode(leftID, lastPos, leftPos, true), omapKey, res, leftPos);
    //        }
    //    } else if (head->key < omapKey) {
    //        lastPos = head->rightPos;
    //        head->rightPos = RandomPath();
    //        Bid rightID = head->rightID;
    //        int rightPos = head->rightPos;
    //        oram->WriteNode(head->key, head);
    //        if (rightID == omapKey) {
    //            return setSpt(oram->ReadNode(rightID, lastPos, rightPos, true), omapKey, res, rightPos);
    //        } else {
    //            return setSpt(oram->ReadNode(rightID, lastPos, rightPos, true), omapKey, res, rightPos);
    //        }
    //    } else {
    //        res = string(head->value.begin(), head->value.end());
    //        vector<string> parts = split(res, "-");
    //        int spt = stoi(parts[1]);
    //        string newval = parts[0] + "-" + to_string(spt + 1);
    //        std::fill(head->value.begin(), head->value.end(), 0);
    //        std::copy(newval.begin(), newval.end(), head->value.begin());
    //        oram->WriteNode(head->key, head);
    //        return head;
    //    }
}

void AVLTree::readAndSetDist(Node* rootNode, Bid omapKey, string& res, string newValue) {
    Bid curKey = rootNode->key;
    unsigned long long lastPos = rootNode->pos;
    unsigned long long newPos = RandomPath();
    rootNode->pos = newPos;
    Bid dumyID = oram->nextDummyCounter;
    Node* tmpDummyNode = new Node();
    tmpDummyNode->isDummy = true;
    tmpDummyNode->pos = RandomPath();
    std::array< byte_t, 16> resVec;
    Node* head;
    int dummyState = 0;
    int upperBound = (int) (1.44 * oram->depth);
    bool found = false;
    unsigned long long dumyPos = RandomPath();

    do {
        unsigned long long rnd = RandomPath();
        unsigned long long rnd2 = RandomPath();
        bool isDummyAction = Node::CTeq(Node::CTcmp(dummyState, 1), 0);
        head = oram->ReadWrite(curKey, lastPos, newPos, isDummyAction, rnd2, omapKey);

        bool cond1 = Node::CTeq(Node::CTcmp(dummyState, 1), 0);
        bool cond2 = Node::CTeq(Bid::CTcmp(head->key, omapKey), 1);
        bool cond3 = Node::CTeq(Bid::CTcmp(head->key, omapKey), -1);
        bool cond4 = Node::CTeq(Bid::CTcmp(head->key, omapKey), 0);

        lastPos = Node::conditional_select(rnd, lastPos, cond1 || (!cond1 && ((cond2 && head->leftID.isZero()) || (cond3 && head->rightID.isZero()))));
        lastPos = Node::conditional_select(head->leftPos, lastPos, !cond1 && cond2);
        lastPos = Node::conditional_select(head->rightPos, lastPos, !cond1 && !cond2 && cond3);

        curKey = dumyID;
        for (int k = 0; k < curKey.id.size(); k++) {
            curKey.id[k] = Node::conditional_select(head->leftID.id[k], curKey.id[k], !cond1 && cond2 && !head->leftID.isZero());
            curKey.id[k] = Node::conditional_select(head->rightID.id[k], curKey.id[k], !cond1 && !cond2 && cond3 && !head->rightID.isZero());
        }

        newPos = Node::conditional_select(rnd, newPos, cond1);
        newPos = Node::conditional_select(rnd2, newPos, !cond1 && cond2 && !head->leftID.isZero());
        newPos = Node::conditional_select(rnd2, newPos, !cond1 && !cond2 && cond3 && !head->rightID.isZero());

        for (int i = 0; i < 16; i++) {
            resVec[i] = Bid::conditional_select(head->value[i], resVec[i], !cond1);
        }

        string tmpRes = string(head->value.begin(), head->value.end());
        vector<string> parts = split(tmpRes, "-");
        string newval = parts[0] + "-" + newValue;
        std::array< byte_t, 16> newVec;
        std::fill(newVec.begin(), newVec.end(), 0);
        std::copy(newval.begin(), newval.end(), newVec.begin());

        dummyState = Node::conditional_select(dummyState + 1, dummyState, !cond1 && !cond2 && !cond3 && cond4 || (!cond1 && ((cond2 && head->leftID.isZero()) || (cond3 && head->rightID.isZero()))));

        for (int k = 0; k < head->key.id.size(); k++) {
            head->key.id[k] = Node::conditional_select(dumyID.id[k], head->key.id[k], cond1);
        }
        for (int i = 0; i < 16; i++) {
            head->value[i] = Bid::conditional_select(newVec[i], head->value[i], !cond1 && !cond2 && !cond3 && cond4);
        }
        found = Node::conditional_select(true, found, !cond1 && !cond2 && !cond3 && cond4);
    } while (!found || oram->readCnt <= upperBound);
    delete tmpDummyNode;
    res.assign(resVec.begin(), resVec.end());


    //    if (head == NULL || head->key == 0)
    //        return NULL;
    //    int lastPos = head->pos;
    //    if (newPos == -1) {
    //        head->pos = RandomPath();
    //        if (head->key != omapKey) {
    //            head = oram->ReadNode(head->key, lastPos, head->pos, true);
    //        } else {
    //            head = oram->ReadNode(head->key, lastPos, head->pos, true);
    //        }
    //    }
    //
    //    if (head->key > omapKey) {
    //        lastPos = head->leftPos;
    //        head->leftPos = RandomPath();
    //        Bid leftID = head->leftID;
    //        int leftPos = head->leftPos;
    //        oram->WriteNode(head->key, head);
    //        if (leftID == omapKey) {
    //            return readAndSetDist(oram->ReadNode(leftID, lastPos, leftPos, true), omapKey, res, newValue, leftPos);
    //        } else {
    //            return readAndSetDist(oram->ReadNode(leftID, lastPos, leftPos, true), omapKey, res, newValue, leftPos);
    //        }
    //    } else if (head->key < omapKey) {
    //        lastPos = head->rightPos;
    //        head->rightPos = RandomPath();
    //        Bid rightID = head->rightID;
    //        int rightPos = head->rightPos;
    //        oram->WriteNode(head->key, head);
    //        if (rightID == omapKey) {
    //            return readAndSetDist(oram->ReadNode(rightID, lastPos, rightPos, true), omapKey, res, newValue, rightPos);
    //        } else {
    //            return readAndSetDist(oram->ReadNode(rightID, lastPos, rightPos, true), omapKey, res, newValue, rightPos);
    //        }
    //    } else {
    //        res = string(head->value.begin(), head->value.end());
    //        vector<string> parts = split(res, "-");
    //        string newval = parts[0] + "-" + newValue;
    //        std::fill(head->value.begin(), head->value.end(), 0);
    //        std::copy(newval.begin(), newval.end(), head->value.begin());
    //        oram->WriteNode(head->key, head);
    //        return head;
    //    }
}

unsigned long long buckCoutner = 0;

unsigned long long AVLTree::getSamplePermutation() {
    if (buckCoutner == Z) {
        PRPCnt++;
        buckCoutner = 1;
    } else {
        buckCoutner++;
    }

    unsigned long long k = PRPCnt % (maxOfRandom);
    return k;
}

Node* AVLTree::readWriteCacheNode(Bid bid, Node* inputnode, bool isRead, bool isDummy) {
    Node* tmpWrite = Node::clone(inputnode);
    bool found = false;

    Node* res = new Node();
    res->isDummy = true;
    res->index = oram->nextDummyCounter++;
    res->key = oram->nextDummyCounter++;
    bool write = !isRead;

    for (Node* node : avlCache) {
        bool match = Node::CTeq(Bid::CTcmp(node->key, bid), 0) && !node->isDummy;

        node->isDummy = Node::conditional_select(true, node->isDummy, !isDummy && match && write);
        bool choice = !isDummy && match && isRead && !node->isDummy;
        res->index = Node::conditional_select((long long) node->index, (long long) res->index, choice);
        res->isDummy = Node::conditional_select(node->isDummy, res->isDummy, choice);
        res->pos = Node::conditional_select((long long) node->pos, (long long) res->pos, choice);
        for (int k = 0; k < res->value.size(); k++) {
            res->value[k] = Node::conditional_select(node->value[k], res->value[k], choice);
        }
        for (int k = 0; k < res->dum.size(); k++) {
            res->dum[k] = Node::conditional_select(node->dum[k], res->dum[k], choice);
        }
        res->evictionNode = Node::conditional_select(node->evictionNode, res->evictionNode, choice);
        res->modified = Node::conditional_select(true, res->modified, choice);
        res->height = Node::conditional_select(node->height, res->height, choice);
        res->leftPos = Node::conditional_select(node->leftPos, res->leftPos, choice);
        res->rightPos = Node::conditional_select(node->rightPos, res->rightPos, choice);
        for (int k = 0; k < res->key.id.size(); k++) {
            res->key.id[k] = Node::conditional_select(node->key.id[k], res->key.id[k], choice);
        }
        for (int k = 0; k < res->leftID.id.size(); k++) {
            res->leftID.id[k] = Node::conditional_select(node->leftID.id[k], res->leftID.id[k], choice);
        }
        for (int k = 0; k < res->rightID.id.size(); k++) {
            res->rightID.id[k] = Node::conditional_select(node->rightID.id[k], res->rightID.id[k], choice);
        }
    }
    //    if (!found && isRead && !isDummy) {
    //        printf("not found in cache %lld\n", bid.getValue());
    //    }

    avlCache.push_back(tmpWrite);
    return res;
}