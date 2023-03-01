#ifndef OHEAP_H
#define OHEAP_H

#include <stdio.h> 
#include <stdlib.h> 
#include <limits.h> 
#include <string>
#include "GraphNode.h"
#include "OMAP.h"

using namespace std;

class OHeap {
private:
    int size;
    vector<OMAP*> omaps;
    int partitions;
    map<int, bool> modifiedOMAPs;

    void swapMinHeapNode(string a, string aValue, string b, string bValue);
    void minHeapify(int idx);
    void writeOMAP(string omapKey, string omapValue);
    string readOMAP(string omapKey);
    vector<string> splitData(const string& str, const string& delim);
    string readAndSetDistInOMAP(string omapKey, string omapValue);
    void decreaseKey(int v, int dist);
    bool isInMinHeap(int v);
    bool isEmpty();

public:
    OHeap(vector<OMAP*>);
    virtual ~OHeap();
    void setNewMinHeapNode(int newMinHeapNodeV, int newMinHeapNodeDist);
    void extractMinID(int& id, int& dist);
    void dummyOperation();


};
#endif /* OHEAP_H */

