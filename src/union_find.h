// Union-Find (Disjoint Set Union)
// Used for: "is this area still connected to a shelter?" in ~O(1).
// Path halving (a form of path compression) + union by rank.
#pragma once
#include <utility>
#include <vector>

class UnionFind {
    std::vector<int> parent, rnk;

public:
    explicit UnionFind(int n = 0) { reset(n); }

    void reset(int n) {
        parent.resize(n);
        rnk.assign(n, 0);
        for (int i = 0; i < n; i++) parent[i] = i;
    }

    int find(int x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];  // path halving
            x = parent[x];
        }
        return x;
    }

    void unite(int a, int b) {
        a = find(a);
        b = find(b);
        if (a == b) return;
        if (rnk[a] < rnk[b]) std::swap(a, b);  // union by rank
        parent[b] = a;
        if (rnk[a] == rnk[b]) rnk[a]++;
    }

    bool connected(int a, int b) { return find(a) == find(b); }
};
