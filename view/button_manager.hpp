#ifndef BUTTON_MANAGER_HPP
#define BUTTON_MANAGER_HPP

#include "button.hpp"
#include <vector>

class ButtonManager {
private:
    std::vector<Button> buttons;
    GLuint buttonVAO, buttonVBO;
    GLuint textVAO, textVBO;
    GLuint buttonShader, textShader;

public:
    void init() {
        setupButtonGeometry();
        compileShaders();
    }

    void setupButtonGeometry() {
        float vertices[] = {
            0.0f, 1.0f, 0.0f, 1.0f,
            1.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,

            0.0f, 1.0f, 0.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f,
            1.0f, 0.0f, 1.0f, 0.0f
        };

        glGenVertexArrays(1, &buttonVAO);
        glGenBuffers(1, &buttonVBO);

        glBindVertexArray(buttonVAO);
        glBindBuffer(GL_ARRAY_BUFFER, buttonVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    }

    void addButton(float x, float y, float w, float h, const std::string& text,
        std::function<void()> callback) {
        buttons.emplace_back(x, y, w, h, text, glm::vec4(0.2f, 0.6f, 1.0f, 1.0f), callback);
    }

    void updateButtons(float mouseX, float mouseY) {
        for (auto& button : buttons) {
            button.update(mouseX, mouseY);
        }
    }

    void handleClick(float mouseX, float mouseY) {
        for (auto& button : buttons) {
            button.handleClick(mouseX, mouseY);
        }
    }

    void renderButtons() {
        glUseProgram(buttonShader);
        glBindVertexArray(buttonVAO);

        for (auto& button : buttons) {
            button.render(buttonShader);
        }
    }
};

#endif // !BUTTON_MANAGER_HPP