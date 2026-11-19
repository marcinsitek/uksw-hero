#include "Pathfinding.h"
#include "NavigationGrid.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

namespace {

    struct Node {
        int x;
        int y;
        int g;  // Cost from start
        int f;  // g + heuristic
    };

    // Priority queue returns the node with the lowest f.
    struct CompareNodes {
        bool operator()(const Node& a, const Node& b) const {
            return a.f > b.f;
        }
    };

}

// Manhattan distance heuristic
int Pathfinding::Heuristic(
    int x1,
    int y1,
    int x2,
    int y2
)
{
    return std::abs(x1 - x2) + std::abs(y1 - y2);
}

// Find a path using A*
std::vector<Vector2> Pathfinding::FindPath(
    Vector2 start,
    Vector2 goal,
    const NavigationGrid& grid
)
{
    // Convert world coordinates to grid coordinates.
    Vector2 startTile = grid.WorldToGrid(start);
    Vector2 goalTile = grid.WorldToGrid(goal);

    int startX = static_cast<int>(startTile.x);
    int startY = static_cast<int>(startTile.y);

    int goalX = static_cast<int>(goalTile.x);
    int goalY = static_cast<int>(goalTile.y);

    // Both positions must be on walkable tiles.
    if (!grid.IsWalkable(startX, startY) ||
        !grid.IsWalkable(goalX, goalY)) {
        return {};
    }

    const int columns = grid.GetColumns();
    const int rows = grid.GetRows();
    const int totalCells = columns * rows;

    // Convert 2D coordinates to a 1D index.
    auto Index = [columns](int x, int y) {
        return y * columns + x;
    };

    const int INF = std::numeric_limits<int>::max();

    // Best known cost to each tile.
    std::vector<int> gCost(totalCells, INF);

    // Parent tile for reconstructing the path.
    std::vector<int> parent(totalCells, -1);

    // Tiles already processed.
    std::vector<bool> closed(totalCells, false);

    // Tiles waiting to be processed.
    std::priority_queue<
        Node,
        std::vector<Node>,
        CompareNodes
    > open;

    int startIndex = Index(startX, startY);

    gCost[startIndex] = 0;

    open.push({
        startX,
        startY,
        0,
        Heuristic(startX, startY, goalX, goalY)
    });

    // Four possible movement directions.
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};

    while (!open.empty()) {

        Node current = open.top();
        open.pop();

        int currentIndex = Index(current.x, current.y);

        // Skip already processed tiles.
        if (closed[currentIndex]) {
            continue;
        }

        closed[currentIndex] = true;

        // Goal reached.
        if (current.x == goalX &&
            current.y == goalY) {

            std::vector<Vector2> path;

            // Reconstruct the path from goal to start.
            int index = currentIndex;

            while (index != -1) {

                int x = index % columns;
                int y = index / columns;

                path.push_back(
                    grid.GridToWorld(x, y)
                );

                index = parent[index];
            }

            // Reverse to get start -> goal order.
            std::reverse(path.begin(), path.end());

            return path;
        }

        // Examine neighboring tiles.
        for (int i = 0; i < 4; i++) {

            int nextX = current.x + dx[i];
            int nextY = current.y + dy[i];

            // Skip blocked or out-of-bounds tiles.
            if (!grid.IsWalkable(nextX, nextY)) {
                continue;
            }

            int nextIndex = Index(nextX, nextY);

            if (closed[nextIndex]) {
                continue;
            }

            // Each horizontal/vertical step costs 1.
            int newCost = gCost[currentIndex] + 1;

            // Update if a better path is found.
            if (newCost < gCost[nextIndex]) {

                gCost[nextIndex] = newCost;
                parent[nextIndex] = currentIndex;

                int f = newCost + Heuristic(
                    nextX,
                    nextY,
                    goalX,
                    goalY
                );

                open.push({
                    nextX,
                    nextY,
                    newCost,
                    f
                });
            }
        }
    }

    // No path found.
    return {};
};