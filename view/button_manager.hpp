#ifndef BUTTON_MANAGER_HPP
#define BUTTON_MANAGER_HPP

#include "button.hpp"
#include <stb_easy_font.h>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

static const char* vertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

uniform mat4 projection;
uniform mat4 model;

void main() {
    vec4 pos = projection * model * vec4(aPos, 0.0, 1.0);
    gl_Position = pos;
}
)";

static const char* fragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 buttonColor;
void main() {
    FragColor = buttonColor;
}
)";

static const char* textVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;

uniform mat4 projection;

void main() {
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
}
)";

static const char* textFragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 textColor;
void main() {
    FragColor = textColor;
}
)";

// ButtonManager owns geometric buffers, shaders, and a collection of Buttons.
class ButtonManager {
private:
    std::vector<Button> buttons;   // Interactive buttons rendered in the UI overlay.
    GLuint buttonVAO, buttonVBO;   // Geometry for the base button quad.
    GLuint textVAO, textVBO;       // Geometry buffer for dynamic text meshes.
    GLuint buttonShader, textShader; // Shader programs for button quads and text glyphs.
    std::vector<char> glyphBuffer; // Temporary buffer filled by stb_easy_font_print.
    std::vector<float> textVertices; // CPU-side vertex data before uploading to OpenGL.

public:
    // Initialize all GL resources and create default shaders.
    void init() {
        setupButtonGeometry();
        setupTextRendering();
        compileShaders();
    }

    // Create the static quad shared by all buttons.
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

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    // Configure buffers used to draw the dynamic glyph mesh.
    void setupTextRendering() {
        glGenVertexArrays(1, &textVAO);
        glGenBuffers(1, &textVBO);

        glBindVertexArray(textVAO);
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    // Register a clickable button with position, size, label, and callback.
    void addButton(float x, float y, float w, float h, const std::string& text,
        std::function<void()> callback) {
        buttons.emplace_back(x, y, w, h, text, glm::vec4(0.2f, 0.6f, 1.0f, 1.0f), callback);
    }

    // Update hover state for all buttons.
    void updateButtons(float mouseX, float mouseY) {
        for (auto& button : buttons) {
            button.update(mouseX, mouseY);
        }
    }

    // Forward click events to buttons so they can trigger callbacks.
    void handleClick(float mouseX, float mouseY) {
        for (auto& button : buttons) {
            button.handleClick(mouseX, mouseY);
        }
    }

    GLuint compileShader(GLenum type, const char* src) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, NULL);
        glCompileShader(shader);
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, NULL, infoLog);
            std::cerr << "Shader compile error: " << infoLog << std::endl;
        }
        return shader;
    }

    // Build the shader programs for button quads and glyph text.
    void compileShaders() {
        GLuint vert = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
        GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
        buttonShader = glCreateProgram();
        glAttachShader(buttonShader, vert);
        glAttachShader(buttonShader, frag);
        glLinkProgram(buttonShader);
        GLint success;
        glGetProgramiv(buttonShader, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(buttonShader, 512, NULL, infoLog);
            std::cerr << "Program link error: " << infoLog << std::endl;
        }
        glDeleteShader(vert);
        glDeleteShader(frag);

        GLuint textVert = compileShader(GL_VERTEX_SHADER, textVertexShaderSrc);
        GLuint textFrag = compileShader(GL_FRAGMENT_SHADER, textFragmentShaderSrc);
        textShader = glCreateProgram();
        glAttachShader(textShader, textVert);
        glAttachShader(textShader, textFrag);
        glLinkProgram(textShader);
        glGetProgramiv(textShader, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(textShader, 512, NULL, infoLog);
            std::cerr << "Text program link error: " << infoLog << std::endl;
        }
        glDeleteShader(textVert);
        glDeleteShader(textFrag);
    }

    // Upload the orthographic projection used by both button and text shaders.
    void setProjection(const glm::mat4& proj) {
        glUseProgram(buttonShader);
        glUniformMatrix4fv(glGetUniformLocation(buttonShader, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUseProgram(0);

        glUseProgram(textShader);
        glUniformMatrix4fv(glGetUniformLocation(textShader, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUseProgram(0);
    }

    // Draw all buttons and their text labels.
    void renderButtons() {
        glUseProgram(buttonShader);
        glBindVertexArray(buttonVAO);

        for (auto& button : buttons) {
            button.render(buttonShader);
        }

        glBindVertexArray(0);
        glUseProgram(0);

        renderButtonLabels();
    }

private:
    // Draw text labels for each button using stb_easy_font glyph data.
    void renderButtonLabels() {
        glUseProgram(textShader);
        glBindVertexArray(textVAO);

        GLint colorLoc = glGetUniformLocation(textShader, "textColor");
        if (colorLoc != -1) {
            glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
        }

        for (const auto& button : buttons) {
            renderButtonLabel(button);
        }

        glBindVertexArray(0);
        glUseProgram(0);
    }

    // Render a single button label by tessellating quads returned by stb_easy_font.
    void renderButtonLabel(const Button& button) {
        const std::string& label = button.getText();
        if (label.empty()) {
            return;
        }

        constexpr size_t bytesPerChar = 272;
        size_t requiredBytes = (std::max<size_t>(label.size(), 1) * bytesPerChar) + bytesPerChar;
        if (glyphBuffer.size() < requiredBytes) {
            glyphBuffer.resize(requiredBytes);
        }

        char* textPtr = const_cast<char*>(label.c_str());
        int quadCount = stb_easy_font_print(0.0f, 0.0f, textPtr, nullptr, glyphBuffer.data(), static_cast<int>(glyphBuffer.size()));
        if (quadCount <= 0) {
            return;
        }

        float textWidth = static_cast<float>(stb_easy_font_width(textPtr));
        float textHeight = static_cast<float>(stb_easy_font_height(textPtr));
        glm::vec2 pos = button.getPosition();
        glm::vec2 size = button.getSize();
        float offsetX = pos.x + (size.x - textWidth) * 0.5f;
        float offsetY = pos.y + (size.y - textHeight) * 0.5f;

        textVertices.clear();
        textVertices.reserve(static_cast<size_t>(quadCount) * 6 * 2);

        struct GlyphVertex {
            float x;
            float y;
            float z;
            unsigned char color[4];
        };

        GlyphVertex* vertices = reinterpret_cast<GlyphVertex*>(glyphBuffer.data());
        auto pushVertex = [this](float x, float y) {
            textVertices.push_back(x);
            textVertices.push_back(y);
        };

        for (int i = 0; i < quadCount; ++i) {
            GlyphVertex* quad = vertices + i * 4;
            pushVertex(quad[0].x, quad[0].y);
            pushVertex(quad[1].x, quad[1].y);
            pushVertex(quad[2].x, quad[2].y);

            pushVertex(quad[0].x, quad[0].y);
            pushVertex(quad[2].x, quad[2].y);
            pushVertex(quad[3].x, quad[3].y);
        }

        for (size_t i = 0; i < textVertices.size(); i += 2) {
            textVertices[i] += offsetX;
            textVertices[i + 1] = offsetY + (textHeight - textVertices[i + 1]);
        }

        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferData(GL_ARRAY_BUFFER, textVertices.size() * sizeof(float), textVertices.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(textVertices.size() / 2));
    }
};

#endif // !BUTTON_MANAGER_HPP