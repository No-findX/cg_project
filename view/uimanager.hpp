#ifndef UIMANAGER_HPP
#define UIMANAGER_HPP

#include "button_manager.hpp"

class UIManager {
private:
    ButtonManager buttonManager;
    float mouseX, mouseY;
    bool mousePressed;

public:
    void init(int windowWidth, int windowHeight) {
        buttonManager.init();
        setupUI();

        setupOrthographicProjection(windowWidth, windowHeight);
    }

    void setupUI() {
        buttonManager.addButton(100, 100, 200, 50, "Start Game", [this]() {
            std::cout << "Start Game clicked!" << std::endl;
            });

        buttonManager.addButton(100, 200, 200, 50, "Options", [this]() {
            std::cout << "Options clicked!" << std::endl;
            });

        buttonManager.addButton(100, 300, 200, 50, "Exit", [this]() {
            std::cout << "Exit clicked!" << std::endl;
            exit(0);
            });
    }

    void handleMouseMove(float x, float y) {
        mouseX = x;
        mouseY = y;
        buttonManager.updateButtons(mouseX, mouseY);
    }

    void handleMouseClick(int button, int action, float x, float y) {
        mouseX = x;
        mouseY = y;

        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            buttonManager.handleClick(mouseX, mouseY);
        }
    }

    void render() {
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        buttonManager.renderButtons();

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }
};

#endif // UIMANAGER_HPP