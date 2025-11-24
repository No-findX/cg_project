#include "../include/level_loader.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

Level LevelLoader::loadLevel(const std::string& level_path)
{
    std::ifstream file(level_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open level file: " + level_path);
    }

    json j;
    file >> j;
    file.close();

    Level loaded_level;
    loaded_level.id = j["l_id"];
    loaded_level.room_num = j["room_num"];
    loaded_level.rooms.resize(loaded_level.room_num);

    for (const auto& room_json : j["rooms"]) {
        int r_id = room_json["r_id"];
        int size = room_json["size"];
        loaded_level.rooms[r_id].size = size;
        
        if (r_id >= loaded_level.room_num) {
            throw std::runtime_error("Invalid room ID: " + std::to_string(r_id));
        }

        // Initialize scene with empty strings
        Scene room_scene;
        for (int i = 0; i < MAX_SIZE; ++i) {
            for (int j = 0; j < MAX_SIZE; ++j) {
                room_scene[i][j] = "#"; // Fill with walls by default
            }
        }

        // Load the actual layout
        const auto& layout = room_json["layout"];
        for (int i = 0; i < size && i < MAX_SIZE; ++i) {
            for (int j = 0; j < size && j < MAX_SIZE; ++j) {
                if (i < layout.size() && j < layout[i].size()) {
                    room_scene[i][j] = layout[i][j];
                }
            }
        }

        loaded_level.rooms[r_id].is_box = room_json["is_box"];
        for (const auto& entry :room_json["entries"])
        {
            std::array<int, 2> lentry = {entry[0], entry[1]};
            loaded_level.rooms[r_id].entries.push_back(lentry);
        }
        loaded_level.rooms[r_id].scene = room_scene;
    }

    return loaded_level;
}
