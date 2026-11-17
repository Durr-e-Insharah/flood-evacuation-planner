#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>

#include "planner.h"

using namespace std;

static void printHelp() {
    cout << "\nCOMMANDS\n"
            "  SHOW                     list all locations and roads with their status\n"
            "  ROUTE a b                shortest open route from a to b\n"
            "  NEAREST a                nearest reachable shelter from a\n"
            "  CHECK a                  is a still connected to any shelter?\n"
            "  REACHABLE a              every location a can still reach\n"
            "  BLOCK a b                close the road between a and b\n"
            "  UNBLOCK a b              reopen a manually blocked road\n"
            "  FLOOD a b cm             set water level (>= " << FLOOD_LIMIT_CM << " cm closes the road)\n"
            "  UNDO                     undo the last BLOCK / UNBLOCK / FLOOD\n"
            "  LOAD file                load another map file\n"
            "  HELP                     show this list\n"
            "  EXIT                     quit\n\n";
}

int main(int argc, char* argv[]) {
    string mapPath = argc > 1 ? argv[1] : "data/map.txt";

    cout << "=== Flood Evacuation Planner ===\n";
    Planner planner;
    if (!planner.load(mapPath)) cout << "Use LOAD <file> to load a map.\n";
    printHelp();

    string line;
    while (cout << "> ", getline(cin, line)) {
        istringstream ss(line);
        vector<string> t;
        for (string w; ss >> w;) t.push_back(w);
        if (t.empty()) continue;

        string cmd = t[0];
        transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c) { return toupper(c); });
        size_t n = t.size();

        if (cmd == "EXIT" || cmd == "QUIT") break;
        else if (cmd == "HELP") printHelp();
        else if (cmd == "SHOW") planner.show();
        else if (cmd == "UNDO") planner.undo();
        else if (cmd == "LOAD" && n == 2) planner.load(t[1]);
        else if (cmd == "ROUTE" && n == 3) planner.route(t[1], t[2]);
        else if (cmd == "NEAREST" && n == 2) planner.nearest(t[1]);
        else if (cmd == "CHECK" && n == 2) planner.check(t[1]);
        else if (cmd == "REACHABLE" && n == 2) planner.reachable(t[1]);
        else if (cmd == "BLOCK" && n == 3) planner.block(t[1], t[2]);
        else if (cmd == "UNBLOCK" && n == 3) planner.unblock(t[1], t[2]);
        else if (cmd == "FLOOD" && n == 4) {
            try {
                planner.setFlood(t[1], t[2], stoi(t[3]));
            } catch (...) {
                cout << "Usage: FLOOD a b cm   (cm must be a number)\n";
            }
        } else cout << "Unknown command or wrong arguments. Type HELP.\n";
    }
    cout << "\nStay safe. Bye!\n";
    return 0;
}
