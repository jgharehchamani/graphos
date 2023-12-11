#include "GraphNode.h"

GraphNode::GraphNode() {
}

GraphNode::~GraphNode() {
}

GraphNode* GraphNode::convertBlockToNode(block b) {
    GraphNode* node = new GraphNode();
    std::array<byte_t, sizeof (GraphNode) > arr;
    std::copy(b.begin(), b.begin() + sizeof (GraphNode), arr.begin());
    from_bytes(arr, *node);
    return node;
}

block GraphNode::convertNodeToBlock(GraphNode* node) {
    std::array<byte_t, sizeof (GraphNode) > data = to_bytes(*node);
    block b(data.begin(), data.end());
    return b;
}