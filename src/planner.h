// Planner: ties the data structures together and implements the commands.
//   Graph (Adjacency List) + Hash Table ... map storage and name lookup  (graph.h)
//   Min-Heap (priority_queue) ............ Dijkstra shortest path
//   Union-Find ........................... cut-off detection (CHECK / ROUTE pre-check)
//   Queue ................................ BFS for REACHABLE
//   Stack ................................ UNDO history + path printing
#pragma once
#include <iostream>
#include <stack>
#include <string>
#include <vector>

#include "graph.h"
#include "union_find.h"

struct PathResult {
    bool found = false;
    int dist = 0;
    std::vector<int> path;
};

struct Action {  // one undoable change to a road
    int a, b;
    bool wasBlocked;
    int wasFlood;
    std::string what;
};

class Planner {
    Graph g;
    UnionFind uf;
    std::stack<Action> undoStack;  // Stack
    std::ostream* out = &std::cout;  // where messages go (console by default, GUI captures them)
    PathResult last;                 // most recent route found (the GUI highlights it)

    void rebuildUnionFind();
    bool resolve(const std::string& name, int& id) const;
    PathResult shortestPath(int src, int dst);  // dst == -1 -> nearest shelter
    void printPath(const PathResult& r) const;
    void changeRoad(int a, int b, bool blocked, int flood, const std::string& what);

public:
    void setOutput(std::ostream& o) { out = &o; }
    const Graph& graph() const { return g; }
    const PathResult& lastPath() const { return last; }
    void clearPath() { last = PathResult(); }
    bool connectedToShelter(int v);  // Union-Find check, used by the GUI to colour cut-off areas

    bool load(const std::string& path);
    void show() const;
    void block(const std::string& a, const std::string& b);
    void unblock(const std::string& a, const std::string& b);
    void setFlood(const std::string& a, const std::string& b, int cm);
    void undo();
    void route(const std::string& a, const std::string& b);
    void nearest(const std::string& a);
    void check(const std::string& a);
    void reachable(const std::string& a);
};
