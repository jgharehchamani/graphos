#ifndef NODE_H
#define NODE_H

#include "Types.h"
using namespace std;

class GraphNode {
public:
    int src_id;
    int dst_id;
    int weight;

    GraphNode();
    ~GraphNode();

    static GraphNode* convertBlockToNode(block b);
    static block convertNodeToBlock(GraphNode* node);

};

#endif /* NODE_H */

