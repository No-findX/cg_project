#include "../include/gameplay.hpp"

#include <iostream>

bool operator== (const Pos& a, const Pos& b)
{
    return a.room == b.room && a.x == b.x && a.y == b.y;
}

GamePlay::GamePlay(const Level level)
{
    rooms = level.rooms;
    
    // Initialize game state by parsing the initial layout
    currState.portal_just_passed = std::nullopt;
    currState.is_win = false;
    
    int boxid = 0;

    // Find initial positions of player, boxes, and destinations
    for (int room = 0; room < rooms.size(); ++room) {
        for (int y = 0; y < rooms[room].size && y < MAX_SIZE; ++y) {
            for (int x = 0; x < rooms[room].size && x < MAX_SIZE; ++x) {
                std::string cell = rooms[room].scene[y][x];
                
                if (cell == "p") {
                    currState.player = {room, x, y};
                    rooms[room].scene[y][x] = "."; // Replace player with empty space
                }
                else if (cell == "b") {
                    currState.boxes[boxid] = {room, x, y};
                    rooms[room].scene[y][x] = "."; // Replace box with empty space
                    boxid++;
                }
                else if (cell == "=") {
                    playerDestination = {room, x, y};
                }
                else if (cell == "_") {
                    boxDestinations.push_back({room, x, y});
                }
                else if (cell >= "0" && cell <= "9") {
                    currState.boxrooms[(cell[0] - '0')] = {room, x, y};
                    rooms[room].scene[y][x] = "."; // Replace box with empty space
                }
            }
        }
    }
    
    nextState = currState;
}

GameState GamePlay::getCurrState()
{
    return currState;
}

GameState GamePlay::getNextState()
{
    return nextState;
}

CellType GamePlay::getCellType(Pos pos)
{
    if (rooms[pos.room].scene[pos.y][pos.x] == "#" || rooms[pos.room].scene[pos.y][pos.x] == "|") {
        return WALL;
    }

    if (currState.player == pos) {
        return PLAYER;
    }

    for (const auto& [bid, box]: currState.boxes) {
        if (box == pos) {
            return BOX;
        }
    }

    for (const auto& [rid, boxroom]: currState.boxrooms) {
        if (boxroom == pos) {
            return BOXROOM;
        }
    }

    return SPACE;
}

CellType GamePlay::operateMove(CellType object_to_move, Pos object_curr_pos, Input move)
{
    // base case: unmovable objects
    if (object_to_move == WALL || object_to_move == SPACE) {
        return object_to_move;
    }

    int dx = 0, dy = 0;
    switch (move) {
        case UP: dy = -1; break;
        case DOWN: dy = 1; break;
        case LEFT: dx = -1; break;
        case RIGHT: dx = 1; break;
    }

    // check if the object goes out from a box-room
    Pos next_pos;
    bool is_going_out = false;
    for (const auto& entry: rooms[object_curr_pos.room].entries) {
        if ((entry[0] == object_curr_pos.y && entry[1] == object_curr_pos.x)
            && ((entry[0] == 0 && move == UP)
                || (entry[0] == rooms[object_curr_pos.room].size - 1 && move == DOWN)
                || (entry[1] == 0 && move == LEFT)
                || (entry[1] == rooms[object_curr_pos.room].size - 1 && move == RIGHT)))
        {
            Pos boxroom_pos = currState.boxrooms[object_curr_pos.room];
            next_pos = {boxroom_pos.room, boxroom_pos.x + dx, boxroom_pos.y + dy};
            nextState.portal_just_passed = std::optional<Pos>(object_curr_pos);
            is_going_out = true;
            break;
        }
    }
    if (!is_going_out) {
        next_pos = {object_curr_pos.room, object_curr_pos.x + dx, object_curr_pos.y + dy};
    }

    // try to push the object that occupies next_pos
    CellType object_to_push = getCellType(next_pos);
    CellType target_after_push = operateMove(object_to_push, next_pos, move);

    // the occupying object is a box-room, try to move in
    if (target_after_push == BOXROOM) {
        int boxroom_id = 0;
        for (const auto& [rid, boxroom]: currState.boxrooms) {
            if (boxroom == next_pos) {
                boxroom_id = rid;
                break;
            }
        }

        bool can_enter = false;
        for (const auto& entry: rooms[boxroom_id].entries) {
            if ((move == UP && entry[0] == rooms[boxroom_id].size - 1)
                || (move == DOWN && entry[0] == 0)
                || (move == LEFT && entry[1] == rooms[boxroom_id].size - 1)
                || (move == RIGHT && entry[1] == 0))
            {
                can_enter = true;
                next_pos = {boxroom_id, entry[1], entry[0]};
                nextState.portal_just_passed = std::optional<Pos>(next_pos);
                break;
            }
        }

        // can't enter
        if (!can_enter) {
            return object_to_move;
        }

        // cross the portal and enter
        CellType new_object_to_push = getCellType(next_pos);
        target_after_push = operateMove(new_object_to_push, next_pos, move);
    }

    // unable to push the object away from the target
    if (target_after_push == WALL || target_after_push == BOX || target_after_push == PLAYER) {
        return object_to_move;
    }

    // push the object away and move into the target
    if (target_after_push == SPACE) {
        switch (object_to_move)
        {
            case PLAYER:
                nextState.player = next_pos;
                break;
            case BOX:
                for (const auto& [bid, box]: currState.boxes) {
                    if (box == object_curr_pos) {
                        nextState.boxes[bid] = next_pos;
                        break;
                    }
                }
                break;
            case BOXROOM:
                for (const auto& [rid, boxroom]: currState.boxrooms) {
                    if (boxroom == object_curr_pos) {
                        nextState.boxrooms[rid] = next_pos;
                        break;
                    }
                }
                break;            
        }
        return SPACE;
    }

}

void GamePlay::operate(Input input)
{
    nextState = currState;
    nextState.portal_just_passed = std::nullopt;
    
    operateMove(PLAYER, currState.player, input);
    
    // Check win condition
    nextState.is_win = (nextState.player == playerDestination);
    
    bool all_destinations_exist_box = true;
    for (const auto& dest: boxDestinations) {
        bool exist_box = false;
        for (const auto& [_, box]: nextState.boxes) {
            if (box == dest) {
                exist_box = true;
                break;
            }
        }
        if (!exist_box) {
            for (const auto& [_, boxroom]: nextState.boxrooms) {
                if (boxroom == dest) {
                    exist_box = true;
                    break;
                }
            }
        }

        if (!exist_box) {
            all_destinations_exist_box = false;
            break;
        }
    }
    
    nextState.is_win = nextState.is_win && all_destinations_exist_box;
}

void GamePlay::updateState()
{
    currState = nextState;
}
