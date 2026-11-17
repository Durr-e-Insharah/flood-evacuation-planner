#include "graph.h"

int Graph::addNode(const std::string& name, bool isShelter, float x, float y) {
    std::string key = toLower(name);
    if (index.count(key)) return -1;
    int v = (int)names.size();
    index[key] = v;
    names.push_back(name);
    shelter.push_back(isShelter);
    px.push_back(x);
    py.push_back(y);
    adjList.emplace_back();
    return v;
}

bool Graph::addEdge(int a, int b, int dist, int flood) {
    if (a < 0 || b < 0 || a >= size() || b >= size() || a == b || dist <= 0) return false;
    adjList[a].push_back({b, dist, flood, false});
    adjList[b].push_back({a, dist, flood, false});
    return true;
}

int Graph::id(const std::string& name) const {
    auto it = index.find(toLower(name));
    return it == index.end() ? -1 : it->second;
}

int Graph::shelterCount() const {
    int c = 0;
    for (bool s : shelter) c += s;
    return c;
}

int Graph::roadCount() const {
    int c = 0;
    for (const auto& list : adjList) c += (int)list.size();
    return c / 2;
}

Edge* Graph::findEdge(int a, int b) {
    for (Edge& e : adjList[a])
        if (e.to == b) return &e;
    return nullptr;
}

bool Graph::getRoad(int a, int b, bool& blocked, int& flood) {
    Edge* e = findEdge(a, b);
    if (!e) return false;
    blocked = e->blocked;
    flood = e->flood;
    return true;
}

bool Graph::setRoad(int a, int b, bool blocked, int flood) {
    Edge* e1 = findEdge(a, b);
    Edge* e2 = findEdge(b, a);
    if (!e1 || !e2) return false;
    e1->blocked = e2->blocked = blocked;
    e1->flood = e2->flood = flood;
    return true;
}

void Graph::clear() {
    index.clear();
    names.clear();
    shelter.clear();
    px.clear();
    py.clear();
    adjList.clear();
}
