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
#include <memory>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "model/include/gameplay.hpp"
#include "model/include/level_loader.hpp"
#include "view/uimanager.hpp"
#include "view/shader.hpp"
#include "model/portal/portal.h"

namespace detail {

class GameRenderer {
public:
    void init(int windowWidth, int windowHeight) {
        windowWidth_ = windowWidth;
        windowHeight_ = windowHeight;
        textureWidth_ = windowWidth_;
        textureHeight_ = windowHeight_;

        // 使用封装的 Shader 类从文件加载
        basicShader_ = std::make_unique<Shader>("view/shader/basic.vert", "view/shader/basic.frag");
        softcubeShader_ = std::make_unique<Shader>("view/shader/softcube.vert", "view/shader/softcube.frag");
        portalSurfaceShader_ = std::make_unique<Shader>("view/shader/portalSurf.vert", "view/shader/portalSurf.frag");

        // 基础几何（pos3 + color3）
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

        // 软体立方体（pos3 + color3 + normal2 + tex3）
        glGenVertexArrays(1, &vaoSoft_);
        glGenBuffers(1, &vboSoft_);
        glBindVertexArray(vaoSoft_);
        glBindBuffer(GL_ARRAY_BUFFER, vboSoft_);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

        const GLsizei softStride = static_cast<GLsizei>(11 * sizeof(float));
        glEnableVertexAttribArray(0); // aPos
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, softStride, (void*)0);
        glEnableVertexAttribArray(1); // aColor
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, softStride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2); // aNormal (vec2)
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, softStride, (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(3); // aTex (vec3)
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, softStride, (void*)(8 * sizeof(float)));

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
        // 缩短时长以提升轻快感，若调用者提供更短时长，可按调用者值
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
        if (!basicShader_) return;
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
                const float vmax = 1.0f / (constv + 0.5f * (acc + dec));

                if (u <= acc) {
                    moveT = 0.5f * (vmax / acc) * u * u;
                } else if (u <= acc + constv) {
                    const float s_acc = 0.5f * vmax * acc;
                    moveT = s_acc + vmax * (u - acc);
                } else {
                    const float s_acc = 0.5f * vmax * acc;
                    const float s_const = vmax * constv;
                    float ud = u - (acc + constv);
                    moveT = s_acc + s_const + vmax * ud - 0.5f * (vmax / dec) * ud * ud;
                }

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

        // New: render portals
        if (state.player.room >= 0 && state.player.room < static_cast<int>(level.rooms.size())) {
            // Toll portals in current room
            std::vector<Portal*> portalsToRender;
            for (const auto& portal : portalsList) {
                if (portal->getPortalPos(state.boxrooms).room == state.player.room) {
                    portalsToRender.push_back(portal);
                }
            }

            // Move all portals to appropriate positions: moving boxrooms
            for (const auto& portal : portalsToRender) {
                // If this is a stationary portal, skip moving
                if (portal->stationary) {
                    continue;
                }

                // 1. Get boxroom ID, start pos & end pos
                int boxroomID = portal->boxroomID;
                auto start = state.boxrooms.find(boxroomID);
                auto end = next_state.boxrooms.find(boxroomID);
                if (start == state.boxrooms.end() || end == next_state.boxrooms.end()) {
                    continue;
                }
                const Pos& startPos = start->second;
                const Pos& endPos = end->second;

                // 2. Get world coordinate of the portal at start & end
                glm::vec3 startWorldPos = computePortalPosition(level.rooms[startPos.room], startPos.x, startPos.y, portal->relativePos, tileWorldSize_, 0.96f);
                glm::vec3 endWorldPos = computePortalPosition(level.rooms[endPos.room], endPos.x, endPos.y, portal->relativePos, tileWorldSize_, 0.96f);

                // 3. Calculate current pos (interpolation with moveT) and update portal position
                if (startPos.room == state.player.room && 
                    endPos.room == state.player.room &&
                    (startPos.x != endPos.x || startPos.y != endPos.y) &&
                    moving_) 
                {
                    portal->setPosition(startWorldPos + (endWorldPos - startWorldPos) * moveT);
                }
                else if (startPos.room == state.player.room && (!moving_ || (startPos.room != endPos.room || (startPos.x == endPos.x && startPos.y == endPos.y)))) {
                    portal->setPosition(startWorldPos);
                }
                else if (!moving_ && endPos.room == state.player.room) {
                    portal->setPosition(endWorldPos);
                }
            }

            // Initialize all portals with recursive render
            glm::mat4 cameraView = getCameraView(level.rooms[state.player.room], tileWorldSize_);
            for (const auto& portal : portalsToRender) {
                renderPortalRecursive(portal, cameraView, 5, state, level, next_state, moveT);
            }

            // Render scene: now use renderRoomIndexWithPortals
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, windowWidth_, windowHeight_);
            glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            renderRoomIndexWithPortals(state.player.room, portalsToRender, nullptr, state, level, next_state, moveT);
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
        if (vboSoft_) {
            glDeleteBuffers(1, &vboSoft_);
            vboSoft_ = 0;
        }
        if (vaoSoft_) {
            glDeleteVertexArrays(1, &vaoSoft_);
            vaoSoft_ = 0;
        }
        if (basicShader_) {
            glDeleteProgram(basicShader_->ID);
            basicShader_.reset();
        }
        if (softcubeShader_) {
            glDeleteProgram(softcubeShader_->ID);
            softcubeShader_.reset();
        }
        if (portalSurfaceShader_) {
            glDeleteProgram(portalSurfaceShader_->ID);
            portalSurfaceShader_.reset();
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

    void registerPortals(std::array<int, 2> entry, int boxroomID, Pos boxroomPos, const Room& outerRoom, const Room& boxroom) {
        int y = entry[0];
        int x = entry[1];
        
        Pos outerPortalPos = boxroomPos;
        Pos innerPortalPos = {boxroomID, x, y};
        
        PortalPosition relativePos;
        glm::vec3 innerNormal, outerNormal;
        if (x == 0) {
            relativePos = XNeg;
            outerNormal = glm::vec3(-1, 0, 0);
            innerNormal = glm::vec3(1, 0, 0);
        }
        else if (x == boxroom.size - 1) {
            relativePos = XPos;
            outerNormal = glm::vec3(1, 0, 0);
            innerNormal = glm::vec3(-1, 0, 0);
        }
        else if (y == 0) {
            relativePos = ZPos;
            outerNormal = glm::vec3(0, 0, 1);
            innerNormal = glm::vec3(0, 0, -1);
        }
        else if (y == boxroom.size - 1) {
            relativePos = ZNeg;
            outerNormal = glm::vec3(0, 0, -1);
            innerNormal = glm::vec3(0, 0, 1);
        }
        else {
            std::cerr << "ERROR: invalid portal position at " << boxroomID
                << ", (" << x << ", " << y << ")" << std::endl;
            return;
        }

        glm::vec3 innerWorldPos = computePortalPosition(boxroom, innerPortalPos.x, innerPortalPos.y, relativePos, tileWorldSize_, 0.96f);
        glm::vec3 outerWorldPos = computePortalPosition(outerRoom, outerPortalPos.x, outerPortalPos.y, relativePos, tileWorldSize_, 0.96f);

        Portal* portalOutside = new Portal(outerPortalPos, relativePos, boxroomID, windowHeight_, windowWidth_, outerWorldPos, outerNormal, 0.96f, tileWorldSize_ * 0.96f);
        Portal* portalInBox = new Portal(innerPortalPos, relativePos, boxroomID, windowHeight_, windowWidth_, innerWorldPos, innerNormal, 0.96f, tileWorldSize_ * 0.96f);
        portalOutside->setPairPortal(portalInBox);
        portalInBox->setPairPortal(portalOutside);
        portalOutside->setVAOs();
        portalInBox->setVAOs();

        // New: need to distinguish between movable / stationary portals!
        // portalInBox is always stationary.
        portalInBox->stationary = true;

        portalsList.push_back(portalOutside);
        portalsList.push_back(portalInBox);
    }

    void clearPortals(void) {
        for (const auto portal : portalsList) {
            delete portal;
        }
    }

    // Handle resize: update stored scr params of portals and update projection matrix
    void handleResize(int width, int height) {
        // 1. Change member var.s
        windowWidth_ = width;
        windowHeight_ = height;

        // 2. Update projection
        updateProjection();

        // 3. Update portals
        for (const auto& portal : portalsList) {
            portal->resizeFrameBuffer(width, height);
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

    // New: Separate camera view calculation from renderRoomIndex
    glm::mat4 getCameraView(const Room& room, const float tileSize) {
        const int tileCount = room.size;
        const float boardHalf = tileCount * tileSize * 0.5f;

        glm::vec3 front;
        float yawRad = glm::radians(cameraYaw_);
        float pitchRad = glm::radians(cameraPitch_);
        front.x = std::cos(pitchRad) * std::cos(yawRad);
        front.y = std::sin(pitchRad);
        front.z = std::cos(pitchRad) * std::sin(yawRad);
        front = glm::normalize(front);

        /*cameraForward2D_ = glm::vec2(front.x, front.z);
        if (glm::dot(cameraForward2D_, cameraForward2D_) > 1e-5f) {
            cameraForward2D_ = glm::normalize(cameraForward2D_);
        }
        else {
            cameraForward2D_ = glm::vec2(1.0f, 0.0f);
        }*/

        glm::vec3 roomCenter = glm::vec3(0.0f, 0.02f, 0.0f);
        glm::vec3 forwardXZ = glm::vec3(front.x, 0.0f, front.z);
        glm::vec3 offsetDir = glm::length(forwardXZ) < 1e-5f ? glm::vec3(-1.0f, 0.0f, 0.0f) : -glm::normalize(forwardXZ);

        glm::vec3 cameraPos = roomCenter + offsetDir * boardHalf + glm::vec3(0.0f, 3.0f, 0.0f);
        glm::mat4 viewMatrix = glm::lookAt(cameraPos, roomCenter, glm::vec3(0.0f, 1.0f, 0.0f));

        return viewMatrix;
    }

    // 新增：支持插值渲染，依据 state 与 next_state 以及 moveT
    // Now features model matrix (basicShader_), clip plane toggling and virtual view toggling
    void renderRoomIndex(int roomId, const GameState& state, const Level& level, const GameState& next_state, float moveT,
                         bool enableClip = false, glm::vec4 clipPlane = glm::vec4(0.0f), bool enableVirtualView = false, glm::mat4 virtualView = glm::mat4(0.0f))
    {
        if (roomId < 0 || roomId >= static_cast<int>(level.rooms.size())) return;
        const Room& room = level.rooms[roomId];
        if (room.size <= 0) return;

        vertexData_.clear();
        softVertexData_.clear();
        moveDirection_ = glm::vec2(0.0f); // 默认无方向（用于非移动或非本房间）

        currentRoomHalfExtent_ = appendRoomGeometry(room, roomId, state, next_state, moveT, tileWorldSize_);

        /*glm::vec3 front;
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
        view_ = glm::lookAt(cameraPos, roomCenter, glm::vec3(0.0f, 1.0f, 0.0f));*/

        glm::mat4 viewToUse = enableVirtualView ? virtualView : getCameraView(room, tileWorldSize_);

        // 绘制静态几何（地面/墙/箱子）
        if (!vertexData_.empty()) {
            basicShader_->use();
            basicShader_->setMat4("model", glm::mat4(1.0f));
            basicShader_->setMat4("view", viewToUse);
            basicShader_->setMat4("projection", projection_);
            basicShader_->setVec4("clipPlane", clipPlane);
            basicShader_->setBool("enableClip", enableClip);

            glBindVertexArray(vao_);
            glBindBuffer(GL_ARRAY_BUFFER, vbo_);
            glBufferData(GL_ARRAY_BUFFER, vertexData_.size() * sizeof(float), vertexData_.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexData_.size() / 6));
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        // 绘制角色（软体立方体 + 着色器形变）
        if (!softVertexData_.empty() && softcubeShader_) {
            softcubeShader_->use();
            softcubeShader_->setMat4("view", viewToUse);
            softcubeShader_->setMat4("projection", projection_);
            softcubeShader_->setVec2("direction", moveDirection_);
            softcubeShader_->setFloat("moveT", moveT);
            softcubeShader_->setVec4("clipPlane", clipPlane);
            softcubeShader_->setBool("enableClip", enableClip);

            // 待机时间（周期 idleDuration_）
            float idleT = 0.0f;
            if (idleDuration_ > 1e-5f) {
                float now = static_cast<float>(glfwGetTime());
                idleT = std::fmod(now, idleDuration_) / idleDuration_;
            }
            softcubeShader_->setFloat("idleT", idleT);

            glBindVertexArray(vaoSoft_);
            glBindBuffer(GL_ARRAY_BUFFER, vboSoft_);
            glBufferData(GL_ARRAY_BUFFER, softVertexData_.size() * sizeof(float), softVertexData_.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(softVertexData_.size() / 11));
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        glUseProgram(0);
    }

    float appendRoomGeometry(const Room& room, int roomId, const GameState& state, const GameState& next_state, float moveT, float tileSize) {
        const int tileCount = room.size;
        const float boardHalf = tileCount * tileSize * 0.5f;

        playerEyePosition_ = glm::vec3(0.0f, 0.02f, 0.0f);

        auto pushVertex = [&](const glm::vec3& pos, const glm::vec3& color) {
            vertexData_.push_back(pos.x);
            vertexData_.push_back(pos.y);
            vertexData_.push_back(pos.z);
            vertexData_.push_back(color.r);
            vertexData_.push_back(color.g);
            vertexData_.push_back(color.b);
        };

        auto pushQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec3& color) {
            pushVertex(v0, color);
            pushVertex(v1, color);
            pushVertex(v2, color);
            pushVertex(v0, color);
            pushVertex(v2, color);
            pushVertex(v3, color);
        };

        // 软体立方体：扩展布局 push
        auto pushSoftVertex = [&](const glm::vec3& pos, const glm::vec3& color, const glm::vec2& n2, const glm::vec3& tex) {
            softVertexData_.push_back(pos.x);
            softVertexData_.push_back(pos.y);
            softVertexData_.push_back(pos.z);
            softVertexData_.push_back(color.r);
            softVertexData_.push_back(color.g);
            softVertexData_.push_back(color.b);
            softVertexData_.push_back(n2.x);
            softVertexData_.push_back(n2.y);
            softVertexData_.push_back(tex.x);
            softVertexData_.push_back(tex.y);
            softVertexData_.push_back(tex.z);
        };

        auto pushSoftQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3,
                                const glm::vec3& color, const glm::vec2& n2, float cx, float cz, float halfW) {
            // aTex.xy = 相对中心归一化偏移，aTex.z = halfW
            auto makeTex = [&](const glm::vec3& p) -> glm::vec3 {
                float xNorm = (halfW > 1e-6f) ? (p.x - cx) / halfW : 0.0f;
                float zNorm = (halfW > 1e-6f) ? (p.z - cz) / halfW : 0.0f;
                return glm::vec3(xNorm, zNorm, halfW);
            };
            pushSoftVertex(v0, color, n2, makeTex(v0));
            pushSoftVertex(v1, color, n2, makeTex(v1));
            pushSoftVertex(v2, color, n2, makeTex(v2));
            pushSoftVertex(v0, color, n2, makeTex(v0));
            pushSoftVertex(v2, color, n2, makeTex(v2));
            pushSoftVertex(v3, color, n2, makeTex(v3));
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

        // CPU 端软体立方体：不做待机形变，只输出细分网格与 aNormal/aTex
        auto appendSoftCube = [&](float minX, float maxX, float minZ, float maxZ, float minY, float maxY, 
            const glm::vec3& color, float /*amp*/, float /*timeT*/) {
            const int RES = 16;

            const float cx = 0.5f * (minX + maxX);
            const float cz = 0.5f * (minZ + maxZ);
            const float hx = 0.5f * (maxX - minX);
            const float hz = 0.5f * (maxZ - minZ);
            const float halfW = std::max(hx, hz);

            const glm::vec3 topColor = color;
            const glm::vec3 sideColor = color * 0.85f;
            const glm::vec3 bottomColor = color * 0.80f;

            auto emitFaceGrid = [&](int axis, float fixed,
                                    float u0, float u1, float v0, float v1,
                                    const glm::vec3& faceColor, const glm::vec2& n2) {
                for (int i = 0; i < RES; ++i) {
                    float a0 = static_cast<float>(i) / RES;
                    float a1 = static_cast<float>(i + 1) / RES;
                    float uu0 = u0 + (u1 - u0) * a0;
                    float uu1 = u0 + (u1 - u0) * a1;

                    for (int j = 0; j < RES; ++j) {
                        float b0 = static_cast<float>(j) / RES;
                        float b1 = static_cast<float>(j + 1) / RES;
                        float vv0 = v0 + (v1 - v0) * b0;
                        float vv1 = v0 + (v1 - v0) * b1;

                        glm::vec3 p00, p10, p11, p01;
                        if (axis == 1) { // 顶/底 (x,z)
                            p00 = glm::vec3(uu0, fixed, vv0);
                            p10 = glm::vec3(uu1, fixed, vv0);
                            p11 = glm::vec3(uu1, fixed, vv1);
                            p01 = glm::vec3(uu0, fixed, vv1);
                        } else if (axis == 0) { // ±X (y,z)
                            p00 = glm::vec3(fixed, uu0, vv0);
                            p10 = glm::vec3(fixed, uu1, vv0);
                            p11 = glm::vec3(fixed, uu1, vv1);
                            p01 = glm::vec3(fixed, uu0, vv1);
                        } else { // ±Z (x,y)
                            p00 = glm::vec3(uu0, vv0, fixed);
                            p10 = glm::vec3(uu1, vv0, fixed);
                            p11 = glm::vec3(uu1, vv1, fixed);
                            p01 = glm::vec3(uu0, vv1, fixed);
                        }

                        // 不再做 CPU 端形变
                        auto makeTex = [&](const glm::vec3& p) -> glm::vec3 {
                            float xNorm = (halfW > 1e-6f) ? (p.x - cx) / halfW : 0.0f;
                            float zNorm = (halfW > 1e-6f) ? (p.z - cz) / halfW : 0.0f;
                            return glm::vec3(xNorm, zNorm, halfW);
                        };

                        pushSoftQuad(p00, p10, p11, p01, faceColor, n2, cx, cz, halfW);
                    }
                }
            };

            // 顶/底：aNormal = (0,0)
            emitFaceGrid(1, maxY, minX, maxX, minZ, maxZ, topColor,   glm::vec2(0.0f, 0.0f));
            emitFaceGrid(1, minY, minX, maxX, minZ, maxZ, bottomColor,glm::vec2(0.0f, 0.0f));
            // ±X：aNormal = (±1,0)
            emitFaceGrid(0, minX, minY, maxY, minZ, maxZ, sideColor,  glm::vec2(-1.0f, 0.0f));
            emitFaceGrid(0, maxX, minY, maxY, minZ, maxZ, sideColor,  glm::vec2( 1.0f, 0.0f));
            // ±Z：aNormal = (0,±1)
            emitFaceGrid(2, minZ, minX, maxX, minY, maxY, sideColor,  glm::vec2(0.0f,-1.0f));
            emitFaceGrid(2, maxZ, minX, maxX, minY, maxY, sideColor,  glm::vec2(0.0f, 1.0f));
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

        auto drawPlayerAtCenter = [&](const glm::vec2& centerXZ, const glm::vec3& color, float height, float idleT) {
            const float inset = tileSize * 0.10f;
            const float halfW = (tileSize * 0.5f) - inset;
            float minX = centerXZ.x - halfW;
            float maxX = centerXZ.x + halfW;
            float minZ = centerXZ.y - halfW;
            float maxZ = centerXZ.y + halfW;
            appendSoftCube(minX, maxX, minZ, maxZ, 0.02f, 0.02f + height, color, 0.05f, idleT);
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

        // 计算玩家插值 + 设置运动方向
        auto drawPlayer = [&]() {
            const Pos& s = state.player;
            const Pos& e = next_state.player;
            glm::vec3 color(0.25f, 0.85f, 0.35f);
            float height = 0.90f;

            // 待机动画时间（周期由 idleDuration_ 控制）
            float idleT = 0.0f;
            if (idleDuration_ > 1e-5f) {
                float now = static_cast<float>(glfwGetTime());
                idleT = std::fmod(now, idleDuration_) / idleDuration_;
            }

            if (s.room == roomId && e.room == roomId && (s.x != e.x || s.y != e.y) && moving_) {
                glm::vec2 cs = centerForCell(s.x, s.y);
                glm::vec2 ce = centerForCell(e.x, e.y);
                glm::vec2 dir = ce - cs;
                if (glm::dot(dir, dir) > 1e-6f) dir = glm::normalize(dir);
                else dir = glm::vec2(0.0f);
                moveDirection_ = dir; // 供 softcubeShader_ 使用

                glm::vec2 c = cs + (ce - cs) * moveT;
                drawPlayerAtCenter(c, color, height, idleT);
            } else if (s.room == roomId && (!moving_ || (s.room != e.room || (s.x == e.x && s.y == e.y)))) {
                moveDirection_ = glm::vec2(0.0f);
                glm::vec2 c = centerForCell(s.x, s.y);
                drawPlayerAtCenter(c, color, height, idleT);
            } else if (!moving_ && e.room == roomId) {
                moveDirection_ = glm::vec2(0.0f);
                glm::vec2 c = centerForCell(e.x, e.y);
                drawPlayerAtCenter(c, color, height, idleT);
            }
        };

        // 计算箱子插值（保持基础几何）
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
        drawPlayer();

        return boardHalf;
    }

    glm::vec3 tileColorForCell(const std::string& cell) const {
        if (cell == "#") return {0.2f, 0.2f, 0.2f};
        if (cell == "=") return {0.25f, 0.6f, 0.3f};
        if (cell == "_") return {0.7f, 0.6f, 0.25f};
        return {0.15f, 0.15f, 0.15f};
    }

    glm::vec3 computePortalPosition(const Room& room, int gx, int gy, PortalPosition pos, float tileSize, float portalHeight, float baseY = 0.02f, float epsilon = 0.01f) {
        float boardHalf = room.size * tileSize * 0.5f;
        float minX = -boardHalf + gx * tileSize;
        float maxX = minX + tileSize;
        float maxZ = boardHalf - gy * tileSize;
        float minZ = maxZ - tileSize;
        float cx = 0.5f * (minX + maxX);
        float cz = 0.5f * (minZ + maxZ);
        glm::vec3 position(0.0f);

        switch (pos) {
        case XPos:
            position = glm::vec3(maxX + epsilon, baseY + portalHeight * 0.5f, cz);
            break;
        case XNeg:
            position = glm::vec3(minX - epsilon, baseY + portalHeight * 0.5f, cz);
            break;
        case ZPos:
            position = glm::vec3(cx, baseY + portalHeight * 0.5f, maxZ + epsilon);
            break;
        case ZNeg:
            position = glm::vec3(cx, baseY + portalHeight * 0.5f, minZ - epsilon);
            break;
        }

        return position;
    }

    // New: Render the scene with portals
    // The portals to be rendered are given in portalsToRender.
    void renderRoomIndexWithPortals(int roomId, std::vector<Portal*> portalsToRender, Portal* checkCurrent, const GameState& state, const Level& level, const GameState& next_state, float moveT,
        bool enableClip = false, glm::vec4 clipPlane = glm::vec4(0.0f), bool enableVirtualView = false, glm::mat4 virtualView = glm::mat4(0.0f))
    {
        if (roomId < 0 || roomId >= static_cast<int>(level.rooms.size())) return;
        const Room& room = level.rooms[roomId];
        if (room.size <= 0) return;
        glm::mat4 viewToUse = enableVirtualView ? virtualView : getCameraView(room, tileWorldSize_);

        // Render the rest of the room first, without portals
        renderRoomIndex(roomId, state, level, next_state, moveT,
            enableClip, clipPlane,
            enableVirtualView, virtualView);

        // Render portals
        for (const auto& portal : portalsToRender) {
            // Render portal wrapper (frame)
            basicShader_->use();
            basicShader_->setMat4("model", portal->getModelMatrix());
            basicShader_->setMat4("view", viewToUse);
            basicShader_->setMat4("projection", projection_);
            basicShader_->setVec4("clipPlane", clipPlane);
            basicShader_->setBool("enableClip", enableClip);

            glBindVertexArray(portal->wrapperVAO_);
            glDrawArrays(GL_TRIANGLES, 0, portal->wrapperVertexNum);
            glBindVertexArray(0);

            // Render portal surface
            portalSurfaceShader_->use();
            portalSurfaceShader_->setMat4("model", portal->getModelMatrix());
            portalSurfaceShader_->setMat4("view", viewToUse);
            portalSurfaceShader_->setMat4("projection", projection_);
            portalSurfaceShader_->setVec4("clipPlane", clipPlane);
            portalSurfaceShader_->setBool("enableClip", enableClip);

            glBindVertexArray(portal->portalVAO_);
            unsigned int texID = (portal == checkCurrent) ? portal->tempTexture : portal->texture;
            glBindTexture(GL_TEXTURE_2D, texID);
            glDrawArrays(GL_TRIANGLES, 0, portal->portalVertexNum);
            glBindVertexArray(0);
        }

        glUseProgram(0);
    }

    // A recursive function that renders portal textures.
    // currentPortal: The portal currently being rendered
    // view: Current view matrix to observe the current portal
    // depth: Remaining recursion depth
    void renderPortalRecursive(Portal* currentPortal, glm::mat4 view, int depth, const GameState& state, const Level& level, const GameState& next_state, float moveT) {
        // Between a pair of portals, the recursion *only* updates the texture of the *current* portal!
        // The other portal is always used as a view point, and is NEVER visible!

        // 0. Calculate Virtual View
        // This is the view out of pairPortal.
        glm::mat4 virtualView = currentPortal->getPortalCameraView(view);
        Portal* pairPortal = currentPortal->pairPortal;

        // 1. Base Case: End of recursion
        // At deepest level: render scene without portal
        if (depth <= 0) {
            // Set Up: Render pairPortal's view to FBO (currentPortal)
            glBindFramebuffer(GL_FRAMEBUFFER, currentPortal->fbo);
            glEnable(GL_DEPTH_TEST);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Enable clipping plane
            glEnable(GL_CLIP_DISTANCE0);

            // Render the scene without portals
            renderRoomIndex(pairPortal->getPortalPos(state.boxrooms).room, state, level, next_state, moveT,
                true, currentPortal->getPairPortalClippingPlane(),
                true, virtualView);

            // Disable clipping plane
            glDisable(GL_CLIP_DISTANCE0);

            // Unbind
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            return;
        }        

        // 2. Deepest First: Recursively render deeper levels FIRST!
        // We need to ensure that all other portals in the room of pairPortal already has the newest texture.
        // First toll portals in the room of pairPortal, then calc depth, finally recursively render.
        // Keep in mind that pairPortal is never rendered in this recursion!
        std::vector<Portal*> portalsToRender;
        int pairRoomID = pairPortal->getPortalPos(state.boxrooms).room;
        bool renderCurrent = false;
        
        for (const auto& portal : portalsList) {
            if (portal->getPortalPos(state.boxrooms).room == pairRoomID && portal != pairPortal) {
                portalsToRender.push_back(portal);
                if (currentPortal == portal) {
                    renderCurrent = true;
                }
            }
        }

        int nextDepth = std::max(depth - static_cast<int>(portalsToRender.size()), 0);
        for (const auto& portal : portalsToRender) {
            renderPortalRecursive(portal, virtualView, nextDepth, state, level, next_state, moveT);
        }

        // 3. Texture Copy (Optional)
        // This is only necessary if we are about to render currentPortal,
        // i.e. currentPortal is in the same room as pairPortal.
        // currentPortal->texture contains the texture from level depth-1.
        // We need to copy it to tempTexture, since we are about to write to texture (FBO).

        if (renderCurrent) {
            // Bind FBO as source
            glBindFramebuffer(GL_READ_FRAMEBUFFER, currentPortal->fbo);
            glBindTexture(GL_TEXTURE_2D, currentPortal->tempTexture);

            // Copy content from FBO to tempTexture
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, currentPortal->scrWidth, currentPortal->scrHeight);

            glBindTexture(GL_TEXTURE_2D, 0);
        }

        // 4. Set Up: Render pairPortal's view to FBO (currentPortal)
        glBindFramebuffer(GL_FRAMEBUFFER, currentPortal->fbo);
        glEnable(GL_DEPTH_TEST);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Enable clipping plane
        glEnable(GL_CLIP_DISTANCE0);

        // 5. Render the Scene
        // Draw other scene obj.s and all portals in the room
        renderRoomIndexWithPortals(pairRoomID, portalsToRender, currentPortal, state, level, next_state, moveT,
            true, currentPortal->getPairPortalClippingPlane(),
            true, virtualView);

        // Disable clipping plane
        glDisable(GL_CLIP_DISTANCE0);

        // Unbind
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
    GLuint vaoSoft_ = 0;
    GLuint vboSoft_ = 0;
    std::unique_ptr<Shader> basicShader_;
    std::unique_ptr<Shader> softcubeShader_;
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
    std::vector<float> softVertexData_;
    glm::vec2 moveDirection_{0.0f, 0.0f};

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

    // 待机动画状态（玩家）
    float idleDuration_ = 3.0f;

    // New: portals
    std::vector<Portal*> portalsList;
    std::unique_ptr<Shader> portalSurfaceShader_;
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

    // New: register portals from level
    /// @brief Register portals from loaded level & initial state
    /// @details Create portals using input level, initial state and other info, call once after loading a new level
    void registerPortals(const GameState& initialState, const Level* level) {
        for (const auto& [rid, boxroomPos] : initialState.boxrooms) {
            Room boxroom = level->rooms[rid];
            for (const auto& entry : boxroom.entries) {
                renderer_.registerPortals(entry, rid, boxroomPos, level->rooms[boxroomPos.room], boxroom);
            }
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

    // Call from GLFW fb resize callback
    void handleResize(int width, int height) {
        renderer_.handleResize(width, height);
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
