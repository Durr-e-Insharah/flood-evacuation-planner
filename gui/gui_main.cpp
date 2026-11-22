// Flood Evacuation Planner - SFML 3 GUI
// Same data structures and algorithms as the console version (src/planner.*),
// this file only draws the map and turns clicks into planner commands.
//
// Use:   click a location (FROM), click another (TO), or click a road to pick both ends.
// Keys:  R route | N nearest shelter | C check | V reachable | B block/unblock
//        Up/Down flood +/-5 cm | U undo | Esc clear | P save screenshot.png

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "../src/planner.h"

using namespace std;

namespace {

const float WIN_W = 1280, WIN_H = 680;  // fits a 1366x768 laptop screen
const float PANEL_W = 340, MAP_W = WIN_W - PANEL_W;
const float NODE_R = 14;

const sf::Color C_BG(226, 236, 247), C_WHITE(255, 255, 255), C_BORDER(190, 205, 225);
const sf::Color C_TEXT(25, 35, 55), C_GRAY(100, 112, 130);
const sf::Color C_OPEN(46, 160, 110), C_WARN(245, 160, 20), C_BLOCK(225, 55, 70), C_FLOOD(40, 110, 220);
const sf::Color C_ROUTE(255, 200, 30), C_AREA(60, 120, 215), C_SHELTER(20, 140, 90), C_CUT(225, 55, 70);
const sf::Color C_FROM(30, 90, 200), C_TO(150, 60, 190), C_NAVY(18, 50, 95);

enum Act { A_ROUTE, A_NEAREST, A_CHECK, A_REACH, A_BLOCK, A_UNDO, A_FUP, A_FDOWN, A_CLEAR };

struct Button {
    string label;
    Act act;
    sf::FloatRect rect;
};

bool loadFont(sf::Font& font) {
    const char* paths[] = {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf",
                           "C:/Windows/Fonts/calibri.ttf", "assets/font.ttf",
                           "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                           "/System/Library/Fonts/Supplemental/Arial.ttf"};
    for (const char* p : paths)
        if (font.openFromFile(p)) return true;
    return false;
}

string trim(string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == ' ' || s.back() == '\r')) s.pop_back();
    return s;
}

void drawLine(sf::RenderTarget& w, sf::Vector2f a, sf::Vector2f b, float thick, sf::Color c) {
    sf::Vector2f d = b - a;
    sf::RectangleShape r({std::hypot(d.x, d.y), thick});
    r.setOrigin({0, thick / 2});
    r.setPosition(a);
    r.setRotation(sf::radians(std::atan2(d.y, d.x)));
    r.setFillColor(c);
    w.draw(r);
}

void drawText(sf::RenderTarget& w, const sf::Font& f, const string& s, float x, float y, unsigned size,
              sf::Color col, bool center = false, bool halo = false, bool bold = false) {
    sf::Text t(f, s, size);
    t.setFillColor(col);
    if (bold) t.setStyle(sf::Text::Bold);
    if (halo) {
        t.setOutlineColor(sf::Color(255, 255, 255));
        t.setOutlineThickness(3);
    }
    auto b = t.getLocalBounds();
    t.setOrigin({center ? b.position.x + b.size.x / 2 : 0.f, 0.f});
    t.setPosition({x, y});
    w.draw(t);
}

vector<string> wrapLines(const string& text, const sf::Font& f, unsigned size, float maxW) {
    vector<string> lines;
    istringstream in(text);
    string line;
    while (getline(in, line)) {
        if (line.empty()) {
            lines.push_back("");
            continue;
        }
        istringstream words(line);
        string word, cur;
        while (words >> word) {
            string test = cur.empty() ? word : cur + " " + word;
            sf::Text t(f, test, size);
            if (t.getLocalBounds().size.x > maxW && !cur.empty()) {
                lines.push_back(cur);
                cur = word;
            } else cur = test;
        }
        lines.push_back(cur);
    }
    return lines;
}

float distToSegment(sf::Vector2f p, sf::Vector2f a, sf::Vector2f b) {
    sf::Vector2f ab = b - a;
    float l2 = ab.x * ab.x + ab.y * ab.y;
    float t = l2 == 0 ? 0 : ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / l2;
    t = std::max(0.f, std::min(1.f, t));
    sf::Vector2f q = a + ab * t;
    return std::hypot(p.x - q.x, p.y - q.y);
}

// Turn map coordinates into screen positions (fit to the map area; circle layout if no coordinates).
vector<sf::Vector2f> layout(const Graph& g) {
    int n = g.size();
    vector<sf::Vector2f> pos(n);
    bool all = true;
    for (int v = 0; v < n; v++) all = all && g.hasPos(v);

    if (!all) {
        float r = std::min(MAP_W, WIN_H) / 2 - 90;
        for (int v = 0; v < n; v++) {
            float a = 6.2831853f * v / n;
            pos[v] = {MAP_W / 2 + r * std::cos(a), WIN_H / 2 + r * std::sin(a)};
        }
        return pos;
    }
    float minx = 1e9, maxx = -1e9, miny = 1e9, maxy = -1e9;
    for (int v = 0; v < n; v++) {
        minx = std::min(minx, g.x(v));
        maxx = std::max(maxx, g.x(v));
        miny = std::min(miny, g.y(v));
        maxy = std::max(maxy, g.y(v));
    }
    float w = std::max(1.f, maxx - minx), h = std::max(1.f, maxy - miny);
    float availW = MAP_W - 2 * 80, availH = WIN_H - 2 * 55;
    float s = std::min(availW / w, availH / h);
    float ox = (MAP_W - w * s) / 2, oy = (WIN_H - h * s) / 2;
    for (int v = 0; v < n; v++) pos[v] = {ox + (g.x(v) - minx) * s, oy + (g.y(v) - miny) * s};
    return pos;
}

const Edge* roadBetween(const Graph& g, int a, int b) {
    if (a < 0 || b < 0) return nullptr;
    for (const Edge& e : g.adj(a))
        if (e.to == b) return &e;
    return nullptr;
}

sf::Color roadColor(const Edge& e) {
    if (e.blocked) return C_BLOCK;
    if (e.flood >= FLOOD_LIMIT_CM) return C_FLOOD;
    if (e.flood > 0) return C_WARN;
    return C_OPEN;
}

}  // namespace

int main(int argc, char* argv[]) {
    string mapPath = "data/map.txt", shotPath;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--shot" && i + 1 < argc) shotPath = argv[++i];  // render demo frame to PNG and exit
        else mapPath = a;
    }

    Planner planner;
    ostringstream buf;
    planner.setOutput(buf);
    bool ok = planner.load(mapPath);
    string message = trim(buf.str());
    buf.str("");
    if (!ok) {
        cerr << message << "\n";
        return 1;
    }
    const Graph& g = planner.graph();

    sf::Font font;
    if (!loadFont(font)) {
        cerr << "Could not find a font. Put any .ttf file at assets/font.ttf\n";
        return 1;
    }

    sf::ContextSettings settings;
    settings.antiAliasingLevel = 4;
    sf::RenderWindow window(sf::VideoMode({(unsigned)WIN_W, (unsigned)WIN_H}), "Flood Evacuation Planner",
                            sf::Style::Close | sf::Style::Titlebar, sf::State::Windowed, settings);
    window.setFramerateLimit(60);

    vector<sf::Vector2f> pos = layout(g);
    int from = -1, to = -1;

    // ---- buttons (2 columns)
    vector<Button> buttons;
    {
        struct B {
            const char* label;
            Act act;
        } defs[] = {{"Route (R)", A_ROUTE},        {"Nearest shelter (N)", A_NEAREST},
                    {"Check (C)", A_CHECK},        {"Reachable (V)", A_REACH},
                    {"Block / Unblock (B)", A_BLOCK}, {"Undo (U)", A_UNDO},
                    {"Flood +5 cm (Up)", A_FUP},   {"Flood -5 cm (Down)", A_FDOWN},
                    {"Clear (Esc)", A_CLEAR}};
        float x0 = MAP_W + 16, y0 = 176, bw = 148, bh = 32;
        for (int i = 0; i < 9; i++)
            buttons.push_back({defs[i].label, defs[i].act,
                               sf::FloatRect({x0 + (i % 2) * (bw + 12), y0 + (i / 2) * (bh + 8)}, {bw, bh})});
    }

    auto name = [&](int v) { return g.name(v); };

    auto runAction = [&](Act act) {
        const Edge* e = roadBetween(g, from, to);
        switch (act) {
            case A_ROUTE:
                if (from < 0 || to < 0) message = "Select two locations first: click FROM, then TO.";
                else planner.route(name(from), name(to));
                break;
            case A_NEAREST:
                if (from < 0) message = "Click a location first.";
                else planner.nearest(name(from));
                break;
            case A_CHECK:
                if (from < 0) message = "Click a location first.";
                else planner.check(name(from));
                break;
            case A_REACH:
                if (from < 0) message = "Click a location first.";
                else planner.reachable(name(from));
                break;
            case A_BLOCK:
                if (!e) message = "Select two connected locations (or click a road) first.";
                else if (e->blocked) planner.unblock(name(from), name(to));
                else planner.block(name(from), name(to));
                break;
            case A_FUP:
            case A_FDOWN:
                if (!e) message = "Select two connected locations (or click a road) first.";
                else planner.setFlood(name(from), name(to), std::max(0, e->flood + (act == A_FUP ? 5 : -5)));
                break;
            case A_UNDO: planner.undo(); break;
            case A_CLEAR:
                from = to = -1;
                planner.clearPath();
                message = "Selection cleared.";
                break;
        }
        string out = trim(buf.str());
        buf.str("");
        if (!out.empty()) message = out;
    };

    auto selectNode = [&](int v) {
        planner.clearPath();
        if (from < 0 || to >= 0) {
            from = v;
            to = -1;
        } else if (v != from) to = v;
    };

    auto saveShot = [&](const string& path) {
        sf::Texture tex(window.getSize());
        tex.update(window);
        return tex.copyToImage().saveToFile(path);
    };

    // ---- scripted frame for --shot (used to make a screenshot for the report)
    if (!shotPath.empty()) {
        auto pick = [&](const char* a, const char* b) {
            from = g.id(a);
            to = g.id(b);
        };
        pick("Bridge", "Main_Chowk");
        runAction(A_BLOCK);
        pick("Old_Bridge", "Canal_Chowk");
        runAction(A_FUP);
        runAction(A_FUP);
        pick("Colony_B", "Bridge");
        runAction(A_BLOCK);
        pick("Colony_A", "School_Shelter");
        runAction(A_ROUTE);
    }

    int frames = 0;
    while (window.isOpen()) {
        while (const optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mb->button != sf::Mouse::Button::Left) continue;
                sf::Vector2f m(static_cast<float>(mb->position.x), static_cast<float>(mb->position.y));
                bool handled = false;
                for (const Button& b : buttons)
                    if (b.rect.contains(m)) {
                        runAction(b.act);
                        handled = true;
                    }
                if (handled || m.x > MAP_W) continue;

                int hit = -1;
                for (int v = 0; v < g.size(); v++)
                    if (std::hypot(m.x - pos[v].x, m.y - pos[v].y) <= NODE_R + 6) hit = v;
                if (hit >= 0) {
                    selectNode(hit);
                    continue;
                }
                float best = 9;  // click on a road: select both ends
                for (int u = 0; u < g.size(); u++)
                    for (const Edge& e : g.adj(u)) {
                        if (u > e.to) continue;
                        float d = distToSegment(m, pos[u], pos[e.to]);
                        if (d < best) {
                            best = d;
                            from = u;
                            to = e.to;
                            planner.clearPath();
                        }
                    }
            } else if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                using K = sf::Keyboard::Key;
                switch (kp->code) {
                    case K::R: runAction(A_ROUTE); break;
                    case K::N: runAction(A_NEAREST); break;
                    case K::C: runAction(A_CHECK); break;
                    case K::V: runAction(A_REACH); break;
                    case K::B: runAction(A_BLOCK); break;
                    case K::U: runAction(A_UNDO); break;
                    case K::Up: runAction(A_FUP); break;
                    case K::Down: runAction(A_FDOWN); break;
                    case K::Escape: runAction(A_CLEAR); break;
                    case K::P: message = saveShot("screenshot.png") ? "Saved screenshot.png" : "Could not save screenshot."; break;
                    default: break;
                }
            }
        }

        sf::Vector2f mouse(static_cast<float>(sf::Mouse::getPosition(window).x),
                           static_cast<float>(sf::Mouse::getPosition(window).y));
        const Edge* sel = roadBetween(g, from, to);

        window.clear(C_BG);

        // ---- map card
        sf::RectangleShape card({MAP_W - 20, WIN_H - 20});
        card.setPosition({10, 10});
        card.setFillColor(sf::Color(247, 251, 255));
        card.setOutlineColor(C_BORDER);
        card.setOutlineThickness(2);
        window.draw(card);

        // ---- route highlight (gold glow under the roads)
        const vector<int>& path = planner.lastPath().path;
        for (size_t i = 0; i + 1 < path.size(); i++) drawLine(window, pos[path[i]], pos[path[i + 1]], 14, C_ROUTE);

        // ---- roads
        for (int u = 0; u < g.size(); u++)
            for (const Edge& e : g.adj(u)) {
                if (u > e.to) continue;
                sf::Vector2f a = pos[u], b = pos[e.to], mid = (a + b) / 2.f;
                bool isSel = (u == from && e.to == to) || (u == to && e.to == from);
                if (isSel) drawLine(window, a, b, 10, sf::Color(40, 50, 70));
                drawLine(window, a, b, 4, roadColor(e));
                if (!Graph::isOpen(e)) {  // little X on closed roads
                    drawLine(window, mid + sf::Vector2f(-6, -6), mid + sf::Vector2f(6, 6), 3, roadColor(e));
                    drawLine(window, mid + sf::Vector2f(-6, 6), mid + sf::Vector2f(6, -6), 3, roadColor(e));
                }
                string lab = to_string(e.dist) + "m";
                if (e.flood > 0) lab += "  " + to_string(e.flood) + "cm";
                drawText(window, font, lab, mid.x, mid.y + 7, 11, C_GRAY, true, true);
            }

        // ---- nodes
        for (int v = 0; v < g.size(); v++) {
            if (v == from || v == to) {
                sf::CircleShape ring(NODE_R + 8, 40);
                ring.setOrigin({NODE_R + 8, NODE_R + 8});
                ring.setPosition(pos[v]);
                ring.setFillColor(sf::Color(255, 255, 255, 0));
                ring.setOutlineThickness(4);
                ring.setOutlineColor(v == from ? C_FROM : C_TO);
                window.draw(ring);
            }
            if (g.isShelter(v)) {
                sf::RectangleShape box({NODE_R * 2, NODE_R * 2});
                box.setOrigin({NODE_R, NODE_R});
                box.setPosition(pos[v]);
                box.setFillColor(C_SHELTER);
                box.setOutlineThickness(2);
                box.setOutlineColor(C_WHITE);
                window.draw(box);
                drawText(window, font, "S", pos[v].x, pos[v].y - 11, 18, C_WHITE, true, false, true);
            } else {
                bool cut = !planner.connectedToShelter(v);
                sf::CircleShape c(NODE_R, 40);
                c.setOrigin({NODE_R, NODE_R});
                c.setPosition(pos[v]);
                c.setFillColor(cut ? C_CUT : C_AREA);
                c.setOutlineThickness(2);
                c.setOutlineColor(C_WHITE);
                window.draw(c);
            }
            drawText(window, font, g.name(v), pos[v].x, pos[v].y + NODE_R + 3, 13, C_TEXT, true, true);
        }

        // ---- statistics box (top-left of the map)
        {
            int openRoads = 0, manualBlocked = 0, floodClosed = 0, cutOff = 0;
            for (int u = 0; u < g.size(); u++) {
                for (const Edge& e : g.adj(u)) {
                    if (u > e.to) continue;
                    if (Graph::isOpen(e)) openRoads++;
                    else if (e.blocked) manualBlocked++;
                    else floodClosed++;
                }
                if (!g.isShelter(u) && !planner.connectedToShelter(u)) cutOff++;
            }
            sf::RectangleShape sb({200, 106});
            sb.setPosition({22, 22});
            sb.setFillColor(sf::Color(246, 249, 253));
            sb.setOutlineColor(C_BORDER);
            sb.setOutlineThickness(1);
            window.draw(sb);
            drawText(window, font, "STATISTICS", 32, 28, 13, C_NAVY, false, false, true);
            drawText(window, font, "Open roads: " + to_string(openRoads), 32, 47, 13, C_TEXT);
            drawText(window, font, "Blocked roads: " + to_string(manualBlocked), 32, 63, 13, C_TEXT);
            drawText(window, font, "Closed by flood: " + to_string(floodClosed), 32, 79, 13, C_TEXT);
            drawText(window, font, "Areas cut off: " + to_string(cutOff), 32, 95, 13, cutOff > 0 ? C_CUT : C_TEXT);
        }

        // ---- right panel
        float px = MAP_W + 10;
        sf::RectangleShape panel({PANEL_W - 10, WIN_H - 20});
        panel.setPosition({px, 10});
        panel.setFillColor(C_WHITE);
        panel.setOutlineColor(C_BORDER);
        panel.setOutlineThickness(2);
        window.draw(panel);

        // navy header band
        sf::RectangleShape headerBand({PANEL_W - 10, 64});
        headerBand.setPosition({px, 10});
        headerBand.setFillColor(C_NAVY);
        window.draw(headerBand);

        float tx = px + 16;
        drawText(window, font, "Flood Evacuation Planner", tx, 22, 20, C_WHITE, false, false, true);
        drawText(window, font, "Click FROM, then TO  (or click a road)", tx, 50, 13, sf::Color(200, 215, 235));

        // selection info
        drawText(window, font, "FROM:", tx, 80, 15, C_FROM, false, false, true);
        drawText(window, font, from >= 0 ? name(from) : "-", tx + 60, 80, 15, C_TEXT);
        drawText(window, font, "TO:", tx, 102, 15, C_TO, false, false, true);
        drawText(window, font, to >= 0 ? name(to) : "-", tx + 60, 102, 15, C_TEXT);
        string roadInfo = "Road: select two connected locations";
        if (sel) {
            string st = sel->blocked ? "CLOSED (blocked)" : sel->flood >= FLOOD_LIMIT_CM ? "CLOSED (flood)" : "OPEN";
            roadInfo = "Road: " + to_string(sel->dist) + " m, water " + to_string(sel->flood) + " cm, " + st;
        } else if (from >= 0 && to >= 0) roadInfo = "No direct road between these two";
        drawText(window, font, roadInfo, tx, 128, 13, C_TEXT);
        drawText(window, font, "Water >= " + to_string(FLOOD_LIMIT_CM) + " cm closes a road automatically.", tx, 148, 12, C_GRAY);

        // buttons
        for (const Button& b : buttons) {
            bool hover = b.rect.contains(mouse);
            sf::RectangleShape r(b.rect.size);
            r.setPosition(b.rect.position);
            r.setFillColor(hover ? sf::Color(40, 110, 190) : C_NAVY);
            window.draw(r);
            drawText(window, font, b.label, b.rect.position.x + b.rect.size.x / 2, b.rect.position.y + 8, 13, C_WHITE, true);
        }
        // result box
        float ry = 176 + 5 * 40 + 8;
        drawText(window, font, "RESULT", tx, ry, 14, C_NAVY, false, false, true);
        sf::RectangleShape rb({PANEL_W - 42, 118});
        rb.setPosition({tx, ry + 24});
        rb.setFillColor(sf::Color(246, 249, 253));
        rb.setOutlineColor(C_BORDER);
        rb.setOutlineThickness(1);
        window.draw(rb);
        vector<string> lines = wrapLines(message, font, 13, PANEL_W - 62);
        float ly = ry + 32;
        for (size_t i = 0; i < lines.size() && i < 7; i++, ly += 17) drawText(window, font, lines[i], tx + 8, ly, 13, C_TEXT);

        // ---- estimated travel time for the current route
        if (path.size() >= 2) {
            int meters = planner.lastPath().dist;
            int walkMin = (int)std::ceil(meters / 83.3);
            int carMin = (int)std::ceil(meters / 500.0);
            drawText(window, font, "Walking: ~" + to_string(walkMin) + " min   Vehicle: ~" + to_string(carMin) + " min",
                     tx, ry + 24 + 118 + 6, 13, C_NAVY, false, false, true);
        }
        // legend
        float lgY = WIN_H - 112;
        struct L {
            const char* text;
            sf::Color col;
            bool square;
        } legend[] = {{"Open road", C_OPEN, false},      {"Water 1-29 cm", C_WARN, false},
                      {"Blocked road", C_BLOCK, false},  {"Closed by flood", C_FLOOD, false},
                      {"Route", C_ROUTE, false},         {"Shelter", C_SHELTER, true},
                      {"Area", C_AREA, true},            {"Area cut off", C_CUT, true}};
        for (int i = 0; i < 8; i++) {
            float lx = tx + (i % 2) * 158, lyy = lgY + (i / 2) * 24;
            sf::RectangleShape sw(legend[i].square ? sf::Vector2f(14, 14) : sf::Vector2f(26, 6));
            sw.setPosition({lx, lyy + (legend[i].square ? 2.f : 6.f)});
            sw.setFillColor(legend[i].col);
            window.draw(sw);
            drawText(window, font, legend[i].text, lx + 34, lyy, 13, C_TEXT);
        }
        window.display();
        if (!shotPath.empty() && ++frames == 3) {
            bool saved = saveShot(shotPath);
            cout << (saved ? "Saved " : "Failed to save ") << shotPath << "\n";
            return saved ? 0 : 1;
        }
    }
    return 0;
}
