#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <queue>
#include <tuple>
#include <vector>

namespace routing {
struct Point {
    float x = 0, y = 0, z = 0;
};
inline float horizontal(Point a, Point b) { return std::hypot(a.x - b.x, a.z - b.z); }
struct Search {
    struct Node {
        int x, z;
        Point point;
        float cost;
        int parent;
    };
    using Key = std::tuple<int, int, int>;
    Point start, goal;
    float spacing = .4f, arrival = .65f;
    bool finished = false, found = false;
    std::vector<Node> nodes;
    std::vector<Point> route;
    std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>,
                        std::greater<std::pair<float, int>>>
        open;
    std::map<Key, float> best;
    std::function<bool(Point, Point &)> sample;
    std::function<bool(Point, Point)> edge;
    int expansions = 0, maxExpansions = 18000;
    static int level(float y) { return (int)std::floor(y / .35f); }
    Search(Point a, Point b, std::function<bool(Point, Point &)> ground,
           std::function<bool(Point, Point)> connection)
        : start(a), goal(b), sample(std::move(ground)), edge(std::move(connection)) {
        nodes.push_back({0, 0, start, 0, -1});
        best[{0, 0, level(start.y)}] = 0;
        open.push({horizontal(start, goal), 0});
    }
    void step(int budget) {
        while (budget-- > 0 && !finished) {
            if (open.empty() || expansions >= maxExpansions) {
                finished = true;
                return;
            }
            int id = open.top().second;
            open.pop();
            Node current = nodes[id];
            auto key = Key{current.x, current.z, level(current.point.y)};
            if (best[key] + .001f < current.cost)
                continue;
            ++expansions;
            if (horizontal(current.point, goal) <= arrival &&
                std::abs(current.point.y - goal.y) < 1.0f) {
                found = finished = true;
                for (int n = id; n >= 0; n = nodes[n].parent)
                    route.push_back(nodes[n].point);
                std::reverse(route.begin(), route.end());
                return;
            }
            for (auto delta :
                 {std::pair{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {-1, 1}, {1, -1}, {1, 1}}) {
                int x = current.x + delta.first, z = current.z + delta.second;
                Point candidate{start.x + x * spacing, current.point.y, start.z + z * spacing},
                    sampled;
                if (candidate.x < std::min(start.x, goal.x) - 8 ||
                    candidate.x > std::max(start.x, goal.x) + 8 ||
                    candidate.z < std::min(start.z, goal.z) - 8 ||
                    candidate.z > std::max(start.z, goal.z) + 8)
                    continue;
                if (!sample(candidate, sampled) || std::abs(sampled.y - current.point.y) > .48f)
                    continue;
                float cost = current.cost + horizontal(sampled, current.point) +
                             std::abs(sampled.y - current.point.y) * .3f;
                Key next{x, z, level(sampled.y)};
                auto old = best.find(next);
                if (old != best.end() && old->second <= cost)
                    continue;
                if (!edge(current.point, sampled))
                    continue;
                best[next] = cost;
                nodes.push_back({x, z, sampled, cost, id});
                int n = (int)nodes.size() - 1;
                open.push(
                    {cost + horizontal(sampled, goal) + std::abs(sampled.y - goal.y) * .2f, n});
            }
        }
    }
};
}
