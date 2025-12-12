#ifndef BUTTON_HPP
#define BUTTON_HPP

#include <iostream>
#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <functional>

// Button encapsulates geometry, state, and callback for a clickable UI element.
class Button {
private:
    float x, y, width, height;           // Axis-aligned bounds in screen coordinates.
    std::string text;                    // Label rendered on the button face.
    glm::vec4 color;                     // Base color when idle.
    glm::vec4 hoverColor;                // Color tint applied when hovered.
    bool isHovered;                      // Hover state updated each frame.
    std::function<void()> onClick;       // Callback executed on left-click release.

public:
    Button(float x, float y, float w, float h, const std::string& text, const glm::vec4& color, std::function<void()> callback)
        : x(x), y(y), width(w), height(h), text(text), color(color), onClick(callback), isHovered(false) {
        hoverColor = glm::vec4(color.r * 0.8f, color.g * 0.8f, color.b * 0.8f, color.a);
    }

    // Returns true if the given UI coordinates fall inside the button rect.
    bool contains(float mouseX, float mouseY) {
        return mouseX >= x && mouseX <= x + width && mouseY >= y && mouseY <= y + height;
    }

    // Refresh hover state based on the latest mouse coordinates.
    void update(float mouseX, float mouseY) {
        isHovered = contains(mouseX, mouseY);
    }

    // Invoke the click callback if the cursor is inside the rect.
    void handleClick(float mouseX, float mouseY) {
        if (contains(mouseX, mouseY)) {
            onClick();
        }
    }

    // Draw the button using the provided shader (expects model + color uniforms).
    void render(GLuint shader) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(x, y, 0.0f));
        model = glm::scale(model, glm::vec3(width, height, 1.0f));

        glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));

        glm::vec4 finalColor = isHovered ? hoverColor : color;
        glUniform4f(glGetUniformLocation(shader, "buttonColor"),
            finalColor.r, finalColor.g, finalColor.b, finalColor.a);

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    const std::string& getText() const {
        return text;
    }

    glm::vec2 getPosition() const {
        return glm::vec2(x, y);
    }

    glm::vec2 getSize() const {
        return glm::vec2(width, height);
    }
};

#endif // BUTTON_HPP