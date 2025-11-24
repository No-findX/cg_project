#ifndef BUTTON_HPP
#define BUTTON_HPP

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "include/stb_image.h"
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include <functional>

class Button {
private:
    float x, y, width, height;
    std::string text;
    glm::vec4 color;
    glm::vec4 hoverColor;
    bool isHovered;
    std::function<void()> onClick;

public:
    Button(float x, float y, float w, float h, const std::string& text,
        const glm::vec4& color, std::function<void()> callback)
        : x(x), y(y), width(w), height(h), text(text), color(color),
        onClick(callback), isHovered(false) {
        hoverColor = glm::vec4(color.r * 0.8f, color.g * 0.8f, color.b * 0.8f, color.a);
    }

    bool contains(float mouseX, float mouseY) {
        return mouseX >= x && mouseX <= x + width &&
            mouseY >= y && mouseY <= y + height;
    }

    void update(float mouseX, float mouseY) {
        isHovered = contains(mouseX, mouseY);
    }

    void handleClick(float mouseX, float mouseY) {
        if (contains(mouseX, mouseY) {
            onClick();
        }
    }

    void render() {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(x, y, 0.0f));
        model = glm::scale(model, glm::vec3(width, height, 1.0f));

        glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));

        glm::vec4 finalColor = isHovered ? hoverColor : color;
        glUniform4f(glGetUniformLocation(shader, "buttonColor"),
            finalColor.r, finalColor.g, finalColor.b, finalColor.a);

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
};

#endif // BUTTON_HPP