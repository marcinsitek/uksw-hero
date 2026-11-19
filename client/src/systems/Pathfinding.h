#pragma once

#include "raylib.h"
#include <vector>

class NavigationGrid;

class Pathfinding {
public:
    // Find a path from start to goal using the A* algorithm.
    //
    // start: Goblin's current position in world coordinates.
    // goal:  Student's current position in world coordinates.
    // grid:  Shared navigation grid containing blocked tiles.
    //
    // Returns:
    // A sequence of waypoints in world coordinates (pixels).
    // Each waypoint represents the center of a walkable tile.
    //
    // Returns an empty vector if no path exists.
    static std::vector<Vector2> FindPath(
        Vector2 start,
        Vector2 goal,
        const NavigationGrid& grid
    );

private:
    // Manhattan distance heuristic for four-directional movement.
    static int Heuristic(
        int x1,
        int y1,
        int x2,
        int y2
    );
};
