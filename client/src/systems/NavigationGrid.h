
#pragma once

#include "raylib.h"
#include <vector>

class NavigationGrid {
public:
    // Creates a navigation grid from the arena dimensions
    // and a collection of rectangular obstacles.
    NavigationGrid(
        int mapWidth,
        int mapHeight,
        int tileSize,
        const std::vector<Rectangle>& obstacles
    );

    // Returns true if the tile can be traversed.
    // Out-of-bounds tiles are considered blocked.
    bool IsWalkable(int x, int y) const;

    // Converts world coordinates (pixels) to grid coordinates.
    // The returned Vector2 contains integer-valued x and y.
    Vector2 WorldToGrid(Vector2 position) const;

    // Returns the world position of the center of a tile.
    Vector2 GridToWorld(int x, int y) const;

    // Returns true if the tile is inside the grid.
    bool IsInBounds(int x, int y) const;

    // Rebuilds the grid when obstacles change.
    void UpdateObstacles(
        const std::vector<Rectangle>& obstacles
    );

    // Grid properties.
    int GetTileSize() const;
    int GetColumns() const;
    int GetRows() const;

private:
    int mapWidth;
    int mapHeight;
    int tileSize;
    int columns;
    int rows;

    // true = walkable, false = blocked
    std::vector<std::vector<bool>> walkable;

    // Generates walkable tiles from obstacle rectangles.
    void BuildGrid(
        const std::vector<Rectangle>& obstacles
    );
};