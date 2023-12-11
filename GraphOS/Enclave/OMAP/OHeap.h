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
    OMAP* omap;
    int maxSize;

    void swapMinHeapNode(string a, string aValue, string b, string bValue);
    void minHeapify(int idx);
    void minHeapify2(int idx);
    void writeOMAP(string omapKey, string omapValue);
    string readWriteOMAP(string omapKey, string omapValue);
    string readOMAP(string omapKey);
    vector<string> splitData(const string& str, const string& delim);

    string CTString(string a, string b, int choice) {
        unsigned int one = 1;
        string result = "";
        int maxSize = max(a.length(), b.length());
        for (int i = 0; i < maxSize; i++) {
            a += " ";
            b += " ";
        }
        for (int i = 0; i < maxSize; i++) {
            result += (~((unsigned int) choice - one) & a.at(i)) | ((unsigned int) (choice - one) & b.at(i));
        }
        result.erase(std::find_if(result.rbegin(), result.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), result.end());
        return result;
    }

    bool CTeq(string a, string b) {
        a.erase(std::find_if(a.rbegin(), a.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), a.end());
        b.erase(std::find_if(b.rbegin(), b.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), b.end());
        bool res = Node::CTeq((int) a.length(), (int) b.length());
        for (int i = 0; i < min((int) a.length(), (int) b.length()); i++) {
            res = Node::conditional_select(false, res, !Node::CTeq(a.at(i), b.at(i)));
        }
        return res;
    }

public:
    OHeap(OMAP* omap, int maxSize);
    virtual ~OHeap();
    void setNewMinHeapNode(int newMinHeapNodeV, int newMinHeapNodeDist);
    void extractMinID(int& id, int& dist);
    void dummyOperation();
    void execute(int& id, int& dist, int op);
    void execute2(int& id, int& dist, int op);


};
#endif /* OHEAP_H */

