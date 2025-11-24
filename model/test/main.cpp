#include "gameplay.hpp"
#include "level_loader.hpp"
#include "interface.hpp"

#include <iostream>

int main(){
    Level level = LevelLoader::loadLevel("..\\levels\\l1.json");
    GamePlay game = GamePlay(level);
    Interface cli = Interface(level);
    char op;
    cli.renderBegin();
    while(true){
        std::cin >> op;
        game.operate(cli.processInput(op));
        cli.render(game.getCurrState(), game.getNextState());
        game.updateState();
    }
}