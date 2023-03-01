#include "AVLTree.h"
#include "Enclave.h"

void check_memory2(string text) {
    unsigned int required = 0x4f00000; // adapt to native uint
    char *mem = NULL;
    while (mem == NULL) {
        mem = (char*) malloc(required);
        if ((required -= 8) < 0xFFF) {
            if (mem) free(mem);
            printf("Cannot allocate enough memory\n");
            return;
        }
    }

    free(mem);
    mem = (char*) malloc(required);
    if (mem == NULL) {
        printf("Cannot enough allocate memory\n");
        return;
    }
    printf("%s = %d\n", text.c_str(), required);
    free(mem);
}

AVLTree::AVLTree(long long maxSize, bytes<Key> secretkey, bool isEmptyMap) {
    oram = new ORAM(maxSize, secretkey, false, isEmptyMap);
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
    node->leftPos = -1;
    node->rightPos = -1;
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

    int oppositeOppositeHeight = 0;
    unsigned long long newOppositePos = RandomPath();
    Node* oppositeOppositeNode = nullptr;

    cond1 = !dummyOp && right && !oppositeNode->leftID.isZero();
    cond2 = !dummyOp && left && !oppositeNode->rightID.isZero();

    readKey = dummy;
    readKey = Bid::conditional_select(oppositeNode->leftID, readKey, cond1);
    readKey = Bid::conditional_select(oppositeNode->rightID, readKey, !cond1 && cond2);

    isDummy = !(cond1 || cond2);

    oppositeOppositeNode = readWriteCacheNode(readKey, tmpDummyNode, true, isDummy); //READ

    oppositeOppositeHeight = Node::conditional_select(oppositeOppositeNode->height, oppositeOppositeHeight, cond1 || cond2);
    delete oppositeOppositeNode;


    int maxValue = max(oppositeOppositeHeight, curNodeHeight) + 1;
    oppositeNode->height = Node::conditional_select(maxValue, oppositeNode->height, !dummyOp);

    delete T2;
    delete tmpDummyNode;
}

Bid AVLTree::insert(Bid rootKey, unsigned long long& rootPos, Bid omapKey, string value, int& height, Bid lastID, bool isDummyIns) {
    totheight++;
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

    if (isDummyIns && CTeq(CTcmp(totheight, oram->depth * 1.44), 1)) {
        Node* nnode = newNode(omapKey, value);
        nnode->pos = RandomPath();
        height = Node::conditional_select(nnode->height, height, !exist);
        rootPos = Node::conditional_select(nnode->pos, rootPos, !exist);

        Node* previousNode = readWriteCacheNode(omapKey, tmpDummyNode, true, !exist);

        Node* wrtNode = Node::clone(previousNode);
        Node::conditional_assign(wrtNode, nnode, !exist);

        unsigned long long lastPos = nnode->pos;
        lastPos = Node::conditional_select(previousNode->pos, lastPos, exist);

        Node* tmp = oram->ReadWrite(omapKey, wrtNode, lastPos, nnode->pos, false, false, false);

        wrtNode->pos = nnode->pos;
        Node* tmp2 = readWriteCacheNode(omapKey, wrtNode, false, false);
        Bid retKey = lastID;
        retKey = Bid::conditional_select(nnode->key, retKey, !exist);
        delete tmp;
        delete tmp2;
        delete wrtNode;
        delete nnode;
        delete previousNode;
        delete tmpDummyNode;
        return retKey;
    }
    /* 1. Perform the normal BST rotation */
    double t;
    bool cond1, cond2, cond3, cond4, cond5, isDummy;

    cond1 = !isDummyIns && !rootKey.isZero();
    remainerIsDummy = !cond1;

    Node* node = nullptr;

    Bid initReadKey = dummy;
    initReadKey = Bid::conditional_select(rootKey, initReadKey, !remainerIsDummy);
    isDummy = remainerIsDummy;
    node = oram->ReadWrite(initReadKey, tmpDummyNode, rootPos, rootPos, true, isDummy, tmpval, Bid::CTeq(Bid::CTcmp(rootKey, omapKey), 0), true); //READ
    Node* tmp2 = readWriteCacheNode(initReadKey, node, false, isDummy);
    delete tmp2;

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

    cond1 = !remainerIsDummy;
    int cmpRes = Bid::CTcmp(omapKey, node->key);
    cond2 = Node::CTeq(cmpRes, -1);
    bool cond2_1 = node->rightID.isZero();
    cond3 = Node::CTeq(cmpRes, 1);
    bool cond3_1 = node->leftID.isZero();
    cond4 = Node::CTeq(cmpRes, 0);

    Bid insRootKey = rootKey;
    insRootKey = Bid::conditional_select(node->leftID, insRootKey, cond1 && cond2);
    insRootKey = Bid::conditional_select(node->rightID, insRootKey, cond1 && cond3);

    unsigned long long insRootPos = rootPos;
    insRootPos = Node::conditional_select(node->leftPos, insRootPos, cond1 && cond2);
    insRootPos = Node::conditional_select(node->rightPos, insRootPos, cond1 && cond3);

    int insHeight = height;
    insHeight = Node::conditional_select(leftHeight, insHeight, cond1 && cond2);
    insHeight = Node::conditional_select(rightHeight, insHeight, cond1 && cond3);

    Bid insLastID = dummy;
    insLastID = Bid::conditional_select(node->key, insLastID, (cond1 && cond4) || (!cond1));

    isDummy = (!cond1) || (cond1 && cond4);

    exist = Node::conditional_select(true, exist, cond1 && cond4);
    Bid resValBid = insert(insRootKey, insRootPos, omapKey, value, insHeight, insLastID, isDummy);


    node->leftID = Bid::conditional_select(resValBid, node->leftID, cond1 && cond2);
    node->rightID = Bid::conditional_select(resValBid, node->rightID, cond1 && cond3);

    node->leftPos = Node::conditional_select(insRootPos, node->leftPos, cond1 && cond2);
    node->rightPos = Node::conditional_select(insRootPos, node->rightPos, cond1 && cond3);
    rootPos = Node::conditional_select(insRootPos, rootPos, (!cond1) || (cond1 && cond4));

    leftHeight = Node::conditional_select(insHeight, leftHeight, cond1 && cond2);
    rightHeight = Node::conditional_select(insHeight, rightHeight, cond1 && cond3);
    height = Node::conditional_select(insHeight, height, (!cond1) || (cond1 && cond4));

    totheight--;
    childDirisLeft = (cond1 && cond2);

    Bid readKey = dummy;
    readKey = Bid::conditional_select(node->rightID, readKey, cond1 && cond2 && (!cond2_1));
    readKey = Bid::conditional_select(node->leftID, readKey, cond1 && cond3 && (!cond3_1));

    unsigned long long tmpreadPos = newRLPos;
    tmpreadPos = Bid::conditional_select(node->rightPos, tmpreadPos, cond1 && cond2 && (!cond2_1));
    tmpreadPos = Bid::conditional_select(node->leftPos, tmpreadPos, cond1 && cond3 && (!cond3_1));

    isDummy = !((cond1 && cond2 && (!cond2_1)) || (cond1 && cond3 && (!cond3_1)));

    Node* tmp = oram->ReadWrite(readKey, tmpDummyNode, tmpreadPos, tmpreadPos, true, isDummy, true); //READ
    Node::conditional_assign(rightNode, tmp, cond1 && cond2 && (!cond2_1));
    Node::conditional_assign(leftNode, tmp, cond1 && cond3 && (!cond3_1));

    tmp2 = readWriteCacheNode(tmp->key, tmp, false, isDummy);

    delete tmp;
    delete tmp2;

    tmp = readWriteCacheNode(node->key, tmpDummyNode, true, !(cond1 && cond4));

    Node::conditional_assign(node, tmp, cond1 && cond4);

    delete tmp;

    rightNodeisNull = Node::conditional_select(false, rightNodeisNull, cond1 && cond2 && (!cond2_1));
    leftNodeisNull = Node::conditional_select(false, leftNodeisNull, cond1 && cond3 && (!cond3_1));

    rightHeight = Node::conditional_select(0, rightHeight, cond1 && cond2 && cond2_1);
    rightHeight = Node::conditional_select(rightNode->height, rightHeight, cond1 && cond2 && (!cond2_1));
    leftHeight = Node::conditional_select(0, leftHeight, cond1 && cond3 && cond3_1);
    leftHeight = Node::conditional_select(leftNode->height, leftHeight, cond1 && cond3 && (!cond3_1));

    std::fill(garbage.begin(), garbage.end(), 0);
    std::copy(value.begin(), value.end(), garbage.begin());

    for (int i = 0; i < 16; i++) {
        node->value[i] = Bid::conditional_select(garbage[i], node->value[i], cond1 && cond4);
    }

    rootPos = Node::conditional_select(node->pos, rootPos, cond1 && cond4);
    height = Node::conditional_select(node->height, height, cond1 && cond4);

    retKey = Bid::conditional_select(node->key, retKey, cond1 && cond4);
    retKey = Bid::conditional_select(resValBid, retKey, !cond1);

    remainerIsDummy = Node::conditional_select(true, remainerIsDummy, cond1 && cond4);
    remainerIsDummy = Node::conditional_select(false, remainerIsDummy, cond1 && (cond2 || cond3));

    /* 2. Update height of this ancestor node */

    int tmpHeight = max(leftHeight, rightHeight) + 1;
    node->height = Node::conditional_select(node->height, tmpHeight, remainerIsDummy);
    height = Node::conditional_select(height, node->height, remainerIsDummy);

    /* 3. Get the balance factor of this ancestor node to check whether this node became unbalanced */

    balance = Node::conditional_select(balance, leftHeight - rightHeight, remainerIsDummy);

    //------------------------------------------------

    cond1 = !remainerIsDummy && CTeq(CTcmp(balance, 1), 1) && Node::CTeq(Bid::CTcmp(omapKey, node->leftID), -1);
    cond2 = !remainerIsDummy && CTeq(CTcmp(balance, -1), -1) && Node::CTeq(Bid::CTcmp(omapKey, node->rightID), 1);
    cond3 = !remainerIsDummy && CTeq(CTcmp(balance, 1), 1) && Node::CTeq(Bid::CTcmp(omapKey, node->leftID), 1);
    cond4 = !remainerIsDummy && CTeq(CTcmp(balance, -1), -1) && Node::CTeq(Bid::CTcmp(omapKey, node->rightID), -1);
    cond5 = !remainerIsDummy;

    //  printf("tot:%d cond1:%d cond2:%d cond3:%d cond4:%d cond5:%d\n",totheight,cond1,cond2,cond3,cond4,cond5);

    //------------------------------------------------
    // Left/Right Node
    //------------------------------------------------

    Bid leftRightReadKey = dummy;
    leftRightReadKey = Bid::conditional_select(node->leftID, leftRightReadKey, (cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull);
    leftRightReadKey = Bid::conditional_select(node->rightID, leftRightReadKey, ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull);

    isDummy = !(((cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull) || (((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull));

    tmp = readWriteCacheNode(leftRightReadKey, tmpDummyNode, true, isDummy); //READ
    Node::conditional_assign(leftNode, tmp, (cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull);
    Node::conditional_assign(rightNode, tmp, ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull);
    delete tmp;

    node->leftPos = Node::conditional_select(leftNode->pos, node->leftPos, (cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull);
    leftNodeisNull = Node::conditional_select(false, leftNodeisNull, (cond1 || (!cond1 && !cond2 && cond3)) && leftNodeisNull);

    node->rightPos = Node::conditional_select(rightNode->pos, node->rightPos, ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull);
    rightNodeisNull = Node::conditional_select(false, rightNodeisNull, ((!cond1 && cond2) || (!cond1 & !cond2 && !cond3 && cond4)) && rightNodeisNull);
    //------------------------------------------------
    // Left-Right Right-Left Node
    //------------------------------------------------

    Bid leftRightNodeReadKey = dummy;
    leftRightNodeReadKey = Bid::conditional_select(leftNode->rightID, leftRightNodeReadKey, !cond1 && !cond2 && cond3);
    leftRightNodeReadKey = Bid::conditional_select(rightNode->leftID, leftRightNodeReadKey, !cond1 && !cond2 && !cond3 && cond4);

    isDummy = !((!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4));

    tmp = readWriteCacheNode(leftRightNodeReadKey, tmpDummyNode, true, isDummy); //READ

    Node* leftRightNode = new Node();
    Node* rightLeftNode = new Node();

    Node::conditional_assign(leftRightNode, tmp, (!cond1 && !cond2 && cond3));
    Node::conditional_assign(rightLeftNode, tmp, (!cond1 && !cond2 && !cond3 && cond4));

    delete tmp;

    int leftLeftHeight = 0;
    int rightRightHeight = 0;

    Bid leftLeftRightRightKey = dummy;
    leftLeftRightRightKey = Bid::conditional_select(leftNode->leftID, leftLeftRightRightKey, (!cond1 && !cond2 && cond3) && !leftNode->leftID.isZero());
    leftLeftRightRightKey = Bid::conditional_select(rightNode->rightID, leftLeftRightRightKey, (!cond1 && !cond2 && !cond3 && cond4) && !rightNode->rightID.isZero());

    isDummy = !(((!cond1 && !cond2 && cond3) && !leftNode->leftID.isZero()) || ((!cond1 && !cond2 && !cond3 && cond4) && !rightNode->rightID.isZero()));

    tmp = readWriteCacheNode(leftLeftRightRightKey, tmpDummyNode, true, isDummy); //READ

    Node* leftLeftNode = new Node();
    Node* rightRightNode = new Node();

    Node::conditional_assign(leftLeftNode, tmp, (!cond1 && !cond2 && cond3) && !leftNode->leftID.isZero());
    Node::conditional_assign(rightRightNode, tmp, (!cond1 && !cond2 && !cond3 && cond4) && !rightNode->rightID.isZero());
    delete tmp;

    leftLeftHeight = Node::conditional_select(leftLeftNode->height, leftLeftHeight, (!cond1 && !cond2 && cond3) && !leftNode->leftID.isZero());
    rightRightHeight = Node::conditional_select(rightRightNode->height, rightRightHeight, (!cond1 && !cond2 && !cond3 && cond4) && !rightNode->rightID.isZero());
    delete leftLeftNode;
    delete rightRightNode;

    //------------------------------------------------
    // Rotate
    //------------------------------------------------

    Node* targetRotateNode = Node::clone(tmpDummyNode);
    Node* oppositeRotateNode = Node::clone(tmpDummyNode);

    Node::conditional_assign(targetRotateNode, leftNode, !cond1 && !cond2 && cond3);
    Node::conditional_assign(targetRotateNode, rightNode, !cond1 && !cond2 && !cond3 && cond4);

    Node::conditional_assign(oppositeRotateNode, leftRightNode, !cond1 && !cond2 && cond3);
    Node::conditional_assign(oppositeRotateNode, rightLeftNode, !cond1 && !cond2 && !cond3 && cond4);

    int rotateHeight = 0;
    rotateHeight = Node::conditional_select(leftLeftHeight, rotateHeight, !cond1 && !cond2 && cond3);
    rotateHeight = Node::conditional_select(rightRightHeight, rotateHeight, !cond1 && !cond2 && !cond3 && cond4);

    bool isRightRotate = !cond1 && !cond2 && !cond3 && cond4;
    bool isDummyRotate = !((!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4));

    rotate(targetRotateNode, oppositeRotateNode, rotateHeight, isRightRotate, isDummyRotate);

    Node::conditional_assign(leftNode, targetRotateNode, !cond1 && !cond2 && cond3);
    Node::conditional_assign(rightNode, targetRotateNode, !cond1 && !cond2 && !cond3 && cond4);

    delete targetRotateNode;

    Node::conditional_assign(leftRightNode, oppositeRotateNode, !cond1 && !cond2 && cond3);
    Node::conditional_assign(rightLeftNode, oppositeRotateNode, !cond1 && !cond2 && !cond3 && cond4);

    delete oppositeRotateNode;


    unsigned long long newP = RandomPath();
    unsigned long long oldLeftRightPos = RandomPath();
    oldLeftRightPos = Node::conditional_select(leftNode->pos, oldLeftRightPos, !cond1 && !cond2 && cond3);
    oldLeftRightPos = Node::conditional_select(rightNode->pos, oldLeftRightPos, !cond1 && !cond2 && !cond3 && cond4);

    leftNode->pos = Node::conditional_select(newP, leftNode->pos, !cond1 && !cond2 && cond3);
    rightNode->pos = Node::conditional_select(newP, rightNode->pos, !cond1 && !cond2 && !cond3 && cond4);

    Bid firstRotateWriteKey = dummy;
    firstRotateWriteKey = Bid::conditional_select(leftNode->key, firstRotateWriteKey, !cond1 && !cond2 && cond3);
    firstRotateWriteKey = Bid::conditional_select(rightNode->key, firstRotateWriteKey, !cond1 && !cond2 && !cond3 && cond4);

    Node* firstRotateWriteNode = Node::clone(tmpDummyNode);
    Node::conditional_assign(firstRotateWriteNode, leftNode, !cond1 && !cond2 && cond3);
    Node::conditional_assign(firstRotateWriteNode, rightNode, !cond1 && !cond2 && !cond3 && cond4);

    isDummy = !((!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4));
    tmp = readWriteCacheNode(firstRotateWriteKey, firstRotateWriteNode, false, isDummy); //WRITE
    delete tmp;
    delete firstRotateWriteNode;

    leftRightNode->leftPos = Node::conditional_select(newP, leftRightNode->leftPos, !cond1 && !cond2 && cond3);
    rightLeftNode->rightPos = Node::conditional_select(newP, rightLeftNode->rightPos, !cond1 && !cond2 && !cond3 && cond4);

    node->leftID = Bid::conditional_select(leftRightNode->key, node->leftID, !cond1 && !cond2 && cond3);
    node->leftPos = Node::conditional_select(leftRightNode->pos, node->leftPos, !cond1 && !cond2 && cond3);

    node->rightID = Bid::conditional_select(rightLeftNode->key, node->rightID, !cond1 && !cond2 && !cond3 && cond4);
    node->rightPos = Node::conditional_select(rightLeftNode->pos, node->rightPos, !cond1 && !cond2 && !cond3 && cond4);


    //------------------------------------------------
    // Second Rotate
    //------------------------------------------------

    targetRotateNode = Node::clone(tmpDummyNode);

    Node::conditional_assign(targetRotateNode, node, cond1 || cond2 || cond3 || cond4);

    oppositeRotateNode = Node::clone(tmpDummyNode);
    Node::conditional_assign(oppositeRotateNode, leftNode, cond1);
    Node::conditional_assign(oppositeRotateNode, rightNode, !cond1 && cond2);
    Node::conditional_assign(oppositeRotateNode, leftRightNode, !cond1 && !cond2 && cond3);
    Node::conditional_assign(oppositeRotateNode, rightLeftNode, !cond1 && !cond2 && !cond3 && cond4);

    rotateHeight = 0;
    rotateHeight = Node::conditional_select(rightHeight, rotateHeight, cond1 || (!cond1 && !cond2 && cond3));
    rotateHeight = Node::conditional_select(leftHeight, rotateHeight, (!cond1 && cond2) || (!cond1 && !cond2 && !cond3 && cond4));

    isRightRotate = cond1 || (!cond1 && !cond2 && cond3);
    isDummyRotate = !(cond1 || cond2 || cond3 || cond4);

    rotate(targetRotateNode, oppositeRotateNode, rotateHeight, isRightRotate, isDummyRotate);

    Node::conditional_assign(node, targetRotateNode, cond1 || cond2 || cond3 || cond4);

    delete targetRotateNode;

    Node::conditional_assign(leftNode, oppositeRotateNode, cond1);
    Node::conditional_assign(rightNode, oppositeRotateNode, !cond1 && cond2);
    Node::conditional_assign(leftRightNode, oppositeRotateNode, !cond1 && !cond2 && cond3);
    Node::conditional_assign(rightLeftNode, oppositeRotateNode, !cond1 && !cond2 && !cond3 && cond4);

    delete oppositeRotateNode;

    //------------------------------------------------
    // Last Two Writes
    //------------------------------------------------

    newP = RandomPath();

    Bid firstReadCache = dummy;
    firstReadCache = Bid::conditional_select(node->leftID, firstReadCache, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && childDirisLeft);
    firstReadCache = Bid::conditional_select(node->rightID, firstReadCache, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && !childDirisLeft);

    isDummy = !(!cond1 && !cond2 && !cond3 && !cond4 && doubleRotation);
    tmp = readWriteCacheNode(firstReadCache, tmpDummyNode, true, isDummy);

    Node::conditional_assign(leftNode, tmp, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && childDirisLeft);
    Node::conditional_assign(rightNode, tmp, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && !childDirisLeft);

    delete tmp;

    Bid finalFirstWriteKey = dummy;
    finalFirstWriteKey = Bid::conditional_select(node->key, finalFirstWriteKey, cond1 || (!cond1 && cond2));
    finalFirstWriteKey = Bid::conditional_select(leftNode->key, finalFirstWriteKey, (!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && childDirisLeft));
    finalFirstWriteKey = Bid::conditional_select(rightNode->key, finalFirstWriteKey, (!cond1 && !cond2 && !cond3 && cond4) || (!cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && !childDirisLeft));

    Node* finalFirstWriteNode = Node::clone(tmpDummyNode);
    Node::conditional_assign(finalFirstWriteNode, node, cond1 || (!cond1 && cond2));
    Node::conditional_assign(finalFirstWriteNode, leftNode, (!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && childDirisLeft));
    Node::conditional_assign(finalFirstWriteNode, rightNode, (!cond1 && !cond2 && !cond3 && cond4) || (!cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && !childDirisLeft));


    unsigned long long finalFirstWritePos = dumyPos;
    finalFirstWritePos = Node::conditional_select(node->pos, finalFirstWritePos, cond1 || (!cond1 && cond2));
    finalFirstWritePos = Node::conditional_select(oldLeftRightPos, finalFirstWritePos, ((!cond1 && !cond2 && cond3)) || ((!cond1 && !cond2 && !cond3 && cond4)));
    finalFirstWritePos = Node::conditional_select(leftNode->pos, finalFirstWritePos, (!cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && childDirisLeft));
    finalFirstWritePos = Node::conditional_select(rightNode->pos, finalFirstWritePos, (!cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && !childDirisLeft));

    unsigned long long finalFirstWriteNewPos = dumyPos;
    finalFirstWriteNewPos = Node::conditional_select(newP, finalFirstWriteNewPos, cond1 || (!cond1 && cond2) || (!cond1 && !cond2 && !cond3 && !cond4 && doubleRotation));
    finalFirstWriteNewPos = Node::conditional_select(leftNode->pos, finalFirstWriteNewPos, (!cond1 && !cond2 && cond3));
    finalFirstWriteNewPos = Node::conditional_select(rightNode->pos, finalFirstWriteNewPos, (!cond1 && !cond2 && !cond3 && cond4));

    isDummy = !cond1 && !cond2 && !cond3 && !cond4 && !doubleRotation;

    tmp = oram->ReadWrite(finalFirstWriteKey, finalFirstWriteNode, finalFirstWritePos, finalFirstWriteNewPos, false, isDummy, false); //WRITE        

    delete tmp;
    delete finalFirstWriteNode;


    node->pos = Node::conditional_select(newP, node->pos, cond1 || (!cond1 && cond2));
    leftNode->rightPos = Node::conditional_select(newP, leftNode->rightPos, cond1);
    rightNode->leftPos = Node::conditional_select(newP, rightNode->leftPos, !cond1 && cond2);
    leftNode->pos = Node::conditional_select(newP, leftNode->pos, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && childDirisLeft);
    rightNode->pos = Node::conditional_select(newP, rightNode->pos, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && !childDirisLeft);
    node->leftPos = Node::conditional_select(newP, node->leftPos, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && childDirisLeft);
    node->rightPos = Node::conditional_select(newP, node->rightPos, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && !childDirisLeft);



    Bid finalFirstWriteCacheWriteKey = dummy;
    finalFirstWriteCacheWriteKey = Bid::conditional_select(node->key, finalFirstWriteCacheWriteKey, cond1 || (!cond1 && cond2));
    finalFirstWriteCacheWriteKey = Bid::conditional_select(node->leftID, finalFirstWriteCacheWriteKey, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && childDirisLeft);
    finalFirstWriteCacheWriteKey = Bid::conditional_select(node->rightID, finalFirstWriteCacheWriteKey, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && !childDirisLeft);

    Node* finalFirstWriteCacheWrite = Node::clone(tmpDummyNode);
    Node::conditional_assign(finalFirstWriteCacheWrite, node, cond1 || (!cond1 && cond2));
    Node::conditional_assign(finalFirstWriteCacheWrite, leftNode, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && childDirisLeft);
    Node::conditional_assign(finalFirstWriteCacheWrite, rightNode, !cond1 && !cond2 && !cond3 && !cond4 && doubleRotation && !childDirisLeft);

    isDummy = (!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4) || (!cond1 && !cond2 && !cond3 && !cond4 && !doubleRotation);

    tmp = readWriteCacheNode(finalFirstWriteCacheWriteKey, finalFirstWriteCacheWrite, false, isDummy); //WRITE

    delete tmp;
    delete finalFirstWriteCacheWrite;

    doubleRotation = Node::conditional_select(false, doubleRotation, doubleRotation && (!cond1 && !cond2 && !cond3 && !cond4));
    doubleRotation = Node::conditional_select(true, doubleRotation, (!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4));
    //------------------------------------------------
    // Last Write
    //------------------------------------------------

    newP = RandomPath();

    Bid finalWriteKey = dummy;
    finalWriteKey = Bid::conditional_select(leftNode->key, finalWriteKey, cond1);
    finalWriteKey = Bid::conditional_select(rightNode->key, finalWriteKey, !cond1 && cond2);
    finalWriteKey = Bid::conditional_select(node->key, finalWriteKey, !cond1 && !cond2 && cond3);
    finalWriteKey = Bid::conditional_select(node->key, finalWriteKey, !cond1 && !cond2 && !cond3 && cond4);
    finalWriteKey = Bid::conditional_select(node->key, finalWriteKey, !cond1 && !cond2 && !cond3 && !cond4 && cond5);


    unsigned long long finalWritePos = dumyPos;
    finalWritePos = Node::conditional_select(leftNode->pos, finalWritePos, cond1);
    finalWritePos = Node::conditional_select(rightNode->pos, finalWritePos, !cond1 && cond2);
    finalWritePos = Node::conditional_select(node->pos, finalWritePos, !cond1 && !cond2 && cond3);
    finalWritePos = Node::conditional_select(node->pos, finalWritePos, !cond1 && !cond2 && !cond3 && cond4);
    finalWritePos = Node::conditional_select(node->pos, finalWritePos, !cond1 && !cond2 && !cond3 && !cond4 && cond5);


    unsigned long long finalWriteNewPos = dumyPos;
    finalWriteNewPos = Node::conditional_select(newP, finalWriteNewPos, cond1 || (!cond1 && cond2) || (!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4) || (!cond1 && !cond2 && !cond3 && !cond4 && cond5));


    isDummy = !(cond1 || cond2 || cond3 || cond4 || cond5);

    Node* finalWriteNode = Node::clone(tmpDummyNode);
    Node::conditional_assign(finalWriteNode, leftNode, cond1);
    Node::conditional_assign(finalWriteNode, rightNode, !cond1 && cond2);
    Node::conditional_assign(finalWriteNode, node, !cond1 && !cond2 && cond3);
    Node::conditional_assign(finalWriteNode, node, !cond1 && !cond2 && !cond3 && cond4);
    Node::conditional_assign(finalWriteNode, node, !cond1 && !cond2 && !cond3 && !cond4 && cond5);

    tmp = oram->ReadWrite(finalWriteKey, finalWriteNode, finalWritePos, finalWriteNewPos, false, isDummy, false); //WRITE
    delete tmp;
    delete finalWriteNode;


    leftNode->pos = Node::conditional_select(newP, leftNode->pos, cond1);
    rightNode->pos = Node::conditional_select(newP, rightNode->pos, !cond1 && cond2);
    node->pos = Node::conditional_select(newP, node->pos, (!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4) || (!cond1 && !cond2 && !cond3 && !cond4 && cond5));

    Bid finalCacheWriteKey = dummy;
    finalCacheWriteKey = Bid::conditional_select(leftNode->key, finalCacheWriteKey, cond1);
    finalCacheWriteKey = Bid::conditional_select(rightNode->key, finalCacheWriteKey, !cond1 && cond2);
    finalCacheWriteKey = Bid::conditional_select(node->key, finalCacheWriteKey, (!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4) || (!cond1 && !cond2 && !cond3 && !cond4 && cond5));

    Node* finalCacheWriteNode = Node::clone(tmpDummyNode);
    Node::conditional_assign(finalCacheWriteNode, leftNode, cond1);
    Node::conditional_assign(finalCacheWriteNode, rightNode, !cond1 && cond2);
    Node::conditional_assign(finalCacheWriteNode, node, (!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4) || (!cond1 && !cond2 && !cond3 && !cond4 && cond5));

    isDummy = !cond1 && !cond2 && !cond3 && !cond4 && !cond5;

    tmp = readWriteCacheNode(finalCacheWriteKey, finalCacheWriteNode, false, isDummy); //WRITE             

    delete tmp;
    delete finalCacheWriteNode;

    leftRightNode->rightPos = Node::conditional_select(newP, leftRightNode->rightPos, !cond1 && !cond2 && cond3);
    rightLeftNode->leftPos = Node::conditional_select(newP, rightLeftNode->leftPos, !cond1 && !cond2 && !cond3 && cond4);


    finalCacheWriteKey = dummy;
    finalCacheWriteKey = Bid::conditional_select(leftRightNode->key, finalCacheWriteKey, (!cond1 && !cond2 && cond3));
    finalCacheWriteKey = Bid::conditional_select(rightLeftNode->key, finalCacheWriteKey, (!cond1 && !cond2 && !cond3 && cond4));

    finalCacheWriteNode = Node::clone(tmpDummyNode);
    Node::conditional_assign(finalCacheWriteNode, leftRightNode, (!cond1 && !cond2 && cond3));
    Node::conditional_assign(finalCacheWriteNode, rightLeftNode, (!cond1 && !cond2 && !cond3 && cond4));

    isDummy = !((!cond1 && !cond2 && cond3) || (!cond1 && !cond2 && !cond3 && cond4));

    tmp = readWriteCacheNode(finalCacheWriteKey, finalCacheWriteNode, false, isDummy); //WRITE             

    delete tmp;
    delete finalCacheWriteNode;



    rootPos = Node::conditional_select(leftNode->pos, rootPos, cond1);
    rootPos = Node::conditional_select(rightNode->pos, rootPos, !cond1 && cond2);
    rootPos = Node::conditional_select(leftRightNode->pos, rootPos, !cond1 && !cond2 && cond3);
    rootPos = Node::conditional_select(rightLeftNode->pos, rootPos, !cond1 && !cond2 && !cond3 && cond4);
    rootPos = Node::conditional_select(node->pos, rootPos, !cond1 && !cond2 && !cond3 && !cond4 && cond5);


    retKey = Bid::conditional_select(leftNode->key, retKey, cond1);
    retKey = Bid::conditional_select(rightNode->key, retKey, !cond1 && cond2);
    retKey = Bid::conditional_select(leftRightNode->key, retKey, !cond1 && !cond2 && cond3);
    retKey = Bid::conditional_select(rightLeftNode->key, retKey, !cond1 && !cond2 && !cond3 && cond4);
    retKey = Bid::conditional_select(node->key, retKey, !cond1 && !cond2 && !cond3 && !cond4 && cond5);

    height = Node::conditional_select(leftNode->height, height, cond1);
    height = Node::conditional_select(rightNode->height, height, !cond1 && cond2);
    height = Node::conditional_select(leftRightNode->height, height, !cond1 && !cond2 && cond3);
    height = Node::conditional_select(rightLeftNode->height, height, !cond1 && !cond2 && !cond3 && cond4);

    if (totheight == 1) {
        isDummy = !doubleRotation;
        unsigned long long finalPos = RandomPath();
        Node* finalNode = readWriteCacheNode(retKey, tmpDummyNode, true, isDummy);
        tmp = oram->ReadWrite(finalNode->key, finalNode, finalNode->pos, finalPos, false, isDummy, false); //WRITE
        rootPos = Node::conditional_select(finalPos, rootPos, doubleRotation);
        height = Node::conditional_select(finalNode->height, height, doubleRotation);
        delete tmp;
        delete finalNode;
    }

    delete tmpDummyNode;
    delete node;
    delete leftNode;
    delete rightNode;
    delete leftRightNode;
    delete rightLeftNode;
    return retKey;

}

string AVLTree::search(Node* rootNode, Bid omapKey) {
    Bid curKey = rootNode->key;
    unsigned long long lastPos = rootNode->pos;
    unsigned long long newPos = RandomPath();
    rootNode->pos = newPos;
    string res = "                ";
    Bid dumyID = oram->nextDummyCounter;
    Node* tmpDummyNode = new Node();
    tmpDummyNode->isDummy = true;
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
        delete head;
    } while (oram->readCnt <= upperBound);
    delete tmpDummyNode;
    for (int i = 0; i < resVec.size(); i++) {
        res[i] = Node::conditional_select(resVec[i], res[i], found);
    }
    // trim trailing spaces
    res.erase(std::find_if(res.rbegin(), res.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), res.end());

    return res;
}

void AVLTree::printTree(Node* rt, int indent) {
    if (rt != 0 && rt->key != 0) {
        Node* tmpDummyNode = new Node();
        tmpDummyNode->isDummy = true;
        Node* root = oram->ReadWriteTest(rt->key, tmpDummyNode, rt->pos, rt->pos, true, false, true);

        if (root->leftID != 0)
            printTree(oram->ReadWriteTest(root->leftID, tmpDummyNode, root->leftPos, root->leftPos, true, false, true), indent + 4);
        if (indent > 0) {
            for (int i = 0; i < indent; i++) {
                printf(" ");
            }
        }

        string value;
        value.assign(root->value.begin(), root->value.end());
        printf("Key:%lld Height:%lld Pos:%lld LeftID:%lld LeftPos:%lld RightID:%lld RightPos:%lld\n", root->key.getValue(), root->height, root->pos, root->leftID.getValue(), root->leftPos, root->rightID.getValue(), root->rightPos);
        if (root->rightID != 0)
            printTree(oram->ReadWriteTest(root->rightID, tmpDummyNode, root->rightPos, root->rightPos, true, false, true), indent + 4);
        delete tmpDummyNode;
        delete rt;
        delete root;
    }
}

/*
 * before executing each operation, this function should be called with proper arguments
 */
void AVLTree::startOperation(bool batchWrite) {
    oram->start(batchWrite);
    totheight = 0;
    exist = false;
    doubleRotation = false;
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

AVLTree::AVLTree(long long maxSize, bytes<Key> secretkey, Bid& rootKey, unsigned long long& rootPos, map<Bid, string>* pairs, map<unsigned long long, unsigned long long>* permutation) {
    int depth = (int) (ceil(log2(maxSize)) - 1) + 1;
    maxOfRandom = (long long) (pow(2, depth));

    vector<Node*> nodes;
    for (auto pair : (*pairs)) {
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

    bitonicSort(&nodes);
    double t;
    printf("Creating BST of %d Nodes\n", nodes.size());
    sortedArrayToBST(&nodes, 0, nodes.size() - 2, rootPos, rootKey, permutation);
    printf("Inserting in ORAM\n");

    double gp;
    int size = (int) nodes.size();
    for (int i = size; i < maxOfRandom * Z; i++) {
        Bid bid;
        bid = INF + i;
        Node* node = newNode(bid, "");
        node->isDummy = true;
        node->pos = (*permutation)[permutationIterator];
        permutationIterator++;
        nodes.push_back(node);
    }
    oram = new ORAM(maxSize, secretkey, &nodes);
}

int AVLTree::sortedArrayToBST(vector<Node*>* nodes, long long start, long long end, unsigned long long& pos, Bid& node, map<unsigned long long, unsigned long long>* permutation) {
    if (start > end) {
        pos = -1;
        node = 0;
        return 0;
    }

    unsigned long long mid = (start + end) / 2;
    Node *root = (*nodes)[mid];

    int leftHeight = sortedArrayToBST(nodes, start, mid - 1, root->leftPos, root->leftID, permutation);

    int rightHeight = sortedArrayToBST(nodes, mid + 1, end, root->rightPos, root->rightID, permutation);
    root->pos = (*permutation)[permutationIterator];
    permutationIterator++;
    root->height = max(leftHeight, rightHeight) + 1;
    pos = root->pos;
    node = root->key;
    return root->height;
}

Node* AVLTree::readWriteCacheNode(Bid bid, Node* inputnode, bool isRead, bool isDummy) {
    Node* tmpWrite = Node::clone(inputnode);

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

    avlCache.push_back(tmpWrite);
    return res;
}

void AVLTree::bitonicSort(vector<Node*>* nodes) {
    int len = nodes->size();
    bitonic_sort(nodes, 0, len, 1);
}

void AVLTree::bitonic_sort(vector<Node*>* nodes, int low, int n, int dir) {
    if (n > 1) {
        int middle = n / 2;
        bitonic_sort(nodes, low, middle, !dir);
        bitonic_sort(nodes, low + middle, n - middle, dir);
        bitonic_merge(nodes, low, n, dir);
    }
}

void AVLTree::bitonic_merge(vector<Node*>* nodes, int low, int n, int dir) {
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

void AVLTree::compare_and_swap(Node* item_i, Node* item_j, int dir) {
    int res = Bid::CTcmp(item_i->key, item_j->key);
    int cmp = Node::CTeq(res, 1);
    Node::conditional_swap(item_i, item_j, Node::CTeq(cmp, dir));
}

int AVLTree::greatest_power_of_two_less_than(int n) {
    int k = 1;
    while (k > 0 && k < n) {
        k = k << 1;
    }
    return k >> 1;
}

AVLTree::AVLTree(long long maxSize, long long initialSize, bytes<Key> secretkey, Bid& rootKey, unsigned long long& rootPos) {
    int nextPower2 = (int) pow(2, ceil(log2(initialSize)));
    unsigned long long blockSize = sizeof (Node);
    clen_size = AES::GetCiphertextLength((int) (blockSize));
    unsigned long long plaintext_size = (blockSize);
    storeSingleBlockSize = clen_size + IV;
    this->secretkey = secretkey;
    int depth = (int) (ceil(log2(maxSize)) - 1) + 1;
    maxOfRandom = (long long) (pow(2, depth));
    totalNumberOfNodes = initialSize;
    totalNumberOfPRFs = maxOfRandom * Z;
    AES::Setup();
    unsigned long long lastID = INF;
    unsigned long long neededNumner = nextPower2 - initialSize;
    double h;


    ocall_start_timer(426);
    printf("Creating permutation\n");
    createPermutation(maxSize);

    ocall_stop_timer(&h, 426);
    printf("PRF Time:%f\n", h);
    printf("Adding dummy nodes to next power of 2\n");
    ocall_start_timer(426);


    for (int i = 0; i <= neededNumner / 10000; i++) {
        char* tmp = new char[10000 * storeSingleBlockSize];
        vector<long long> indexes;
        for (int j = 0; j < min((int) (neededNumner - i * 10000), 10000); j++) {
            Bid bid = lastID;
            lastID++;
            Node* node = newNode(bid, "");
            node->isDummy = false;
            indexes.push_back(initialSize + (i * 10000) + j);

            std::array<byte_t, sizeof (Node) > data;

            const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*node)));
            const byte_t* end = begin + sizeof (Node);
            std::copy(begin, end, std::begin(data));

            block buffer(data.begin(), data.end());
            block ciphertext = AES::Encrypt(secretkey, buffer, clen_size, plaintext_size);
            std::memcpy(tmp + j * ciphertext.size(), ciphertext.data(), storeSingleBlockSize);
            delete node;
        }

        if (min((int) (neededNumner - i * 10000), 10000) != 0) {
            ocall_nwrite_rawRamStore(min((int) (neededNumner - i * 10000), 10000), indexes.data(), (const char*) tmp, storeSingleBlockSize * min((int) (neededNumner - i * 10000), 10000));
        }
        delete tmp;
        indexes.clear();
    }
    totalNumberOfNodes = nextPower2;

    printf("Bitonic sort\n");
    bitonicSortOCallBased(nextPower2);
    printf("Create AVL tree\n");
    sortedArrayToBST(0, nextPower2 - 2, rootPos, rootKey);
    printf("Finish AVL tree\n");


    Node* lastDummy = getNode(initialSize + neededNumner - 1);
    PRF* posPRF = getPRF(permutationIterator);
    lastDummy->pos = posPRF->index;
    //delete posPRF;
    //delete lastDummy;
    permutationIterator++;
    setNode(initialSize + neededNumner - 1, lastDummy);

    neededNumner = maxOfRandom * Z - (nextPower2);

    for (int i = 0; i <= neededNumner / 10000; i++) {
        char* tmp = new char[10000 * storeSingleBlockSize];
        vector<long long> indexes;
        for (int j = 0; j < min((int) (neededNumner - i * 10000), 10000); j++) {
            Bid bid;
            bid = lastID;
            lastID++;
            Node* node = newNode(bid, "");
            node->isDummy = true;
            PRF* posPRF = getPRF(permutationIterator);
            node->pos = posPRF->index;
            //delete posPRF;
            permutationIterator++;
            indexes.push_back((nextPower2) + (i * 10000) + j);
            std::array<byte_t, sizeof (Node) > data;

            const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*node)));
            const byte_t* end = begin + sizeof (Node);
            std::copy(begin, end, std::begin(data));

            block buffer(data.begin(), data.end());
            block ciphertext = AES::Encrypt(secretkey, buffer, clen_size, plaintext_size);
            std::memcpy(tmp + j * ciphertext.size(), ciphertext.data(), storeSingleBlockSize);
            delete node;
        }

        if (min((int) (neededNumner - i * 10000), 10000) != 0) {
            ocall_nwrite_rawRamStore(min((int) (neededNumner - i * 10000), 10000), indexes.data(), (const char*) tmp, storeSingleBlockSize * min((int) (neededNumner - i * 10000), 10000));
        }
        delete tmp;
        indexes.clear();
    }

    totalNumberOfNodes = maxOfRandom * Z;

    flushCache();
    flushPRFCache();

    printf("Inserting in ORAM\n");

    oram = new ORAM(maxSize, secretkey, maxOfRandom * Z);

    double t;
    ocall_stop_timer(&t, 426);
    printf("Setup Time:%f\n", t);
}

void AVLTree::bitonicSortOCallBased(int len) {
    bitonic_sortOCallBased(0, len, 1);
}

void AVLTree::bitonic_sortOCallBased(int low, int n, int dir) {
    if (n > 1) {
        int middle = n / 2;
        bitonic_sortOCallBased(low, middle, !dir);
        bitonic_sortOCallBased(low + middle, n - middle, dir);
        bitonic_mergeOCallBased(low, n, dir);
    }
}

void AVLTree::bitonic_mergeOCallBased(int low, int n, int dir) {
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

        bitonic_mergeOCallBased(low, m, dir);
        bitonic_mergeOCallBased(low + m, n - m, dir);
    }
}

void AVLTree::setNode(int index, Node* node) {
    //    size_t readSize;
    //    unsigned long long plaintext_size = sizeof (Node);
    //
    //    char* tmp = new char[storeSingleBlockSize];
    //
    //    std::array<byte_t, sizeof (Node) > data;
    //
    //    const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*node)));
    //    const byte_t* end = begin + sizeof (Node);
    //    std::copy(begin, end, std::begin(data));
    //
    //    block buffer(data.begin(), data.end());
    //    block ciphertext = AES::Encrypt(secretkey, buffer, clen_size, plaintext_size);
    //    std::memcpy(tmp, ciphertext.data(), storeSingleBlockSize);
    //    delete node;
    //
    //    ocall_write_rawRamStore(index, (const char*) tmp, storeSingleBlockSize);
    //    delete tmp;
}

Node* AVLTree::getNode(int index) {
    //        char* tmp = new char[storeSingleBlockSize];
    //        size_t readSize;
    //        ocall_read_rawRamStore(&readSize, index, tmp, storeSingleBlockSize);
    //        block ciphertext(tmp, tmp + storeSingleBlockSize);
    //        block buffer = AES::Decrypt(secretkey, ciphertext, clen_size);
    //    
    //        Node* node = new Node();
    //        std::array<byte_t, sizeof (Node) > arr;
    //        std::copy(buffer.begin(), buffer.begin() + sizeof (Node), arr.begin());
    //        from_bytes(arr, *node);
    //        delete tmp;
    //        return node;


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

void AVLTree::flushPRFCache() {
    for (int j = 0; j < prfCache1.size(); j++) {
        delete prfCache1[j];
    }

    for (int j = 0; j < prfCache2.size(); j++) {
        delete prfCache2[j];
    }

    prfCache1.clear();
    prfCache2.clear();

}

void AVLTree::flushCache() {
    size_t readSize;
    unsigned long long plaintext_size = sizeof (Node);

    char* tmp = new char[setupCache1.size() * storeSingleBlockSize];
    vector<long long> indexes;
    for (int j = 0; j < setupCache1.size(); j++) {
        indexes.push_back(currentBatchBegin1 + j);

        std::array<byte_t, sizeof (Node) > data;

        const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*(setupCache1[j]))));
        const byte_t* end = begin + sizeof (Node);
        std::copy(begin, end, std::begin(data));

        block buffer(data.begin(), data.end());
        block ciphertext = AES::Encrypt(secretkey, buffer, clen_size, plaintext_size);
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
        block ciphertext = AES::Encrypt(secretkey, buffer, clen_size, plaintext_size);
        std::memcpy(tmp + j * ciphertext.size(), ciphertext.data(), storeSingleBlockSize);
        delete setupCache2[j];
    }

    if (setupCache2.size() != 0) {
        ocall_nwrite_rawRamStore(setupCache2.size(), indexes.data(), (const char*) tmp, storeSingleBlockSize * setupCache2.size());
    }

    setupCache2.clear();
    delete tmp;
}

void AVLTree::fetchBatch1(int beginIndex) {
    size_t readSize;
    unsigned long long plaintext_size = sizeof (Node);

    char* tmp = new char[setupCache1.size() * storeSingleBlockSize];
    vector<long long> indexes;
    for (int j = 0; j < setupCache1.size(); j++) {
        indexes.push_back(currentBatchBegin1 + j);

        std::array<byte_t, sizeof (Node) > data;

        const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*(setupCache1[j]))));
        const byte_t* end = begin + sizeof (Node);
        std::copy(begin, end, std::begin(data));

        block buffer(data.begin(), data.end());
        block ciphertext = AES::Encrypt(secretkey, buffer, clen_size, plaintext_size);
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
        block buffer = AES::Decrypt(secretkey, ciphertext, clen_size);
        Node* node = new Node();
        std::array<byte_t, sizeof (Node) > arr;
        std::copy(buffer.begin(), buffer.begin() + sizeof (Node), arr.begin());
        from_bytes(arr, *node);
        setupCache1.push_back(node);
    }
    currentBatchBegin1 = beginIndex;

    delete tmp;
}

void AVLTree::fetchBatch2(int beginIndex) {
    size_t readSize;
    unsigned long long plaintext_size = sizeof (Node);

    char* tmp = new char[setupCache2.size() * storeSingleBlockSize];
    vector<long long> indexes;
    for (int j = 0; j < setupCache2.size(); j++) {
        indexes.push_back(currentBatchBegin2 + j);

        std::array<byte_t, sizeof (Node) > data;

        const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*(setupCache2[j]))));
        const byte_t* end = begin + sizeof (Node);
        std::copy(begin, end, std::begin(data));

        block buffer(data.begin(), data.end());
        block ciphertext = AES::Encrypt(secretkey, buffer, clen_size, plaintext_size);
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
        block buffer = AES::Decrypt(secretkey, ciphertext, clen_size);

        Node* node = new Node();
        std::array<byte_t, sizeof (Node) > arr;
        std::copy(buffer.begin(), buffer.begin() + sizeof (Node), arr.begin());
        from_bytes(arr, *node);
        setupCache2.push_back(node);
    }
    currentBatchBegin2 = beginIndex;
    delete tmp;
}

int AVLTree::sortedArrayToBST(long long start, long long end, unsigned long long& pos, Bid& node) {
    if (start > end) {
        pos = -1;
        node = 0;
        return 0;
    }

    unsigned long long mid = (start + end) / 2;
    Bid leftID, rightID;
    unsigned long long leftPos, rightPos;
    int leftHeight = sortedArrayToBST(start, mid - 1, leftPos, leftID);
    int rightHeight = sortedArrayToBST(mid + 1, end, rightPos, rightID);
    Node* root = getNode(mid);
    root->index = index++;
    root->leftID = leftID;
    root->rightID = rightID;
    root->leftPos = leftPos;
    root->rightPos = rightPos;
    PRF* posPRF = getPRF(permutationIterator);
    root->pos = posPRF->index;
    //delete posPRF;
    permutationIterator++;
    int height = max(leftHeight, rightHeight) + 1;
    pos = root->pos;
    node = root->key;
    root->height = height;
    setNode(mid, root);
    return height;
}

void AVLTree::createPermutation(int maxSize) {
    unsigned long long PRFblockSize = sizeof (PRF);
    unsigned long long prf_clen_size = AES::GetCiphertextLength((int) (PRFblockSize));
    unsigned long long PRFplaintext_size = (PRFblockSize);
    unsigned long long PRFstoreSingleBlockSize = prf_clen_size + IV;

    unsigned long long counter = 0;
    unsigned long long pos = 0;
    int k = 0;
    unsigned long long needed = maxSize*Z;
    for (int i = 0; i <= needed / 10000; i++) {
        char* tmp = new char[10000 * PRFstoreSingleBlockSize];

        vector<long long> indexes;
        for (int j = 0; j < min((int) (needed - i * 10000), 10000); j++) {
            PRF prf;
            prf.setValue(counter);
            indexes.push_back(counter);
            block buffer1(prf.id.begin(), prf.id.end());
            block prfVal = AES::PRF(secretkey, buffer1, PRF_SIZE * 2, PRF_SIZE);
            std::copy(prfVal.begin(), prfVal.end(), prf.id.begin());
            counter++;
            k++;
            prf.index = pos;

            std::array<byte_t, sizeof (PRF) > data;

            const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof(prf));
            const byte_t* end = begin + sizeof (PRF);
            std::copy(begin, end, std::begin(data));

            block buffer(data.begin(), data.end());
            block ciphertext = AES::Encrypt(secretkey, buffer, prf_clen_size, PRFplaintext_size);
            std::memcpy(tmp + j * ciphertext.size(), ciphertext.data(), PRFstoreSingleBlockSize);
            if (k == Z) {
                k = 0;
                pos++;
            }
        }
        if (min((int) (needed - i * 10000), 10000) != 0) {
            ocall_nwrite_prf(indexes.size(), indexes.data(), (const char*) tmp, PRFstoreSingleBlockSize * indexes.size());
        }
        delete tmp;
    }
    prfbitonicSortOCallBased(needed);
}

void AVLTree::prfbitonicSortOCallBased(int len) {
    prfbitonic_sortOCallBased(0, len, 1);
}

void AVLTree::prfbitonic_sortOCallBased(int low, int n, int dir) {
    if (n > 1) {
        int middle = n / 2;
        prfbitonic_sortOCallBased(low, middle, !dir);
        prfbitonic_sortOCallBased(low + middle, n - middle, dir);
        prfbitonic_mergeOCallBased(low, n, dir);
    }
}

void AVLTree::prfbitonic_mergeOCallBased(int low, int n, int dir) {
    if (n > 1) {
        int m = greatest_power_of_two_less_than(n);

        for (int i = low; i < (low + n - m); i++) {
            if (i != (i + m)) {
                PRF* left = getPRF(i);
                PRF* right = getPRF(i + m);
                prf_compare_and_swap(left, right, dir);
                setPRF(i, left);
                setPRF(i + m, right);

            }
        }

        prfbitonic_mergeOCallBased(low, m, dir);
        prfbitonic_mergeOCallBased(low + m, n - m, dir);
    }
}

void AVLTree::setPRF(int index, PRF* node) {
    //    unsigned long long PRFblockSize = sizeof (PRF);
    //    unsigned long long prf_clen_size = AES::GetCiphertextLength((int) (PRFblockSize));
    //    unsigned long long PRFplaintext_size = (PRFblockSize);
    //    unsigned long long PRFstoreSingleBlockSize = prf_clen_size + IV;
    //    char* tmp = new char[PRFstoreSingleBlockSize];
    //
    //    std::array<byte_t, sizeof (PRF) > data;
    //
    //    const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*node)));
    //    const byte_t* end = begin + sizeof (PRF);
    //    std::copy(begin, end, std::begin(data));
    //
    //    block buffer(data.begin(), data.end());
    //    block ciphertext = AES::Encrypt(secretkey, buffer, prf_clen_size, PRFplaintext_size);
    //    std::memcpy(tmp, ciphertext.data(), PRFstoreSingleBlockSize);
    //    delete node;
    //
    //    ocall_write_prfRamStore(index, (const char*) tmp, PRFstoreSingleBlockSize);
    //    delete tmp;
}

PRF* AVLTree::getPRF(int index) {
    //    unsigned long long PRFblockSize = sizeof (PRF);
    //    unsigned long long prf_clen_size = AES::GetCiphertextLength((int) (PRFblockSize));
    //    unsigned long long PRFplaintext_size = (PRFblockSize);
    //    unsigned long long PRFstoreSingleBlockSize = prf_clen_size + IV;
    //    char* tmp = new char[PRFstoreSingleBlockSize];
    //    size_t readSize;
    //    ocall_read_prfRamStore(&readSize, index, tmp, PRFstoreSingleBlockSize);
    //    block ciphertext(tmp, tmp + PRFstoreSingleBlockSize);
    //    block buffer = AES::Decrypt(secretkey, ciphertext, prf_clen_size);
    //
    //    PRF* node = new PRF();
    //    std::array<byte_t, sizeof (PRF) > arr;
    //    std::copy(buffer.begin(), buffer.begin() + sizeof (PRF), arr.begin());
    //    from_bytes(arr, *node);
    //    delete tmp;
    //    return node;


    if ((index >= currentPRFBatch1 && index < currentPRFBatch1 + BATCH_SIZE) && (prfCache1.size() != 0)) {
        return prfCache1[index - currentPRFBatch1];
    } else if ((index >= currentPRFBatch2 && index < currentPRFBatch2 + BATCH_SIZE) && (prfCache2.size() != 0)) {
        return prfCache2[index - currentPRFBatch2];
    } else {
        int pageBegin = (index / BATCH_SIZE) * BATCH_SIZE;
        if (isPRFLeftBatchUpdated == false) {
            isPRFLeftBatchUpdated = true;
            fetchPRF1(pageBegin);
            return prfCache1[index - currentPRFBatch1];
        } else {
            isPRFLeftBatchUpdated = false;
            fetchPRF2(pageBegin);
            return prfCache2[index - currentPRFBatch2];
        }
    }
}

void AVLTree::prf_compare_and_swap(PRF* item_i, PRF* item_j, int dir) {
    int res = PRF::CTcmp(item_i->id, item_j->id);
    int cmp = PRF::CTeq(res, 1);
    PRF::conditional_swap(item_i, item_j, PRF::CTeq(cmp, dir));
}

void AVLTree::fetchPRF1(int beginIndex) {
    size_t readSize;
    unsigned long long PRFblockSize = sizeof (PRF);
    unsigned long long prf_clen_size = AES::GetCiphertextLength((int) (PRFblockSize));
    unsigned long long PRFplaintext_size = (PRFblockSize);
    unsigned long long PRFstoreSingleBlockSize = prf_clen_size + IV;

    char* tmp = new char[prfCache1.size() * PRFstoreSingleBlockSize];
    vector<long long> indexes;
    for (int j = 0; j < prfCache1.size(); j++) {
        indexes.push_back(currentPRFBatch1 + j);

        std::array<byte_t, sizeof (PRF) > data;

        const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*(prfCache1[j]))));
        const byte_t* end = begin + sizeof (PRF);
        std::copy(begin, end, std::begin(data));

        block buffer(data.begin(), data.end());
        block ciphertext = AES::Encrypt(secretkey, buffer, prf_clen_size, PRFplaintext_size);
        std::memcpy(tmp + j * ciphertext.size(), ciphertext.data(), PRFstoreSingleBlockSize);
        delete prfCache1[j];
    }

    if (prfCache1.size() != 0) {
        ocall_nwrite_prf(prfCache1.size(), indexes.data(), (const char*) tmp, PRFstoreSingleBlockSize * prfCache1.size());
    }
    delete tmp;

    prfCache1.clear();


    tmp = new char[BATCH_SIZE * PRFstoreSingleBlockSize];
    ocall_nread_prf(&readSize, BATCH_SIZE, beginIndex, tmp, BATCH_SIZE * PRFstoreSingleBlockSize);

    for (unsigned int i = 0; i < min((unsigned long long) BATCH_SIZE, totalNumberOfPRFs - beginIndex); i++) {
        block ciphertext(tmp + i*PRFstoreSingleBlockSize, tmp + (i + 1) * PRFstoreSingleBlockSize);
        block buffer = AES::Decrypt(secretkey, ciphertext, prf_clen_size);

        PRF* node = new PRF();
        std::array<byte_t, sizeof (PRF) > arr;
        std::copy(buffer.begin(), buffer.begin() + sizeof (PRF), arr.begin());
        from_bytes(arr, *node);
        prfCache1.push_back(node);
    }
    currentPRFBatch1 = beginIndex;
    delete tmp;
}

void AVLTree::fetchPRF2(int beginIndex) {
    size_t readSize;
    unsigned long long PRFblockSize = sizeof (PRF);
    unsigned long long prf_clen_size = AES::GetCiphertextLength((int) (PRFblockSize));
    unsigned long long PRFplaintext_size = (PRFblockSize);
    unsigned long long PRFstoreSingleBlockSize = prf_clen_size + IV;

    char* tmp = new char[prfCache2.size() * PRFstoreSingleBlockSize];
    vector<long long> indexes;
    for (int j = 0; j < prfCache2.size(); j++) {
        indexes.push_back(currentPRFBatch2 + j);

        std::array<byte_t, sizeof (PRF) > data;

        const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*(prfCache2[j]))));
        const byte_t* end = begin + sizeof (PRF);
        std::copy(begin, end, std::begin(data));

        block buffer(data.begin(), data.end());
        block ciphertext = AES::Encrypt(secretkey, buffer, prf_clen_size, PRFplaintext_size);
        std::memcpy(tmp + j * ciphertext.size(), ciphertext.data(), PRFstoreSingleBlockSize);
        delete prfCache2[j];
    }

    if (prfCache2.size() != 0) {
        ocall_nwrite_prf(prfCache2.size(), indexes.data(), (const char*) tmp, PRFstoreSingleBlockSize * prfCache2.size());
    }
    delete tmp;

    prfCache2.clear();


    tmp = new char[BATCH_SIZE * PRFstoreSingleBlockSize];
    ocall_nread_prf(&readSize, BATCH_SIZE, beginIndex, tmp, BATCH_SIZE * PRFstoreSingleBlockSize);
    for (unsigned int i = 0; i < min((unsigned long long) BATCH_SIZE, totalNumberOfPRFs - beginIndex); i++) {
        block ciphertext(tmp + i*PRFstoreSingleBlockSize, tmp + (i + 1) * PRFstoreSingleBlockSize);
        block buffer = AES::Decrypt(secretkey, ciphertext, prf_clen_size);

        PRF* node = new PRF();
        std::array<byte_t, sizeof (PRF) > arr;
        std::copy(buffer.begin(), buffer.begin() + sizeof (PRF), arr.begin());
        from_bytes(arr, *node);
        prfCache2.push_back(node);
    }
    currentPRFBatch2 = beginIndex;

    delete tmp;
}

void AVLTree::searchAndIncrement(Node* rootNode, Bid omapKey, string& res, bool isFirstPart) {
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
    unsigned long long dumyPos = RandomPath();

    do {
        unsigned long long rnd = RandomPath();
        unsigned long long rnd2 = RandomPath();
        bool isDummyAction = Node::CTeq(Node::CTcmp(dummyState, 1), 0);
        head = oram->ReadWrite(curKey, lastPos, newPos, isDummyAction, rnd2, omapKey, isFirstPart);

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

        dummyState = Node::conditional_select(dummyState + 1, dummyState, !cond1 && !cond2 && !cond3 && cond4 || (!cond1 && ((cond2 && head->leftID.isZero()) || (cond3 && head->rightID.isZero()))));
        delete head;
    } while (oram->readCnt <= upperBound);
    delete tmpDummyNode;
    res.assign(resVec.begin(), resVec.end());
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
    unsigned long long dumyPos = RandomPath();

    do {
        unsigned long long rnd = RandomPath();
        unsigned long long rnd2 = RandomPath();
        bool isDummyAction = Node::CTeq(Node::CTcmp(dummyState, 1), 0);
        head = oram->ReadWrite(curKey, lastPos, newPos, isDummyAction, rnd2, omapKey, newValue, false);

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

        dummyState = Node::conditional_select(dummyState + 1, dummyState, !cond1 && !cond2 && !cond3 && cond4 || (!cond1 && ((cond2 && head->leftID.isZero()) || (cond3 && head->rightID.isZero()))));
        delete head;
    } while (oram->readCnt <= upperBound);
    delete tmpDummyNode;
    res.assign(resVec.begin(), resVec.end());
}

void AVLTree::searchInsert(Node* rootNode, Bid omapKey, string& res, string newValue) {
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
    unsigned long long dumyPos = RandomPath();
    std::array< byte_t, 16> newVec;
    std::fill(newVec.begin(), newVec.end(), 0);
    std::copy(newValue.begin(), newValue.end(), newVec.begin());

    do {
        unsigned long long rnd = RandomPath();
        unsigned long long rnd2 = RandomPath();
        bool isDummyAction = Node::CTeq(Node::CTcmp(dummyState, 1), 0);

        head = oram->ReadWrite(curKey, lastPos, newPos, isDummyAction, rnd2, omapKey, newVec);

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

        dummyState = Node::conditional_select(dummyState + 1, dummyState, !cond1 && !cond2 && !cond3 && cond4 || (!cond1 && ((cond2 && head->leftID.isZero()) || (cond3 && head->rightID.isZero()))));
        delete head;
    } while (oram->readCnt <= upperBound);
    delete tmpDummyNode;
    res.assign(resVec.begin(), resVec.end());
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//                  OLD BIG ENCLAVE SETUP
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

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