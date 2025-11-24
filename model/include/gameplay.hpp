#ifndef GAMEPLAY_HPP
#define GAMEPLAY_HPP

#include "level_loader.hpp"

#include <vector>
#include <optional>
#include <map>

/// @brief Position of a character (player/box) in the game world
/// @details Represents a 3D coordinate system where characters can exist in different rooms
///          and have 2D coordinates within each room.
struct Pos
{
    int room;    ///< Room ID where the character is located
    int x;       ///< X position in the room (0 = leftmost, C-style array indexing)
    int y;       ///< Y position in the room (0 = topmost, C-style array indexing)
};
bool operator== (const Pos& a, const Pos& b);

/// @brief The state of the game, including character positions and game status
/// @details Contains all information needed to represent the current state of the game,
///          including player position, box positions, portal usage, and win condition.
struct GameState
{
    Pos player;                                ///< Current position of the player
    std::map<int, Pos> boxes;                  ///< Positions of all boxes (box_id -> position)
    std::map<int, Pos> boxrooms;               ///< Positions of all box rooms (room_id -> position)
    std::optional<Pos> portal_just_passed;    ///< Portal position if player just used one (for rendering)
    bool is_win;                              ///< True if the player has won the game
};

/// @brief Player input directions for movement
enum Input
{
    UP,      
    DOWN,    
    LEFT,   
    RIGHT   
};

/// @brief Types of cells that can exist in the game world
enum CellType
{
    PLAYER,   
    WALL,      
    SPACE,    
    BOX,      
    BOXROOM   
};

/// @brief Game manager for sokoban game, handling game logic and maintaining game state
/// @details This class manages the game mechanics, processes player input, updates game state,
///          and handles the special portal mechanics unique to this variant of sokoban.
class GamePlay
{
public:
    /// @brief Initialize a game with a loaded level
    /// @param level The level data containing rooms and initial layout
    GamePlay(const Level level);

    /// @brief Get the current game state
    /// @return Current game state including all character positions
    GameState getCurrState();

    /// @brief Get the next game state (after pending operations are applied)
    /// @return The game state that will become current after updateState() is called
    GameState getNextState();

    /// @brief Process player input and calculate the resulting game state
    /// @param input Player's move direction (UP/DOWN/LEFT/RIGHT)
    void operate(Input input);

    /// @brief Apply the pending state changes and make them current
    /// @details Replaces current state with next state and clears the next state
    void updateState();
private:
    Pos playerDestination;                     ///< Calculated destination for player movement
    std::vector<Pos> boxDestinations;          ///< Calculated destinations for box movements
    std::vector<Room> rooms;                   ///< All rooms in the current level
    GameState currState;                       ///< Current state of the game
    GameState nextState;                       ///< Next state after operations are applied


    CellType getCellType(Pos cell_pos);
    CellType operateMove(CellType object_to_move, Pos object_curr_pos, Input move);
};

#endif