#ifndef NODE_H
#define NODE_H

#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>      /* vsnprintf */
#include <vector>
#include <array>
#include "OMAP/Types.hpp"
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

