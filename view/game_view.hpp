#ifndef GAME_VIEW_HPP
#define GAME_VIEW_HPP

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <vector>
#include <iostream>
#include <cmath>
#include <limits>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "model/include/gameplay.hpp"
#include "model/include/level_loader.hpp"
#include "view/uimanager.hpp"

namespace detail {

inline GLuint CompileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "Shader compile error: " << log << std::endl;
    }
    return shader;
}

static const char* kGameVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform mat4 view;
uniform mat4 projection;

out vec3 vColor;

void main() {
    vColor = aColor;
    gl_Position = projection * view * vec4(aPos, 1.0);
}
)";

static const char* kGameFragmentShader = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

class GameRenderer {
public:
    void init(int windowWidth, int windowHeight) {
        windowWidth_ = windowWidth;
        windowHeight_ = windowHeight;
        GLuint vert = CompileShader(GL_VERTEX_SHADER, kGameVertexShader);
        GLuint frag = CompileShader(GL_FRAGMENT_SHADER, kGameFragmentShader);
        shader_ = glCreateProgram();
        glAttachShader(shader_, vert);
        glAttachShader(shader_, frag);
        glLinkProgram(shader_);
        GLint success = 0;
        glGetProgramiv(shader_, GL_LINK_STATUS, &success);
        if (!success) {
            char log[512];
            glGetProgramInfoLog(shader_, 512, nullptr, log);
            std::cerr << "Program link error: " << log << std::endl;
        }
        glDeleteShader(vert);
        glDeleteShader(frag);

        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        updateProjection();
    }

    /// Update yaw/pitch in response to mouse deltas while clamping to avoid flips.
    void rotateCamera(float deltaX, float deltaY) {
        const float sensitivity = 0.15f;
        cameraYaw_ += deltaX * sensitivity;
        cameraPitch_ -= deltaY * sensitivity;
        if (cameraYaw_ > 360.0f) {
            cameraYaw_ -= 360.0f;
        } else if (cameraYaw_ < -360.0f) {
            cameraYaw_ += 360.0f;
        }
        cameraPitch_ = std::clamp(cameraPitch_, -80.0f, 80.0f);
    }

    /// Translate player input (WASD) to world-aligned directions based on camera yaw.
    Input remapInputForCamera(Input input) const {
        return mapCameraRelativeInput(input);
    }

    /// Gather geometry for visible rooms and submit draw calls using current camera.
    void render(const GameState& state, const Level& level) {
        if (shader_ == 0) {
            return;
        }

        if (state.player.room < 0 || state.player.room >= static_cast<int>(level.rooms.size())) {
            return;
        }

        const Room& room = level.rooms[state.player.room];
        if (room.size <= 0) {
            return;
        }

        vertexData_.clear();
        currentRoomHalfExtent_ = appendRoomGeometry(room, state.player.room, state, tileWorldSize_);
        glm::vec3 cameraPos = playerEyePosition_ + glm::vec3(0.0f, eyeHeightOffset_, 0.0f);

        glm::vec3 front;
        float yawRad = glm::radians(cameraYaw_);
        float pitchRad = glm::radians(cameraPitch_);
        front.x = std::cos(pitchRad) * std::cos(yawRad);
        front.y = std::sin(pitchRad);
        front.z = std::cos(pitchRad) * std::sin(yawRad);
        front = glm::normalize(front);
        cameraForward2D_ = glm::vec2(front.x, front.z);
        if (glm::dot(cameraForward2D_, cameraForward2D_) > 1e-5f) {
            cameraForward2D_ = glm::normalize(cameraForward2D_);
        } else {
            cameraForward2D_ = glm::vec2(0.0f, -1.0f);
        }

        view_ = glm::lookAt(cameraPos, cameraPos + front, glm::vec3(0.0f, 1.0f, 0.0f));

        if (vertexData_.empty()) {
            return;
        }

        glUseProgram(shader_);
        glUniformMatrix4fv(glGetUniformLocation(shader_, "view"), 1, GL_FALSE, glm::value_ptr(view_));
        glUniformMatrix4fv(glGetUniformLocation(shader_, "projection"), 1, GL_FALSE, glm::value_ptr(projection_));

        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, vertexData_.size() * sizeof(float), vertexData_.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexData_.size() / 6));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);
    }

    /// Release GL resources so repeated init/shutdown are safe.
    void shutdown() {
        if (vbo_) {
            glDeleteBuffers(1, &vbo_);
            vbo_ = 0;
        }
        if (vao_) {
            glDeleteVertexArrays(1, &vao_);
            vao_ = 0;
        }
        if (shader_) {
            glDeleteProgram(shader_);
            shader_ = 0;
        }
    }

private:
    /// Rebuild projection when the window changes (perspective for pseudo-3D feel).
    void updateProjection() {
        const float aspect = windowHeight_ == 0 ? 1.0f : static_cast<float>(windowWidth_) / static_cast<float>(windowHeight_);
        projection_ = glm::perspective(glm::radians(55.0f), aspect, 0.1f, 200.0f);
    }

    /// Convert a top-down room definition into extruded tiles/columns. Returns half extent.
    float appendRoomGeometry(const Room& room, int roomId, const GameState& state, float tileSize) {
        const int tileCount = room.size;
        const float boardHalf = tileCount * tileSize * 0.5f;
        playerEyePosition_ = glm::vec3(0.0f, 0.05f, 0.0f);

        auto pushQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec3& color) {
            pushVertex(v0, color);
            pushVertex(v1, color);
            pushVertex(v2, color);
            pushVertex(v0, color);
            pushVertex(v2, color);
            pushVertex(v3, color);
        };

        auto appendFloor = [&](float minX, float maxX, float minZ, float maxZ, const glm::vec3& color) {
            glm::vec3 v0(minX, 0.0f, minZ);
            glm::vec3 v1(maxX, 0.0f, minZ);
            glm::vec3 v2(maxX, 0.0f, maxZ);
            glm::vec3 v3(minX, 0.0f, maxZ);
            pushQuad(v0, v1, v2, v3, color);
        };

        auto appendColumn = [&](float minX, float maxX, float minZ, float maxZ, float minY, float maxY, const glm::vec3& color) {
            glm::vec3 topColor = color;
            glm::vec3 sideColor = color * 0.85f;
            glm::vec3 top0(minX, maxY, minZ);
            glm::vec3 top1(maxX, maxY, minZ);
            glm::vec3 top2(maxX, maxY, maxZ);
            glm::vec3 top3(minX, maxY, maxZ);
            pushQuad(top0, top1, top2, top3, topColor);

            glm::vec3 bottom0(minX, minY, minZ);
            glm::vec3 bottom1(maxX, minY, minZ);
            glm::vec3 bottom2(maxX, minY, maxZ);
            glm::vec3 bottom3(minX, minY, maxZ);
            pushQuad(bottom0, bottom1, top1, top0, sideColor);
            pushQuad(bottom1, bottom2, top2, top1, sideColor);
            pushQuad(bottom2, bottom3, top3, top2, sideColor);
            pushQuad(bottom3, bottom0, top0, top3, sideColor);
        };

        auto boundsForCell = [&](int gridX, int gridY) {
            float minX = -boardHalf + gridX * tileSize;
            float maxX = minX + tileSize;
            float maxZ = boardHalf - gridY * tileSize;
            float minZ = maxZ - tileSize;
            return std::array<float, 4>{minX, maxX, minZ, maxZ};
        };

        for (int y = 0; y < tileCount; ++y) {
            for (int x = 0; x < tileCount; ++x) {
                auto bounds = boundsForCell(x, y);
                glm::vec3 baseColor = tileColorForCell(room.scene[y][x]);
                appendFloor(bounds[0], bounds[1], bounds[2], bounds[3], baseColor);

                if (room.scene[y][x] == "#") {
                    appendColumn(bounds[0], bounds[1], bounds[2], bounds[3], 0.0f, wallHeight_, glm::vec3(0.3f, 0.3f, 0.35f));
                }
            }
        }

        auto drawOccupant = [&](const Pos& pos, const glm::vec3& color, float height, bool isPlayer) {
            if (pos.room != roomId) {
                return;
            }
            auto bounds = boundsForCell(pos.x, pos.y);
            const float inset = tileSize * 0.2f;
            if (isPlayer) {
                const float centerX = 0.5f * (bounds[0] + bounds[1]);
                const float centerZ = 0.5f * (bounds[2] + bounds[3]);
                playerEyePosition_ = glm::vec3(centerX, 0.05f, centerZ);
                if (hidePlayerMesh_) {
                    return;
                }
            }

            appendColumn(bounds[0] + inset, bounds[1] - inset, bounds[2] + inset, bounds[3] - inset,
                         0.02f, 0.02f + height, color);
        };

        for (const auto& kv : state.boxes) {
            drawOccupant(kv.second, glm::vec3(0.85f, 0.55f, 0.2f), 0.35f, false);
        }

        drawOccupant(state.player, glm::vec3(0.25f, 0.85f, 0.35f), 0.45f, true);
        return boardHalf;
    }

    /// Map textual tiles to base colors for the 3D renderer.
    glm::vec3 tileColorForCell(const std::string& cell) const {
        if (cell == "#") return {0.2f, 0.2f, 0.2f};
        if (cell == "=") return {0.25f, 0.6f, 0.3f};
        if (cell == "_") return {0.7f, 0.6f, 0.25f};
        if (!cell.empty() && std::isdigit(static_cast<unsigned char>(cell[0]))) return {0.4f, 0.35f, 0.7f};
        return {0.15f, 0.15f, 0.15f};
    }

    /// Helper to push a colored vertex into the dynamic buffer.
    void pushVertex(const glm::vec3& pos, const glm::vec3& color) {
        vertexData_.push_back(pos.x);
        vertexData_.push_back(pos.y);
        vertexData_.push_back(pos.z);
        vertexData_.push_back(color.r);
        vertexData_.push_back(color.g);
        vertexData_.push_back(color.b);
    }

    /// Determine which grid direction best matches the requested input given camera yaw.
    Input mapCameraRelativeInput(Input input) const {
        glm::vec2 forward = cameraForward2D_;
        if (glm::dot(forward, forward) < 1e-5f) {
            forward = glm::vec2(0.0f, -1.0f);
        }

        auto chooseCardinal = [&](const glm::vec2& dir) {
            glm::vec2 norm = glm::dot(dir, dir) < 1e-6f ? glm::vec2(0.0f, -1.0f) : glm::normalize(dir);
            struct Candidate {
                glm::vec2 vec;
                Input input;
            };
            static const Candidate candidates[] = {
                {{ 1.0f,  0.0f}, RIGHT},
                {{-1.0f,  0.0f}, LEFT},
                {{ 0.0f,  1.0f}, UP},
                {{ 0.0f, -1.0f}, DOWN}
            };
            float bestDot = -std::numeric_limits<float>::infinity();
            Input best = UP;
            for (const auto& c : candidates) {
                float dot = glm::dot(norm, c.vec);
                if (dot > bestDot) {
                    bestDot = dot;
                    best = c.input;
                }
            }
            return best;
        };

        auto right = glm::vec2(-forward.y, forward.x);

        switch (input) {
        case UP:
            return chooseCardinal(forward);
        case DOWN:
            return chooseCardinal(-forward);
        case RIGHT:
            return chooseCardinal(right);
        case LEFT:
        default:
            return chooseCardinal(-right);
        }
    }

    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint shader_ = 0;
    glm::mat4 projection_ = glm::mat4(1.0f);
    glm::mat4 view_ = glm::mat4(1.0f);
    int windowWidth_ = 0;
    int windowHeight_ = 0;
    std::vector<float> vertexData_;

    float cameraYaw_ = -90.0f;
    float cameraPitch_ = 0.0f;
    float currentRoomHalfExtent_ = 5.0f;
    glm::vec3 playerEyePosition_{0.0f, 0.05f, 0.0f};
    glm::vec2 cameraForward2D_{0.0f, -1.0f};
    const float tileWorldSize_ = 1.0f;
    const float wallHeight_ = 0.5f;
    const float eyeHeightOffset_ = 0.55f;
    const bool hidePlayerMesh_ = true;
};

} // namespace detail

/// High level view facade handling UI/menu overlay plus in-game rendering.
class GameView {
public:
    /// Prepare UI + renderer subsystems once.
    bool init(int width, int height) {
        if (initialized_) {
            return true;
        }
        uiManager_.init(width, height);
        renderer_.init(width, height);
        initialized_ = true;
        return true;
    }

    /// Provide callback triggered when UI start button is pressed.
    void setStartCallback(std::function<void()> callback) {
        startCallback_ = std::move(callback);
        uiManager_.setStartGameCallback(startCallback_);
    }

    /// Toggle between menu overlay and gameplay rendering.
    void setGameSceneVisible(bool visible) {
        showGameScene_ = visible;
    }

    /// Release renderer resources (UI has trivial lifetime).
    void shutdown() {
        if (!initialized_) {
            return;
        }
        renderer_.shutdown();
        initialized_ = false;
    }

    /// Render either the 3D view or fallback UI menu when no game is displayed.
    void render(const GameState* state, const Level* level) {
        bool renderedScene = false;
        if (showGameScene_ && state && level) {
            renderer_.render(*state, *level);
            renderedScene = true;
        }
        if (!renderedScene) {
            uiManager_.render();
        }
    }

    /// Remap inputs only when the 3D renderer is active.
    Input remapInputForCamera(Input input) const {
        if (!showGameScene_) {
            return input;
        }
        return renderer_.remapInputForCamera(input);
    }

    /// Route mouse deltas either to camera orbit or UI hover logic.
    void handleMouseMove(float x, float y) {
        if (showGameScene_) {
            if (rotatingCamera_) {
                renderer_.rotateCamera(x - lastMouseX_, y - lastMouseY_);
            }
            lastMouseX_ = x;
            lastMouseY_ = y;
        } else {
            uiManager_.handleMouseMove(x, y);
        }
    }

    /// Start/stop camera rotation on right-click while preserving UI click handling.
    void handleMouseButton(int button, int action, float x, float y) {
        if (showGameScene_) {
            if (button == GLFW_MOUSE_BUTTON_RIGHT) {
                if (action == GLFW_PRESS) {
                    rotatingCamera_ = true;
                    lastMouseX_ = x;
                    lastMouseY_ = y;
                } else if (action == GLFW_RELEASE) {
                    rotatingCamera_ = false;
                }
            }
        } else {
            uiManager_.handleMouseClick(button, action, x, y);
        }
    }

private:
    UIManager uiManager_;
    detail::GameRenderer renderer_;
    bool initialized_ = false;
    bool showGameScene_ = false;
    std::function<void()> startCallback_;
    bool rotatingCamera_ = false;
    float lastMouseX_ = 0.0f;
    float lastMouseY_ = 0.0f;
};

#endif // GAME_VIEW_HPP
