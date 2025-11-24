#include "interface.hpp"

#include <iostream>
#include <iomanip>

Interface::Interface(const Level level)
{
    for (const auto& room: level.rooms){
        scenes.push_back(room.scene);
    }
}

void Interface::renderBegin()
{
    std::cout << "=== Portal Parabox Game ===" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  w - Up" << std::endl;
    std::cout << "  s - Down" << std::endl;
    std::cout << "  a - Left" << std::endl;
    std::cout << "  d - Right" << std::endl;
    std::cout << "  q - Quit" << std::endl;
    std::cout << std::endl;
    std::cout << "Legend:" << std::endl;
    std::cout << "  # - Wall" << std::endl;
    std::cout << "  . - Empty space" << std::endl;
    std::cout << "  P - Player" << std::endl;
    std::cout << "  B - Box" << std::endl;
    std::cout << "  = - Player destination" << std::endl;
    std::cout << "  _ - Box destination" << std::endl;
    std::cout << "  1,2,3... - Portal box (enter to go to corresponding room)" << std::endl;
    std::cout << "===========================" << std::endl;
}

void Interface::render(GameState curr_state, GameState next_state)
{
    // Clear screen (Windows compatible)
    system("cls");
    
    renderBegin();
    
    // Display current room
    std::cout << "Current Room: " << next_state.player.room << std::endl;
    
    if (next_state.portal_just_passed.has_value()) {
        std::cout << "Portal used! Traveled from room " << next_state.portal_just_passed->room 
                  << " to room " << next_state.player.room << std::endl;
    }
    
    if (next_state.is_win) {
        std::cout << "*** CONGRATULATIONS! YOU WON! ***" << std::endl;
    }
    
    std::cout << std::endl;
    
    // Create a copy of the current room scene for rendering
    Scene display_scene = scenes[next_state.player.room];
    
    // Place boxes on the display scene
    for (const auto& [bid, box] : next_state.boxes) {
        if (box.room == next_state.player.room) {
            // Check if this position has a room identifier
            std::string original_cell = scenes[box.room][box.y][box.x];
            display_scene[box.y][box.x] = "B"; // Regular box
        }
    }
    for (const auto& [rid, boxroom] : next_state.boxrooms) {
        if (boxroom.room == next_state.player.room) {
            std::string original_cell = scenes[boxroom.room][boxroom.y][boxroom.x];
            display_scene[boxroom.y][boxroom.x] = std::to_string(rid);
        }
    }
    
    // Place player on the display scene
    display_scene[next_state.player.y][next_state.player.x] = "P";
    
    // Render the scene
    for (int y = 0; y < MAX_SIZE; ++y) {
        for (int x = 0; x < MAX_SIZE; ++x) {
            std::string cell = display_scene[y][x];
            
            // Color coding for better visibility (if terminal supports it)
            if (cell == "P") {
                std::cout << "\033[32mP\033[0m "; // Green player
            } else if (cell == "B") {
                std::cout << "\033[33mB\033[0m "; // Yellow box
            } else if (cell >= "1" && cell <= "9") {
                std::cout << "\033[35m" << cell << "\033[0m "; // Magenta portal box
            } else if (cell == "=") {
                std::cout << "\033[31m=\033[0m "; // Red player destination
            } else if (cell == "_") {
                std::cout << "\033[34m_\033[0m "; // Blue box destination
            } else if (cell == "#") {
                std::cout << "# ";
            } else {
                std::cout << ". ";
            }
        }
        std::cout << std::endl;
    }
    
    std::cout << std::endl;
    
    // Show other rooms for reference
    for (int room = 0; room < scenes.size(); ++room) {
        if (room != next_state.player.room) {
            std::cout << "Room " << room << ":" << std::endl;
            Scene room_display = scenes[room];
            
            // Place boxes in this room
            for (const auto& [bid, box] : next_state.boxes) {
                if (box.room == room) {
                    std::string original_cell = scenes[box.room][box.y][box.x];
                    room_display[box.y][box.x] = "B";
                }
            }
            for (const auto& [rid, boxroom] : next_state.boxrooms) {
                if (boxroom.room == room) {
                    std::string original_cell = scenes[boxroom.room][boxroom.y][boxroom.x];
                    room_display[boxroom.y][boxroom.x] = std::to_string(rid);
                }
            }
            
            // Show a smaller version
            bool has_content = false;
            for (int y = 0; y < MAX_SIZE; ++y) {
                bool line_has_content = false;
                for (int x = 0; x < MAX_SIZE; ++x) {
                    if (room_display[y][x] != "#") {
                        line_has_content = true;
                        has_content = true;
                        break;
                    }
                }
                if (line_has_content) {
                    for (int x = 0; x < MAX_SIZE; ++x) {
                        std::cout << room_display[y][x] << " ";
                    }
                    std::cout << std::endl;
                }
            }
            if (!has_content) {
                std::cout << "(Empty room)" << std::endl;
            }
            std::cout << std::endl;
        }
    }
    
    std::cout << "Enter move (w/a/s/d) or q to quit: ";
}

Input Interface::processInput(char c)
{
    switch (c) {
        case 'w':
        case 'W':
            return UP;
        case 's':
        case 'S':
            return DOWN;
        case 'a':
        case 'A':
            return LEFT;
        case 'd':
        case 'D':
            return RIGHT;
        case 'q':
        case 'Q':
            std::cout << "Thanks for playing!" << std::endl;
            exit(0);
        default:
            std::cout << "Invalid input! Use w/a/s/d to move, q to quit." << std::endl;
            return UP; // Default fallback
    }
}
