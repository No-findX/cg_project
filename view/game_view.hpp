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
#include <string>

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
        textureWidth_ = windowWidth_;
        textureHeight_ = windowHeight_;

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

        glGenFramebuffers(1, &fbo_);

        updateProjection();
    }

    // no-op: remove mouse-drag free rotation for 2.5D style
    void rotateCamera(float /*deltaX*/, float /*deltaY*/) {
    }

    // 旋转时拒绝新的旋转请求，避免角度漂移
    void rotateCameraBy90(bool left, float rotationtime = 0.0) {
        const float step = left ? -90.0f : 90.0f;

        // 若正在旋转，则忽略新的旋转输入（输入锁定）
        if (rotating_) {
            return;
        }

        if (rotationtime == 0.0f) {
            if (cameraYaw_ > 360.0f) cameraYaw_ -= 360.0f;
            else if (cameraYaw_ < -360.0f) cameraYaw_ += 360.0f;
            cameraYaw_ += step;
            cameraPitch_ = fixedPitch_;
            rotating_ = false;
        } else {
            if (rotateTargetYaw_ > 360.0f) rotateTargetYaw_ -= 360.0f;
            else if (rotateTargetYaw_ < -360.0f) rotateTargetYaw_ += 360.0f;
            rotateStartYaw_ = cameraYaw_;
            rotateTargetYaw_ = cameraYaw_ + step;

            rotateDuration_ = rotationtime;
            rotateStartTime_ = static_cast<float>(glfwGetTime());
            rotating_ = true;
            cameraPitch_ = fixedPitch_;
        }
    }

    // 开始一次位移动画（用于玩家/箱子推进）
    void beginMoveAnimation(float duration) {
        // 缩短时长以提升轻快感，若调用者提供更长时长，可按调用者值
        const float preferred = 0.3f;
        moveDuration_ = std::min(duration > 0.0f ? duration : preferred, preferred);
        moveStartTime_ = static_cast<float>(glfwGetTime());
        moving_ = true;
    }
        
    Input remapInputForCamera(Input input) const {
        return mapCameraRelativeInput(input);
    }

    // 查询当前是否处于相机旋转中（供输入层加锁）
    bool isRotating() const {
        return rotating_;
    }

    void render(const GameState& state, const Level& level, const GameState& next_state) {
        if (shader_ == 0) return;
        if (level.rooms.empty()) return;

        // 相机旋转动画（平滑）
        if (rotating_) {
            const float now = static_cast<float>(glfwGetTime());
            float elapsed = now - rotateStartTime_;
            if (elapsed >= rotateDuration_) {
                cameraYaw_ = rotateTargetYaw_;
                rotating_ = false;
            } else {
                float u = elapsed / rotateDuration_;
                if (u < 0.0f) u = 0.0f;
                if (u > 1.0f) u = 1.0f;
                float p = u * u * (3.0f - 2.0f * u);
                float delta = (rotateTargetYaw_ >= rotateStartYaw_) ? (90.0f) : (-90.0f);
                cameraYaw_ = rotateStartYaw_ + delta * p;
            }
        }

        // 位移动画时间参数（匀加速-匀速-匀减速）
        float moveT = 1.0f;
        if (moving_) {
            const float now = static_cast<float>(glfwGetTime());
            float elapsed = now - moveStartTime_;
            if (elapsed >= moveDuration_) {
                moving_ = false;
                moveT = 1.0f;
            } else {
                float u = elapsed / moveDuration_;
                if (u < 0.0f) u = 0.0f;
                if (u > 1.0f) u = 1.0f;

                // 速度梯形：前 20% 加速，后 20% 减速，中间匀速
                const float acc = 0.2f;
                const float dec = 0.2f;
                const float constv = 1.0f - acc - dec; // 0.6
                // 归一化最大速度，使总位移为 1
                // 加速段距离 s1 = 0.5*a*vmax, 减速段同 s3 = 0.5*d*vmax, 匀速段 s2 = constv*vmax
                // s1+s2+s3 = (0.5*(acc+dec)+constv)*vmax = 1 => vmax = 1 / (constv + 0.5f*(acc+dec))
                const float vmax = 1.0f / (constv + 0.5f * (acc + dec));

                if (u <= acc) {
                    // s = 0.5*a*(u/acc)^2 * vmax*acc 直接用匀加速：s = 0.5 * aAcc * t^2
                    // 以归一化时间 u，在加速段位移为 0.5 * vmax/acc * u^2
                    moveT = 0.5f * (vmax / acc) * u * u;
                } else if (u <= acc + constv) {
                    // 到达加速段末位移
                    const float s_acc = 0.5f * vmax * acc;
                    // 匀速段：s = vmax * (u - acc)
                    moveT = s_acc + vmax * (u - acc);
                } else {
                    // 进入减速段
                    const float s_acc = 0.5f * vmax * acc;
                    const float s_const = vmax * constv;
                    float ud = u - (acc + constv); // 减速段经历的归一化时间
                    // 减速段位移：s = vmax*ud - 0.5*(vmax/dec)*ud^2
                    moveT = s_acc + s_const + vmax * ud - 0.5f * (vmax / dec) * ud * ud;
                }

                // 保护
                if (moveT < 0.0f) moveT = 0.0f;
                if (moveT > 1.0f) moveT = 1.0f;
            }
        }

        ensureRoomTextures(level.rooms.size());

        for (size_t i = 0; i < level.rooms.size(); ++i) {
            glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, roomTextures_[i], 0);
            GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
            glDrawBuffers(1, drawBuffers);

            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                std::cerr << "Framebuffer not complete for room " << i << std::endl;
            }

            glViewport(0, 0, textureWidth_, textureHeight_);
            glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            renderRoomIndex(static_cast<int>(i), state, level, next_state, moveT);

            glBindTexture(GL_TEXTURE_2D, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        if (state.player.room >= 0 && state.player.room < static_cast<int>(level.rooms.size())) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, windowWidth_, windowHeight_);
            glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            renderRoomIndex(state.player.room, state, level, next_state, moveT);
        }
    }

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
        if (fbo_) {
            glDeleteFramebuffers(1, &fbo_);
            fbo_ = 0;
        }
        if (!roomTextures_.empty()) {
            glDeleteTextures(static_cast<GLsizei>(roomTextures_.size()), roomTextures_.data());
            roomTextures_.clear();
        }
    }

private:
    void updateProjection() {
        const float aspect = windowHeight_ == 0 ? 1.0f : static_cast<float>(windowWidth_) / static_cast<float>(windowHeight_);
        projection_ = glm::perspective(glm::radians(55.0f), aspect, 0.1f, 200.0f);
    }

    void ensureRoomTextures(size_t count) {
        if (textureWidth_ != windowWidth_ || textureHeight_ != windowHeight_) {
            textureWidth_ = windowWidth_;
            textureHeight_ = windowHeight_;
            if (!roomTextures_.empty()) {
                glDeleteTextures(static_cast<GLsizei>(roomTextures_.size()), roomTextures_.data());
                roomTextures_.clear();
            }
        }

        while (roomTextures_.size() < count) {
            GLuint tex = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth_, textureHeight_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);
            roomTextures_.push_back(tex);
        }
    }

    // 新增：支持插值渲染，依据 state 与 next_state 以及 moveT
    void renderRoomIndex(int roomId, const GameState& state, const Level& level, const GameState& next_state, float moveT) {
        if (roomId < 0 || roomId >= static_cast<int>(level.rooms.size())) return;
        const Room& room = level.rooms[roomId];
        if (room.size <= 0) return;

        vertexData_.clear();
        currentRoomHalfExtent_ = appendRoomGeometry(room, roomId, state, next_state, moveT, tileWorldSize_);

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
            cameraForward2D_ = glm::vec2(1.0f, 0.0f);
        }

        glm::vec3 roomCenter = glm::vec3(0.0f, 0.02f, 0.0f);
        glm::vec3 forwardXZ = glm::vec3(front.x, 0.0f, front.z);
        glm::vec3 offsetDir = glm::length(forwardXZ) < 1e-5f ? glm::vec3(-1.0f, 0.0f, 0.0f) : -glm::normalize(forwardXZ);

        glm::vec3 cameraPos = roomCenter + offsetDir * currentRoomHalfExtent_ + glm::vec3(0.0f, 3.0f, 0.0f);
        view_ = glm::lookAt(cameraPos, roomCenter, glm::vec3(0.0f, 1.0f, 0.0f));

        if (vertexData_.empty()) return;

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

    float appendRoomGeometry(const Room& room, int roomId, const GameState& state, const GameState& next_state, float moveT, float tileSize) {
        const int tileCount = room.size;
        const float boardHalf = tileCount * tileSize * 0.5f;

        playerEyePosition_ = glm::vec3(0.0f, 0.02f, 0.0f);

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

        auto centerForCell = [&](int gridX, int gridY) -> glm::vec2 {
            auto b = boundsForCell(gridX, gridY);
            return glm::vec2((b[0] + b[1]) * 0.5f, (b[2] + b[3]) * 0.5f); // x, z
        };

        auto drawAtCenter = [&](const glm::vec2& centerXZ, const glm::vec3& color, float height) {
            const float inset = tileSize * 0.02f;
            const float halfW = (tileSize * 0.5f) - inset;
            float minX = centerXZ.x - halfW;
            float maxX = centerXZ.x + halfW;
            float minZ = centerXZ.y - halfW;
            float maxZ = centerXZ.y + halfW;
            appendColumn(minX, maxX, minZ, maxZ, 0.02f, 0.02f + height, color);
        };

        // 地面/墙体
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

        // 计算玩家插值
        auto drawPlayer = [&]() {
            const Pos& s = state.player;
            const Pos& e = next_state.player;
            glm::vec3 color(0.25f, 0.85f, 0.35f);
            float height = 0.96f;

            if (s.room == roomId && e.room == roomId && (s.x != e.x || s.y != e.y) && moving_) {
                glm::vec2 cs = centerForCell(s.x, s.y);
                glm::vec2 ce = centerForCell(e.x, e.y);
                glm::vec2 c = cs + (ce - cs) * moveT;
                drawAtCenter(c, color, height);
            } else if (s.room == roomId && (!moving_ || (s.room != e.room || (s.x == e.x && s.y == e.y)))) {
                // 静止或跨房间：使用 s
                glm::vec2 c = centerForCell(s.x, s.y);
                drawAtCenter(c, color, height);
            } else if (!moving_ && e.room == roomId) {
                // 动画结束后落位到 e
                glm::vec2 c = centerForCell(e.x, e.y);
                drawAtCenter(c, color, height);
            }
        };

        // 计算箱子插值
        auto drawBoxes = [&](std::map<int, Pos> boxes, std::map<int, Pos> next_boxes, glm::vec3 color) {
            for (const auto& kv : boxes) {
                int id = kv.first;
                const Pos& s = kv.second;
                float height = 0.96f;

                auto it = next_boxes.find(id);
                if (it != next_boxes.end()) {
                    const Pos& e = it->second;

                    if (s.room == roomId && e.room == roomId && (s.x != e.x || s.y != e.y) && moving_) {
                        glm::vec2 cs = centerForCell(s.x, s.y);
                        glm::vec2 ce = centerForCell(e.x, e.y);
                        glm::vec2 c = cs + (ce - cs) * moveT;
                        drawAtCenter(c, color, height);
                        continue;
                    }

                    if (s.room == roomId && (!moving_ || (s.room != e.room || (s.x == e.x && s.y == e.y)))) {
                        glm::vec2 c = centerForCell(s.x, s.y);
                        drawAtCenter(c, color, height);
                        continue;
                    }

                    if (!moving_ && e.room == roomId) {
                        glm::vec2 c = centerForCell(e.x, e.y);
                        drawAtCenter(c, color, height);
                        continue;
                    }
                } else {
                    // 目标状态中不存在该箱子（例如被传送/消失），保持当前显示（非动画）
                    if (s.room == roomId) {
                        glm::vec2 c = centerForCell(s.x, s.y);
                        drawAtCenter(c, color, height);
                    }
                }
            }
        };

        drawBoxes(state.boxes, next_state.boxes, glm::vec3(0.85f, 0.55f, 0.2f));
        drawBoxes(state.boxrooms, next_state.boxrooms, glm::vec3(0.4f, 0.35f, 0.7f));
        // 玩家最后绘制，避免被箱子遮挡
        drawPlayer();

        return boardHalf;
    }

    glm::vec3 tileColorForCell(const std::string& cell) const {
        if (cell == "#") return {0.2f, 0.2f, 0.2f};
        if (cell == "=") return {0.25f, 0.6f, 0.3f};
        if (cell == "_") return {0.7f, 0.6f, 0.25f};
        return {0.15f, 0.15f, 0.15f};
    }

    void pushVertex(const glm::vec3& pos, const glm::vec3& color) {
        vertexData_.push_back(pos.x);
        vertexData_.push_back(pos.y);
        vertexData_.push_back(pos.z);
        vertexData_.push_back(color.r);
        vertexData_.push_back(color.g);
        vertexData_.push_back(color.b);
    }

    Input mapCameraRelativeInput(Input input) const {
        glm::vec2 forward;
        {
            float yawRad = glm::radians(cameraYaw_);
            forward = glm::vec2(std::cos(yawRad), std::sin(yawRad));
        }
        if (glm::dot(forward, forward) < 1e-5f) {
            forward = glm::vec2(1.0f, 0.0f);
        }

        auto chooseCardinal = [&](const glm::vec2& dir) {
            glm::vec2 norm = glm::dot(dir, dir) < 1e-6f ? glm::vec2(1.0f, 0.0f) : glm::normalize(dir);
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

    // GL 资源
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint shader_ = 0;
    GLuint fbo_ = 0;
    std::vector<GLuint> roomTextures_;
    int textureWidth_ = 0;
    int textureHeight_ = 0;

    // 矩阵与窗口
    glm::mat4 projection_ = glm::mat4(1.0f);
    glm::mat4 view_ = glm::mat4(1.0f);
    int windowWidth_ = 0;
    int windowHeight_ = 0;
    std::vector<float> vertexData_;

    // 2.5D 离散相机
    float cameraYaw_ = 0.0f;                // 0 -> +X
    float cameraPitch_ = -45.0f;            // 固定俯仰
    const float fixedPitch_ = -45.0f;
    float currentRoomHalfExtent_ = 5.0f;
    glm::vec3 playerEyePosition_{0.0f, 0.02f, 0.0f};
    glm::vec2 cameraForward2D_{1.0f, 0.0f};
    const float tileWorldSize_ = 1.0f;
    const float wallHeight_ = 1.0f;
    const float eyeHeightOffset_ = 3.0f;
    const bool hidePlayerMesh_ = false;

    // 旋转动画状态
    bool rotating_ = false;
    float rotateStartYaw_ = 0.0f;
    float rotateTargetYaw_ = 0.0f;
    float rotateDuration_ = 0.0f;
    float rotateStartTime_ = 0.0f;

    // 位移动画状态（玩家/箱子）
    bool moving_ = false;
    float moveDuration_ = 0.0f;
    float moveStartTime_ = 0.0f;
};

} // namespace detail

class GameView {
public:
    bool init(int width, int height) {
        if (initialized_) return true;
        uiManager_.init(width, height);
        renderer_.init(width, height);
        initialized_ = true;
        return true;
    }

    void setStartCallback(std::function<void()> callback) {
        startCallback_ = std::move(callback);
        uiManager_.setStartGameCallback(startCallback_);
    }

    void setGameSceneVisible(bool visible) {
        showGameScene_ = visible;
    }

    void shutdown() {
        if (!initialized_) return;
        renderer_.shutdown();
        initialized_ = false;
    }

    void render(const GameState* state, const Level* level, const GameState* next_state = nullptr) {
        bool renderedScene = false;
        if (showGameScene_ && state && level) {
            // 容错：若未提供 next_state，则使用 state 自身
            const GameState& nextRef = next_state ? *next_state : *state;
            renderer_.render(*state, *level, nextRef);
            renderedScene = true;
        }
        if (!renderedScene) {
            uiManager_.render();
        }
    }

    Input remapInputForCamera(Input input) const {
        if (!showGameScene_) return input;
        return renderer_.remapInputForCamera(input);
    }

    void handleMouseMove(float x, float y) {
        if (showGameScene_) {
            lastMouseX_ = x;
            lastMouseY_ = y;
        } else {
            uiManager_.handleMouseMove(x, y);
        }
    }

    void handleMouseButton(int button, int action, float x, float y) {
        if (showGameScene_) {
            (void)button; (void)action; (void)x; (void)y;
        } else {
            uiManager_.handleMouseClick(button, action, x, y);
        }
    }

    // call from GLFW key callback: left/right arrows rotate camera 90° and camera position updates next render
    void handleKey(int key) {
        if (!showGameScene_) return;

        // 若相机正在旋转，忽略新的旋转输入
        if (renderer_.isRotating()) return;

        if (key == GLFW_KEY_U) renderer_.rotateCameraBy90(true, 0.5);
        else if (key == GLFW_KEY_I) renderer_.rotateCameraBy90(false, 0.5);
    }

    // 新增：由应用层启动位移动画
    void beginMoveAnimation(float duration) {
        if (!showGameScene_) return;
        renderer_.beginMoveAnimation(duration);
    }

    // 新增：查询相机是否处于旋转中（用于输入加锁）
    bool isCameraRotating() const {
        return renderer_.isRotating();
    }

private:
    UIManager uiManager_;
    detail::GameRenderer renderer_;
    bool initialized_ = false;
    bool showGameScene_ = false;
    std::function<void()> startCallback_;
    float lastMouseX_ = 0.0f;
    float lastMouseY_ = 0.0f;

    float animationDuration_ = 0.5f;
};

#endif // GAME_VIEW_HPP
