#ifndef UIMANAGER_HPP
#define UIMANAGER_HPP

#include "button_manager.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <functional>

// UIManager wires ButtonManager events to higher-level callbacks.
class UIManager {
private:
    ButtonManager buttonManager;      // Renders and stores UI buttons.
    float mouseX, mouseY;             // Last known mouse position in UI coordinates.
    bool mousePressed;                // Cached button state (unused but kept for future use).
    int windowWidth = 0;
    int windowHeight = 0;
    std::function<void()> startGameCallback; // Invoked when the Start button is clicked.

public:
    // Prepare UI controls, projection matrix, and button callbacks.
    void init(int windowWidth, int windowHeight) {
        this->windowWidth = windowWidth;
        this->windowHeight = windowHeight;
        buttonManager.init(windowWidth, windowHeight);
        setupUI();

        setupOrthographicProjection(windowWidth, windowHeight);
    }

    // Handle resize: update stored scr params of portals and update projection matrix
    void handleResize(int width, int height) {
        // 1. Change member var.s
        windowWidth = width;
        windowHeight = height;

        // 2. Update projection
        setupOrthographicProjection(width, height);

        // 3. Button manager resize
        buttonManager.updateWindowSize(width, height);
    }

    // Create the main menu buttons and wire high-level actions.
    void setupUI() {
        buttonManager.addButton(0.125, 0.166 * 3, 0.25, 0.083, "Start Game", [this]() {
            if (startGameCallback) {
                startGameCallback();
            } else {
                std::cout << "Start Game clicked!" << std::endl;
            }
            });

        buttonManager.addButton(0.125, 0.166 * 2, 0.25, 0.083, "Options", [this]() {
            std::cout << "Options clicked!" << std::endl;
            });

        buttonManager.addButton(0.125, 0.166 * 1, 0.25, 0.083, "Exit", [this]() {
            std::cout << "Exit clicked!" << std::endl;
            exit(0);
            });
    }

    // Allow the application to inject behavior for Start Game.
    void setStartGameCallback(std::function<void()> callback) {
        startGameCallback = std::move(callback);
    }

    // Translate window coordinates to UI space and update hover state.
    void handleMouseMove(float x, float y) {
        mouseX = x;
        mouseY = convertToUiY(y);
        buttonManager.updateButtons(mouseX, mouseY);
    }

    // Pass mouse clicks to the button manager when the left button is pressed.
    void handleMouseClick(int button, int action, float x, float y) {
        mouseX = x;
        mouseY = convertToUiY(y);

        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            buttonManager.handleClick(mouseX, mouseY);
        }
    }

    // Configure the projection so UI dimensions map 1:1 to screen pixels.
    void setupOrthographicProjection(int windowWidth, int windowHeight) {
        // glm.hpp in this project exposes glm::ortho in the global namespace via gtc/matrix_transform.hpp
        glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(windowWidth), 0.0f, static_cast<float>(windowHeight), -1.0f, 1.0f);
        buttonManager.setProjection(proj);
    }

    // Draw the full UI layer (buttons + blending state).
    void render() {
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        buttonManager.renderButtons();

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }

private:
    // Convert from top-left window coordinates to bottom-left UI coordinates.
    float convertToUiY(float cursorY) const {
        if (windowHeight <= 0) {
            return cursorY;
        }
        return static_cast<float>(windowHeight) - cursorY;
    }
};

#endif // UIMANAGER_HPP