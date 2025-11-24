#ifndef INTERFACE_HPP
#define INTERFACE_HPP

#include "level_loader.hpp"
#include "gameplay.hpp"

#include <vector>

class Interface
{
public:
    Interface(const Level level);
    void renderBegin();
    void render(GameState curr_state, GameState next_state); //param curr_state is for motion rendering in gui. just print next_state in cli.
    Input processInput(char c);
private:
    std::vector<Scene> scenes;
};

#endif