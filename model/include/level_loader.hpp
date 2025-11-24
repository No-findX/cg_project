#ifndef LEVEL_LOADER_HPP
#define LEVEL_LOADER_HPP

#define MAX_SIZE 12

#include "third_party/json.hpp"

#include <string>
#include <vector>
#include <array>

using json = nlohmann::json;

/// @brief A scene representing the 2D layout of a level
/// @details Contains the fixed walls ("#") and movable spaces/portals (".") in a level.
///          Each cell is represented as a string to accommodate various game elements.
using Scene = std::array<std::array<std::string, MAX_SIZE>, MAX_SIZE>;

/// @brief Represents a single room in the game level
/// @details A room can be either a regular room or a "box room" that can be entered
///          by pushing the corresponding numbered box into it.
struct Room
{
    int size;                                    ///< Size of the room (width and height)
    bool is_box;                                ///< True if this room can be entered via a box
    std::vector<std::array<int, 2>> entries;    ///< Entry points [y, x] where player can enter this room
    Scene scene;                                ///< 2D layout of the room
};

/// @brief Represents a complete game level
/// @details Contains all rooms and metadata for a level, loaded from JSON configuration
struct Level
{
    int id;                        ///< Unique identifier for the level
    int room_num;                  ///< Number of rooms in this level
    std::vector<Room> rooms;       ///< All rooms in the level, sorted by room ID (0, 1, ...)
};

/// @brief Utility class for loading game levels from JSON files
/// @details This class provides static methods to parse JSON level files and convert them
///          into Level structures that can be used by the game engine.
class LevelLoader{
public:
    /// @brief Load a level from a JSON file
    /// @param level_path Path to the level file (e.g., "path/to/level.json")
    /// @return Level structure containing the parsed level data
    /// @throws std::runtime_error if file cannot be read or JSON is invalid
    static Level loadLevel(const std::string& level_path);

    LevelLoader() = delete;
    LevelLoader(const LevelLoader&) = delete;
    LevelLoader& operator=(const LevelLoader&) = delete;
};

#endif