#include "route_planner.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace routing;
    auto floor = [](Point p, Point &out) {
        out = p;
        out.y = 0;
        return !(p.x > 1 && p.x < 2 && p.z > -.8 && p.z < .8);
    };
    auto clear = [&](Point a, Point b) {
        for (int i = 0; i <= 8; ++i) {
            float t = i / 8.f;
            Point p{a.x + (b.x - a.x) * t, 0, a.z + (b.z - a.z) * t}, v;
            if (!floor(p, v))
                return false;
        }
        return true;
    };
    Search detour({0, 0, 0}, {3, 0, 0}, floor, clear);
    while (!detour.finished)
        detour.step(20);
    assert(detour.found && detour.route.size() > 2);
    bool around = false;
    for (auto p : detour.route)
        if (std::abs(p.z) >= .8)
            around = true;
    assert(around);
    Search blocked(
        {0, 0, 0}, {3, 0, 0},
        [](Point p, Point &o) {
            o = p;
            return p.x < .5;
        },
        [](Point, Point) { return true; });
    blocked.maxExpansions = 50;
    while (!blocked.finished)
        blocked.step(10);
    assert(!blocked.found && blocked.expansions <= 50);
    Search cliff(
        {0, 0, 0}, {2, 3, 0},
        [](Point p, Point &o) {
            o = p;
            o.y = p.x > .5 ? 3 : 0;
            return true;
        },
        [](Point, Point) { return true; });
    cliff.maxExpansions = 80;
    while (!cliff.finished)
        cliff.step(10);
    assert(!cliff.found);
    std::cout << "Route detour, blocked-route limit and cliff rejection passed.\n";
}
