#ifndef OBLIVIOUSMINHEAP_H
#define OBLIVIOUSMINHEAP_H
#include <stdio.h> 
#include <stdlib.h> 
#include <limits.h> 
#include <string>
#include "GraphNode.h"

using namespace std;

class ObliviousMinHeap {
private:
    int size;

    void swapMinHeapNode(string a, string aValue, string b, string bValue);
    void minHeapify(int idx);
    void writeOMAP(string omapKey, string omapValue);
    string readOMAP(string omapKey);
    vector<string> splitData(const string& str, const string& delim);
    string readAndSetDistInOMAP(string omapKey, string omapValue);

public:
    ObliviousMinHeap(int V);
    virtual ~ObliviousMinHeap();
    void setNewMinHeapNode(int arrayIndex, int newMinHeapNodeV, int newMinHeapNodeDist);
    void decreaseKey(int v, int dist);
    bool isInMinHeap(int v);
    int isEmpty();
    int extractMinID();


};

#endif /* OBLIVIOUSMINHEAP_H */

