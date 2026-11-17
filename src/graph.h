// Graph (Adjacency List) + Hash Table (name -> id)
// Locations are nodes, roads are undirected edges.
#pragma once
#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

const int FLOOD_LIMIT_CM = 30;  // road auto-closes at or above this water level

struct Edge {
    int to;
    int dist;      // metres
    int flood;     // water level in cm
    bool blocked;  // manually closed by the user
};

inline std::string toLower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

class Graph {
    std::unordered_map<std::string, int> index;  // Hash Table: lowercase name -> node id
    std::vector<std::string> names;              // node id -> display name
    std::vector<bool> shelter;                   // node id -> is it a shelter?
    std::vector<float> px, py;                   // optional map position (used by the GUI)
    std::vector<std::vector<Edge>> adjList;      // Adjacency List

    Edge* findEdge(int a, int b);

public:
    int addNode(const std::string& name, bool isShelter, float x = -1, float y = -1);  // -1 if duplicate
    bool addEdge(int a, int b, int dist, int flood);

    int id(const std::string& name) const;  // -1 if not found
    const std::string& name(int v) const { return names[v]; }
    bool isShelter(int v) const { return shelter[v]; }
    bool hasPos(int v) const { return px[v] >= 0 && py[v] >= 0; }
    float x(int v) const { return px[v]; }
    float y(int v) const { return py[v]; }
    int size() const { return (int)names.size(); }
    int shelterCount() const;
    int roadCount() const;
    const std::vector<Edge>& adj(int v) const { return adjList[v]; }

    // A road is usable only if not manually blocked AND water is below the limit.
    static bool isOpen(const Edge& e) { return !e.blocked && e.flood < FLOOD_LIMIT_CM; }

    // Read / change the state of the road a <-> b (both directions kept in sync).
    bool getRoad(int a, int b, bool& blocked, int& flood);
    bool setRoad(int a, int b, bool blocked, int flood);
    void clear();
};
