#include "NavigationGrid.h"

#include <cmath>
#include <stdexcept>

// Constructor
NavigationGrid::NavigationGrid(
    int mapWidth,
    int mapHeight,
    int tileSize,
    const std::vector<Rectangle>& obstacles
)
    : mapWidth(mapWidth),
      mapHeight(mapHeight),
      tileSize(tileSize),
      columns(0),
      rows(0)
{
    if (mapWidth <= 0 || mapHeight <= 0 || tileSize <= 0) {
        throw std::invalid_argument(
            "Map dimensions and tile size must be positive"
        );
    }

    columns = (mapWidth + tileSize - 1) / tileSize;
    rows = (mapHeight + tileSize - 1) / tileSize;

    walkable.resize(
        rows,
        std::vector<bool>(columns, true)
    );

    BuildGrid(obstacles);
}

// Check whether a tile is walkable
bool NavigationGrid::IsWalkable(int x, int y) const
{
    if (!IsInBounds(x, y)) {
        return false;
    }

    return walkable[y][x];
}

// Check whether grid coordinates are valid
bool NavigationGrid::IsInBounds(int x, int y) const
{
    return x >= 0 && x < columns &&
           y >= 0 && y < rows;
}

// Convert world coordinates (pixels) to grid coordinates
Vector2 NavigationGrid::WorldToGrid(Vector2 position) const
{
    return {
        static_cast<float>(
            std::floor(position.x / tileSize)
        ),
        static_cast<float>(
            std::floor(position.y / tileSize)
        )
    };
}

// Convert grid coordinates to the center of a tile
Vector2 NavigationGrid::GridToWorld(int x, int y) const
{
    return {
        x * static_cast<float>(tileSize) + tileSize / 2.0f,
        y * static_cast<float>(tileSize) + tileSize / 2.0f
    };
}

// Rebuild the grid when obstacles change
void NavigationGrid::UpdateObstacles(
    const std::vector<Rectangle>& obstacles
)
{
    BuildGrid(obstacles);
}

// Generate the navigation grid
void NavigationGrid::BuildGrid(
    const std::vector<Rectangle>& obstacles
)
{
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < columns; x++) {

            // The tile's rectangle in world coordinates
            Rectangle tile = {
                x * static_cast<float>(tileSize),
                y * static_cast<float>(tileSize),
                static_cast<float>(tileSize),
                static_cast<float>(tileSize)
            };

            // Initially assume the tile is walkable
            bool isWalkable = true;

            // Prevent navigation outside the actual map
            if (tile.x + tile.width > mapWidth ||
                tile.y + tile.height > mapHeight) {
                isWalkable = false;
            }

            // Check against every obstacle
            if (isWalkable) {
                for (const Rectangle& obstacle : obstacles) {

                    if (CheckCollisionRecs(tile, obstacle)) {
                        isWalkable = false;
                        break;
                    }
                }
            }

            walkable[y][x] = isWalkable;
        }
    }
}

// Return tile size
int NavigationGrid::GetTileSize() const
{
    return tileSize;
}

// Return number of columns
int NavigationGrid::GetColumns() const
{
    return columns;
}

// Return number of rows
int NavigationGrid::GetRows() const
{
    return rows;
}