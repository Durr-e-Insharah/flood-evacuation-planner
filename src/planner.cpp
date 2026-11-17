#include "planner.h"

#include <climits>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>

using namespace std;

// ---------------------------------------------------------------- loading
bool Planner::load(const string& path) {
    ifstream in(path);
    if (!in) {
        *out << "Error: cannot open map file '" << path << "'\n";
        return false;
    }
    g.clear();
    while (!undoStack.empty()) undoStack.pop();
    last = PathResult();

    string line;
    int lineNo = 0;
    while (getline(in, line)) {
        lineNo++;
        if (line.empty() || line[0] == '#') continue;
        istringstream ss(line);
        string kind;
        ss >> kind;
        if (kind == "NODE") {
            string name, type;
            float x = -1, y = -1;
            ss >> name >> type;
            if (!(ss >> x >> y)) x = y = -1;  // coordinates are optional
            if (name.empty() || g.addNode(name, toLower(type) == "shelter", x, y) < 0)
                *out << "Warning (line " << lineNo << "): bad or duplicate NODE, skipped\n";
        } else if (kind == "EDGE") {
            string a, b;
            int dist = 0, flood = 0;
            ss >> a >> b >> dist >> flood;
            if (!g.addEdge(g.id(a), g.id(b), dist, flood))
                *out << "Warning (line " << lineNo << "): bad EDGE, skipped\n";
        } else {
            *out << "Warning (line " << lineNo << "): unknown entry '" << kind << "', skipped\n";
        }
    }
    rebuildUnionFind();
    *out << "Loaded " << g.size() << " locations (" << g.shelterCount() << " shelters), "
         << g.roadCount() << " roads. Flood limit: " << FLOOD_LIMIT_CM << " cm.\n";
    return g.size() > 0;
}

// Union-Find cannot "un-union", so after any road change we rebuild it
// from the currently open roads. Cost O(E), fine for a city-sized map.
void Planner::rebuildUnionFind() {
    uf.reset(g.size());
    for (int u = 0; u < g.size(); u++)
        for (const Edge& e : g.adj(u))
            if (Graph::isOpen(e)) uf.unite(u, e.to);
}

bool Planner::resolve(const string& name, int& id) const {
    id = g.id(name);
    if (id < 0) *out << "Unknown location: " << name << "  (type SHOW to see all names)\n";
    return id >= 0;
}

// ---------------------------------------------------------------- SHOW
void Planner::show() const {
    *out << "\nLOCATIONS\n";
    for (int v = 0; v < g.size(); v++)
        *out << "  " << left << setw(20) << g.name(v) << (g.isShelter(v) ? "[SHELTER]" : "") << "\n";

    *out << "\nROADS\n";
    for (int u = 0; u < g.size(); u++)
        for (const Edge& e : g.adj(u)) {
            if (u > e.to) continue;  // print each road once
            string status = "OPEN";
            if (e.blocked) status = "CLOSED (blocked)";
            else if (e.flood >= FLOOD_LIMIT_CM) status = "CLOSED (flood)";
            *out << "  " << left << setw(18) << g.name(u) << " <-> " << setw(18) << g.name(e.to)
                 << right << setw(5) << e.dist << " m  flood " << setw(3) << e.flood << " cm  "
                 << status << "\n";
        }
    *out << "\n";
}

// ---------------------------------------------------------------- road changes
void Planner::changeRoad(int a, int b, bool blocked, int flood, const string& what) {
    bool wasBlocked;
    int wasFlood;
    g.getRoad(a, b, wasBlocked, wasFlood);
    undoStack.push({a, b, wasBlocked, wasFlood, what});
    g.setRoad(a, b, blocked, flood);
    rebuildUnionFind();
    last = PathResult();  // old route may no longer be valid
}

void Planner::block(const string& an, const string& bn) {
    int a, b;
    if (!resolve(an, a) || !resolve(bn, b)) return;
    bool blocked;
    int flood;
    if (!g.getRoad(a, b, blocked, flood)) {
        *out << "No road between " << g.name(a) << " and " << g.name(b) << ".\n";
        return;
    }
    if (blocked) {
        *out << "That road is already blocked.\n";
        return;
    }
    changeRoad(a, b, true, flood, "BLOCK " + g.name(a) + " " + g.name(b));
    *out << "Road " << g.name(a) << " <-> " << g.name(b) << " closed.\n";
}

void Planner::unblock(const string& an, const string& bn) {
    int a, b;
    if (!resolve(an, a) || !resolve(bn, b)) return;
    bool blocked;
    int flood;
    if (!g.getRoad(a, b, blocked, flood)) {
        *out << "No road between " << g.name(a) << " and " << g.name(b) << ".\n";
        return;
    }
    if (!blocked) {
        *out << "That road is not manually blocked.\n";
        return;
    }
    changeRoad(a, b, false, flood, "UNBLOCK " + g.name(a) + " " + g.name(b));
    *out << "Road " << g.name(a) << " <-> " << g.name(b) << " unblocked.\n";
    if (flood >= FLOOD_LIMIT_CM)
        *out << "Note: water level is still " << flood << " cm, so the road stays closed until FLOOD lowers it.\n";
}

void Planner::setFlood(const string& an, const string& bn, int cm) {
    int a, b;
    if (!resolve(an, a) || !resolve(bn, b)) return;
    bool blocked;
    int flood;
    if (!g.getRoad(a, b, blocked, flood)) {
        *out << "No road between " << g.name(a) << " and " << g.name(b) << ".\n";
        return;
    }
    if (cm < 0) {
        *out << "Flood level cannot be negative.\n";
        return;
    }
    changeRoad(a, b, blocked, cm, "FLOOD " + g.name(a) + " " + g.name(b) + " " + to_string(cm));
    *out << "Water level on " << g.name(a) << " <-> " << g.name(b) << " set to " << cm << " cm. ";
    if (cm >= FLOOD_LIMIT_CM) *out << "Road AUTO-CLOSED (limit " << FLOOD_LIMIT_CM << " cm).\n";
    else if (blocked) *out << "Road is still manually blocked.\n";
    else *out << "Road is open.\n";
}

void Planner::undo() {
    if (undoStack.empty()) {
        *out << "Nothing to undo.\n";
        return;
    }
    Action act = undoStack.top();
    undoStack.pop();
    g.setRoad(act.a, act.b, act.wasBlocked, act.wasFlood);
    rebuildUnionFind();
    last = PathResult();
    *out << "Undone: " << act.what << "\n";
}

// ---------------------------------------------------------------- Dijkstra
PathResult Planner::shortestPath(int src, int dst) {
    int n = g.size();
    vector<int> dist(n, INT_MAX), parent(n, -1);
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;  // Min-Heap

    dist[src] = 0;
    pq.push({0, src});
    int target = -1;
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d > dist[u]) continue;  // stale heap entry
        if (u == dst || (dst == -1 && g.isShelter(u))) {
            target = u;
            break;
        }
        for (const Edge& e : g.adj(u)) {
            if (!Graph::isOpen(e)) continue;  // closed roads do not exist for routing
            int nd = d + e.dist;
            if (nd < dist[e.to]) {
                dist[e.to] = nd;
                parent[e.to] = u;
                pq.push({nd, e.to});
            }
        }
    }

    PathResult res;
    if (target == -1) return res;
    res.found = true;
    res.dist = dist[target];
    stack<int> st;  // parent chain comes out backwards, a Stack reverses it
    for (int v = target; v != -1; v = parent[v]) st.push(v);
    while (!st.empty()) {
        res.path.push_back(st.top());
        st.pop();
    }
    return res;
}

void Planner::printPath(const PathResult& r) const {
    *out << "Route: ";
    for (size_t i = 0; i < r.path.size(); i++) *out << (i ? " -> " : "") << g.name(r.path[i]);
    *out << "\nTotal distance: " << r.dist << " m\n";
}

// ---------------------------------------------------------------- queries
void Planner::route(const string& an, const string& bn) {
    last = PathResult();
    int a, b;
    if (!resolve(an, a) || !resolve(bn, b)) return;
    if (a == b) {
        *out << "Start and destination are the same place.\n";
        return;
    }
    if (!uf.connected(a, b)) {  // Union-Find answers instantly, no Dijkstra needed
        *out << "NO ROUTE: " << g.name(a) << " and " << g.name(b) << " are cut off from each other.\n";
        return;
    }
    last = shortestPath(a, b);
    printPath(last);
}

void Planner::nearest(const string& an) {
    last = PathResult();
    int a;
    if (!resolve(an, a)) return;
    bool any = false;
    for (int v = 0; v < g.size() && !any; v++)
        if (g.isShelter(v) && uf.connected(a, v)) any = true;
    if (!any) {
        *out << "WARNING: " << g.name(a) << " is cut off from all shelters!\n";
        return;
    }
    PathResult r = shortestPath(a, -1);
    last = r;
    if (r.path.size() == 1) {
        *out << g.name(a) << " is already a shelter.\n";
        return;
    }
    *out << "Nearest shelter: " << g.name(r.path.back()) << "\n";
    printPath(r);
}

bool Planner::connectedToShelter(int v) {
    for (int s = 0; s < g.size(); s++)
        if (g.isShelter(s) && uf.connected(v, s)) return true;
    return false;
}

void Planner::check(const string& an) {
    int a;
    if (!resolve(an, a)) return;
    vector<string> found;
    for (int v = 0; v < g.size(); v++)
        if (g.isShelter(v) && uf.connected(a, v)) found.push_back(g.name(v));
    if (found.empty()) {
        *out << "WARNING: " << g.name(a) << " is cut off from all shelters!\n";
        return;
    }
    *out << "OK: " << g.name(a) << " can reach " << found.size() << " of " << g.shelterCount() << " shelter(s): ";
    for (size_t i = 0; i < found.size(); i++) *out << (i ? ", " : "") << found[i];
    *out << "\n";
}

void Planner::reachable(const string& an) {
    int a;
    if (!resolve(an, a)) return;
    vector<bool> seen(g.size(), false);
    queue<int> q;  // Queue for BFS
    q.push(a);
    seen[a] = true;
    vector<int> order;
    while (!q.empty()) {
        int u = q.front();
        q.pop();
        for (const Edge& e : g.adj(u))
            if (Graph::isOpen(e) && !seen[e.to]) {
                seen[e.to] = true;
                order.push_back(e.to);
                q.push(e.to);
            }
    }
    *out << g.name(a) << " can reach " << order.size() << " location(s)";
    if (order.empty()) *out << " (isolated).\n";
    else {
        *out << ":\n  ";
        for (size_t i = 0; i < order.size(); i++)
            *out << (i ? ", " : "") << g.name(order[i]) << (g.isShelter(order[i]) ? " [S]" : "");
        *out << "\n";
    }
}
