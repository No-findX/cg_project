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
#include <fstream>
#include <sstream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>

#include "model/include/gameplay.hpp"
#include "model/include/level_loader.hpp"
#include "view/uimanager.hpp"
#include "view/shader.hpp"
#include "model/portal/portal.h"
#include "view/gamelight.hpp"
#include "view/skybox_loader.hpp"

namespace detail {

#define RGB_2_FLT(x) ((((x)>>16) & 0xff) / 255.0f), ((((x)>>8) & 0xff) / 255.0f), (((x) & 0xff) / 255.0f)

class GameRenderer {
public:
    struct ObjThroughPortal {
        bool exists = false;
        bool renderTwice = false;
        bool isPlayer = false;
        Portal* portal = nullptr;
        std::vector<float> vertexData;
    };

    struct LoadedObjData {
        glm::mat4 model = glm::mat4(1.0);
        const std::vector<float>* vertexData;
    };

    // 初始化渲染器：设置窗口尺寸、加载 Shader、初始化光照系统、阴影资源、Skybox、VAO/VBO 等
    void init(int windowWidth, int windowHeight) {
        windowWidth_ = windowWidth;
        windowHeight_ = windowHeight;
        textureWidth_ = windowWidth_;
        textureHeight_ = windowHeight_;        

        // 使用封装的 Shader 类加载着色器
        shader_ = std::make_unique<Shader>("view/shader/pbr.vert", "view/shader/pbr.frag");
        
        // 初始化光照系统
        lightingSystem_ = std::make_unique<LabLightingSystem>();
        depthShader_ = std::make_unique<Shader>("view/shader/shadow_depth.vert", "view/shader/shadow_depth.frag");
        softDepthShader_ = std::make_unique<Shader>("view/shader/soft_shadow_depth.vert", "view/shader/soft_shadow_depth.frag");
        setupShadowResources();
        basicShader_ = std::make_unique<Shader>("view/shader/basic.vert", "view/shader/basic.frag");
        softcubeShader_ = std::make_unique<Shader>("view/shader/softcube.vert", "view/shader/softcube.frag");
        skyboxShader_ = std::make_unique<Shader>("view/shader/skybox.vert", "view/shader/skybox.frag");
        portalSurfaceShader_ = std::make_unique<Shader>("view/shader/portalSurf.vert", "view/shader/portalSurf.frag");

        // 初始化 SkyBox（加载立方体贴图）
        std::vector<std::string> faces = {
            "view/assest/skybox/px.png",
            "view/assest/skybox/nx.png",
            "view/assest/skybox/py.png",
            "view/assest/skybox/ny.png",
            "view/assest/skybox/pz.png",
            "view/assest/skybox/nz.png"
        };
        skybox_ = std::make_unique<SkyBox>(faces);
        // 绑定 skyboxShader_ 的纹理单元为 "skybox"（绑定到 GL_TEXTURE0）
        if (skyboxShader_) {
            skyboxShader_->use();
            skyboxShader_->setInt("skybox", 0);
            glUseProgram(0);
        }

        // 设置 VAO/VBO：pos3 + normal3 + color3 + tex2
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

        const GLsizei pbrStride = static_cast<GLsizei>(11 * sizeof(float));
        glEnableVertexAttribArray(0); // Pos
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, pbrStride, (void*)0);
        glEnableVertexAttribArray(1); // Normal
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, pbrStride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2); // Color
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, pbrStride, (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(3); // Tex
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, pbrStride, (void*)(9 * sizeof(float)));

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        tileTex_ = loadTexture2D("view/assest/texture/tile.jpg");
        wallTex_ = loadTexture2D("view/assest/texture/wall.png");
        boxTex_ = loadTexture2D("view/assest/texture/box.jpg");

        // 设置 basicShader_ 的纹理单元
        if (basicShader_) {
            basicShader_->use();
            basicShader_->setInt("Texture", 0);
            glUseProgram(0);
        }

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

        // 加载椅子模型
        loadModel("resource/modern chair 11 obj.obj", rawChairData_, chairColor);
        
    }

    // no-op: remove mouse-drag free rotation for 2.5D style
    void rotateCamera(float /*deltaX*/, float /*deltaY*/) {
    }

    // 摄像机旋转 90 度（平滑过渡）
    void rotateCameraBy90(bool left, float rotationtime = 0.0) {
        const float step = left ? -90.0f : 90.0f;

        // 如果正在旋转，则忽略新的旋转请求（输入锁定）
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
    void beginMoveAnimation(float duration, Input input) {
        // 缩短时长以提升轻快感，若调用者提供更短时长，可按调用者值
        const float preferred = 0.3f;
        //moveDuration_ = std::min(duration > 0.0f ? duration : preferred, preferred);
        moveDuration_ = duration > 0.0f ? duration : preferred;
        moveStartTime_ = static_cast<float>(glfwGetTime());
        moving_ = true;
        moveInput_ = input;
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
        
        bool wasRotating = rotating_;

        // 相机旋转动画（平滑）
        if (rotating_) {
            const float now = static_cast<float>(glfwGetTime());
            float elapsed = now - rotateStartTime_;
            if (elapsed >= rotateDuration_ * 0.98f) { // 接近结束时直接完成
                cameraYaw_ = rotateTargetYaw_;
                // 规范化角度到 [-180, 180] 范围，防止数值溢出
                while (cameraYaw_ > 180.0f) cameraYaw_ -= 360.0f;
                while (cameraYaw_ < -180.0f) cameraYaw_ += 360.0f;
                rotating_ = false;
            } else {
                float u = elapsed / rotateDuration_;
                if (u < 0.0f) u = 0.0f;
                if (u > 1.0f) u = 1.0f;
                float p = u * u * (3.0f - 2.0f * u); // 平滑插值
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

        //bool texturesChanged = ensureRoomTextures(level.rooms.size());
        //bool stateChanged = isStateChanged(state, lastState_);
        //bool roomChanged = (state.player.room != lastRoomId_);
        //bool needsUpdate = texturesChanged || stateChanged || roomChanged || moving_ || wasRotating;

        //if (needsUpdate) {
        //    for (size_t i = 0; i < level.rooms.size(); ++i) {
        //        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        //        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, roomTextures_[i], 0);
        //        GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
        //        glDrawBuffers(1, drawBuffers);

        //        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        //            std::cerr << "Framebuffer not complete for room " << i << std::endl;
        //        }

        //        glViewport(0, 0, textureWidth_, textureHeight_);
        //        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        //        glClear(GL_COLOR_BUFFER_BIT);

        //        renderRoomIndex(static_cast<int>(i), state, level, next_state, moveT/*, true*/);

        //        glBindTexture(GL_TEXTURE_2D, 0);
        //        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        //    }
        //    lastState_ = state;
        //    lastRoomId_ = state.player.room;
        //}

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
                glm::vec3 startWorldPos = computePortalPosition(level.rooms[startPos.room], startPos.x, startPos.y, portal->relativePos, tileWorldSize_, portalHeight);
                glm::vec3 endWorldPos = computePortalPosition(level.rooms[endPos.room], endPos.x, endPos.y, portal->relativePos, tileWorldSize_, portalHeight);

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
            glm::mat4 skyboxView = getCameraView(level.rooms[state.player.room], tileWorldSize_, 1.25f, 4.0f);
            for (const auto& portal : portalsToRender) {
                renderPortalRecursive(portal, cameraView, skyboxView, 2, state, level, next_state, moveT, true);
            }

            // Render scene: now use renderRoomIndexWithPortals
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, windowWidth_, windowHeight_);
            glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            renderRoomIndexWithPortals(state.player.room, portalsToRender, nullptr, state, level, next_state, moveT, true);
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
        if (depthShader_) {
            glDeleteProgram(depthShader_->ID);
            depthShader_.reset();
        }
        if (softDepthShader_) {
            glDeleteProgram(softDepthShader_->ID);
            softDepthShader_.reset();
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
        if (skyboxShader_) {
            glDeleteProgram(skyboxShader_->ID);
            skyboxShader_.reset();
        }
        skybox_.reset();
        if (fbo_) {
            glDeleteFramebuffers(1, &fbo_);
            fbo_ = 0;
        }
        if (!roomTextures_.empty()) {
            glDeleteTextures(static_cast<GLsizei>(roomTextures_.size()), roomTextures_.data());
            roomTextures_.clear();
        }
        if (depthMap_) {
            glDeleteTextures(1, &depthMap_);
            depthMap_ = 0;
        }
        if (depthMapFBO_) {
            glDeleteFramebuffers(1, &depthMapFBO_);
            depthMapFBO_ = 0;
        }
        if (tileTex_) { glDeleteTextures(1, &tileTex_); tileTex_ = 0; }
        if (wallTex_) { glDeleteTextures(1, &wallTex_); wallTex_ = 0; }
        if (boxTex_)  { glDeleteTextures(1, &boxTex_);  boxTex_  = 0; }
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

        glm::vec3 innerWorldPos = computePortalPosition(boxroom, innerPortalPos.x, innerPortalPos.y, relativePos, tileWorldSize_, portalHeight);
        glm::vec3 outerWorldPos = computePortalPosition(outerRoom, outerPortalPos.x, outerPortalPos.y, relativePos, tileWorldSize_, portalHeight);

        Portal* portalOutside = new Portal(outerPortalPos, relativePos, boxroomID, windowHeight_, windowWidth_, outerWorldPos, outerNormal, portalHeight, tileWorldSize_ * 0.96f);
        Portal* portalInBox = new Portal(innerPortalPos, relativePos, boxroomID, windowHeight_, windowWidth_, innerWorldPos, innerNormal, portalHeight, tileWorldSize_ * 0.96f);
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
        portalsList.clear();
    }

    void resetCamera() {
        cameraYaw_ = 0.0f;
        rotateTargetYaw_ = 0.0f;
        rotating_ = false;
        moving_ = false;
        cameraPosition_ = glm::vec3(0.0f, 3.0f, 0.0f);
        objThroughPortalData = ObjThroughPortal();
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

    // 加载纹理工具函数
    GLuint loadTexture2D(const char* path) {
        int w = 0, h = 0, n = 0;
        stbi_set_flip_vertically_on_load(1);
        unsigned char* data = stbi_load(path, &w, &h, &n, 0);
        if (!data) {
            std::cerr << "Failed to load texture: " << path << std::endl;
            return 0;
        }
        GLenum format = GL_RGB;
        if (n == 1) format = GL_RED;
        else if (n == 3) format = GL_RGB;
        else if (n == 4) format = GL_RGBA;

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(data);
        return tex;
    }

    bool ensureRoomTextures(size_t count) {
        bool changed = false;
        if (textureWidth_ != windowWidth_ || textureHeight_ != windowHeight_) {
            textureWidth_ = windowWidth_;
            textureHeight_ = windowHeight_;
            if (!roomTextures_.empty()) {
                glDeleteTextures(static_cast<GLsizei>(roomTextures_.size()), roomTextures_.data());
                roomTextures_.clear();
                changed = true;
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
            changed = true;
        }
        return changed;
    }

    // New: Separate camera view calculation from renderRoomIndex
    // Now separate orbital position calculation and view matrix calculation
    glm::vec3 getOrbitalPosition(const glm::vec3& center, const glm::vec3& direction, float distance, float heightOffset) {
        return center + direction * distance + glm::vec3(0.0f, heightOffset, 0.0f);
    }
    
    glm::mat4 getCameraView(const Room& room, const float tileSize, float distanceScale = 1.0f, float heightOverride = 3.0f) {
        const int tileCount = room.size;
        const float boardHalf = tileCount * tileSize * 0.5f;

        glm::vec3 front;
        float yawRad = glm::radians(cameraYaw_);
        float pitchRad = glm::radians(cameraPitch_);
        front.x = std::cos(pitchRad) * std::cos(yawRad);
        front.y = std::sin(pitchRad);
        front.z = std::cos(pitchRad) * std::sin(yawRad);
        front = glm::normalize(front);

        glm::vec3 roomCenter = glm::vec3(0.0f, 0.02f, 0.0f);
        glm::vec3 forwardXZ = glm::vec3(front.x, 0.0f, front.z);
        glm::vec3 offsetDir = glm::length(forwardXZ) < 1e-5f ? glm::vec3(-1.0f, 0.0f, 0.0f) : -glm::normalize(forwardXZ);

        // Allows for different dist. and height
        float currentDistance = (room.size * tileSize * 0.5f) * distanceScale;
        glm::vec3 cameraPos = getOrbitalPosition(roomCenter, offsetDir, currentDistance, heightOverride);
        glm::mat4 viewMatrix = glm::lookAt(cameraPos, roomCenter, glm::vec3(0.0f, 1.0f, 0.0f));

        return viewMatrix;
    }

    void setupShadowResources() {
        glGenFramebuffers(1, &depthMapFBO_);
        glGenTextures(1, &depthMap_);
        glBindTexture(GL_TEXTURE_2D, depthMap_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadowMapResolution_, shadowMapResolution_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap_, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
 
    glm::mat4 calculateLightSpaceMatrix(const glm::vec3& roomCenter, float halfExtent) const {
        float orthoRange = halfExtent + 4.0f;
        glm::mat4 lightProjection = glm::ortho(-orthoRange, orthoRange, -orthoRange, orthoRange, 0.1f, 50.0f);
        glm::vec3 lightDir = glm::normalize(lightingSystem_->getMainLight().direction);
        glm::vec3 lightPos = roomCenter - lightDir * 20.0f; // 从远处光源位置进行正交投影
        glm::mat4 lightView = glm::lookAt(lightPos, roomCenter, glm::vec3(0.0f, 1.0f, 0.0f));
        return lightProjection * lightView;
    }
 
    void renderShadowPass(const glm::mat4& lightSpaceMatrix, const glm::mat4& model, GLsizei vertexCount) {
        if (!depthShader_ || vertexCount <= 0) return;
        GLint prevViewport[4];
        glGetIntegerv(GL_VIEWPORT, prevViewport);
        GLint prevFBO = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFBO);
 
        glViewport(0, 0, shadowMapResolution_, shadowMapResolution_);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO_);
        glClear(GL_DEPTH_BUFFER_BIT);
 
        glCullFace(GL_FRONT);
        depthShader_->use();
        depthShader_->setMat4("lightSpaceMatrix", lightSpaceMatrix);
        depthShader_->setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
        glUseProgram(0);
        glCullFace(GL_BACK);
 
        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
        glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    }

    // 新增：支持插值渲染，依据 state 与 next_state 以及 moveT
    // Now features model matrix (basicShader_), clip plane toggling and virtual view toggling
    // New: more clipping planes for passing-through-portal rendering
    void renderRoomIndex(int roomId, const GameState& state, const Level& level, const GameState& next_state, float moveT, /*bool rebuild = true,*/
                         bool enableClip = false, glm::vec4 clipPlane = glm::vec4(0.0f), bool enableVirtualView = false, glm::mat4 virtualView = glm::mat4(0.0f), glm::mat4 virtualSkyboxView = glm::mat4(0.0f))
    {
        if (roomId < 0 || roomId >= static_cast<int>(level.rooms.size())) return;
        const Room& room = level.rooms[roomId];
        if (room.size <= 0) return;

        // 优先计算摄像机位置和 View 矩阵
        // 确保在构建几何体（特别是墙体剔除）时使用当前房间对应的正确摄像机位置
        float currentRoomHalfExtent = room.size * tileWorldSize_ * 0.5f;

        glm::mat4 viewToUse = enableVirtualView ? virtualView : getCameraView(room, tileWorldSize_, 1.0f, 3.0f);
        glm::mat4 viewSkyboxToUse = enableVirtualView ? virtualSkyboxView : getCameraView(room, tileWorldSize_, 1.25f, 4.0f);

        moveDirection_ = glm::vec2(0.0f); // 默认无方向（用于非移动或非本房间）
        appendRoomGeometry(room, roomId, state, next_state, moveT, tileWorldSize_, enableVirtualView);
        
        glm::vec3 roomCenter = glm::vec3(0.0f, 0.02f, 0.0f);
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 lightSpaceMatrix = calculateLightSpaceMatrix(roomCenter, currentRoomHalfExtent);

        // 保存当前 FBO（可能是 roomTextures_ 的 FBO）
        GLint prevFBO = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFBO);
        GLint prevViewport[4];
        glGetIntegerv(GL_VIEWPORT, prevViewport);

        // 1. Shadow Pass (阴影贴图生成)
        glViewport(0, 0, shadowMapResolution_, shadowMapResolution_);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO_);
        glClear(GL_DEPTH_BUFFER_BIT);
        
        // Ordinary Obj.s
        if (depthShader_) {
            depthShader_->use();
            depthShader_->setMat4("lightSpaceMatrix", lightSpaceMatrix);
            depthShader_->setMat4("model", model);

            depthShader_->setVec4("clipPlane1", glm::vec4(0.0));
            depthShader_->setBool("enableClip1", false);
            
            glCullFace(GL_FRONT);
            
            auto drawShadowBatch = [&](const std::vector<float>& data) {
                if (data.empty()) return;
                glBindVertexArray(vao_);
                glBindBuffer(GL_ARRAY_BUFFER, vbo_);
                glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
                glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(data.size() / 11));
            };
            
            drawShadowBatch(floorVertexData_);
            drawShadowBatch(wallVertexData_);
            drawShadowBatch(boxVertexData_);

            for (const auto& obj : loadedObjs) {
                depthShader_->setMat4("model", obj.model);
                drawShadowBatch(*(obj.vertexData));
            }
            depthShader_->setMat4("model", model);

            // Obj. through portal
            if (objThroughPortalData.exists && !objThroughPortalData.vertexData.empty() && !objThroughPortalData.isPlayer) {
                // Enable clipping plane 1
                glEnable(GL_CLIP_DISTANCE1);

                if (!objThroughPortalData.renderTwice) {
                    depthShader_->setVec4("clipPlane1", objThroughPortalData.portal->getPortalClippingPlane());
                    depthShader_->setBool("enableClip1", true);
                    drawShadowBatch(objThroughPortalData.vertexData);
                }
                else {
                    depthShader_->setVec4("clipPlane1", objThroughPortalData.portal->getPortalClippingPlane());
                    depthShader_->setBool("enableClip1", true);

                    glBindVertexArray(vao_);
                    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
                    glBufferData(GL_ARRAY_BUFFER, objThroughPortalData.vertexData.size() * sizeof(float), objThroughPortalData.vertexData.data(), GL_DYNAMIC_DRAW);
                    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>((objThroughPortalData.vertexData.size() / 11) / 2));

                    depthShader_->setVec4("clipPlane1", objThroughPortalData.portal->getPairPortalClippingPlane());

                    glBindVertexArray(vao_);
                    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
                    glBufferData(GL_ARRAY_BUFFER, objThroughPortalData.vertexData.size() * sizeof(float), objThroughPortalData.vertexData.data(), GL_DYNAMIC_DRAW);
                    glDrawArrays(GL_TRIANGLES, static_cast<GLsizei>((objThroughPortalData.vertexData.size() / 11) / 2), static_cast<GLsizei>((objThroughPortalData.vertexData.size() / 11) / 2));
                }
                    
                // Disable clipping plane 1
                glDisable(GL_CLIP_DISTANCE1);
            }
            
            
            glCullFace(GL_BACK);
            glUseProgram(0);
        }

        // Soft box (player)
        if (softDepthShader_ && (!softVertexData_.empty() || (objThroughPortalData.exists && objThroughPortalData.isPlayer && !objThroughPortalData.vertexData.empty()))) {
            softDepthShader_->use();
            softDepthShader_->setMat4("lightSpaceMatrix", lightSpaceMatrix);

            softDepthShader_->setVec2("direction", moveDirection_);
            softDepthShader_->setFloat("moveT", moveT);

            // 待机时间（周期 idleDuration_）
            float idleT = 0.0f;
            if (idleDuration_ > 1e-5f) {
                float now = static_cast<float>(glfwGetTime());
                idleT = std::fmod(now, idleDuration_) / idleDuration_;
            }
            softDepthShader_->setFloat("idleT", idleT);

            glCullFace(GL_FRONT);

            if (!(objThroughPortalData.exists && objThroughPortalData.isPlayer)) {
                glBindVertexArray(vaoSoft_);
                glBindBuffer(GL_ARRAY_BUFFER, vboSoft_);
                glBufferData(GL_ARRAY_BUFFER, softVertexData_.size() * sizeof(float), softVertexData_.data(), GL_DYNAMIC_DRAW);
                glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(softVertexData_.size() / 11));
            }
            else {
                // Player is going through portal
                // Enable clipping plane 1
                glEnable(GL_CLIP_DISTANCE1);

                if (!objThroughPortalData.renderTwice) {
                    softDepthShader_->setVec4("clipPlane1", objThroughPortalData.portal->getPortalClippingPlane());
                    softDepthShader_->setBool("enableClip1", true);

                    glBindVertexArray(vaoSoft_);
                    glBindBuffer(GL_ARRAY_BUFFER, vboSoft_);
                    glBufferData(GL_ARRAY_BUFFER, objThroughPortalData.vertexData.size() * sizeof(float), objThroughPortalData.vertexData.data(), GL_DYNAMIC_DRAW);
                    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(objThroughPortalData.vertexData.size() / 11));
                }
                else {
                    softDepthShader_->setVec4("clipPlane1", objThroughPortalData.portal->getPortalClippingPlane());
                    softDepthShader_->setBool("enableClip1", true);

                    glBindVertexArray(vaoSoft_);
                    glBindBuffer(GL_ARRAY_BUFFER, vboSoft_);
                    glBufferData(GL_ARRAY_BUFFER, objThroughPortalData.vertexData.size() * sizeof(float), objThroughPortalData.vertexData.data(), GL_DYNAMIC_DRAW);
                    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>((objThroughPortalData.vertexData.size() / 11) / 2));

                    softDepthShader_->setVec2("direction", moveDirectionEnd_);
                    softDepthShader_->setVec4("clipPlane1", objThroughPortalData.portal->getPairPortalClippingPlane());

                    glBindVertexArray(vaoSoft_);
                    glBindBuffer(GL_ARRAY_BUFFER, vboSoft_);
                    glBufferData(GL_ARRAY_BUFFER, objThroughPortalData.vertexData.size() * sizeof(float), objThroughPortalData.vertexData.data(), GL_DYNAMIC_DRAW);
                    glDrawArrays(GL_TRIANGLES, static_cast<GLsizei>((objThroughPortalData.vertexData.size() / 11) / 2), static_cast<GLsizei>((objThroughPortalData.vertexData.size() / 11) / 2));
                }

                // Disable clipping plane 1
                glDisable(GL_CLIP_DISTANCE1);
            }

            glCullFace(GL_BACK);
            glUseProgram(0);
        }
        
        // 恢复之前的 FBO 和 Viewport
        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
        glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
        
        // 清除深度缓冲以便进行主渲染（颜色缓冲已在外部清除）
        glClear(GL_DEPTH_BUFFER_BIT);

        // 2. Skybox Pass (天空盒渲染)
        if (skybox_ && skyboxShader_) {
            glDepthFunc(GL_LEQUAL);
            skyboxShader_->use();
            glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(viewSkyboxToUse));
            skyboxShader_->setMat4("view", viewNoTranslation);
            skyboxShader_->setMat4("projection", projection_);
            skybox_->Draw(*skyboxShader_);
            glUseProgram(0);
            glDepthFunc(GL_LESS);
        }

        // 3. PBR Lighting Pass (场景物体渲染：地板、墙壁、箱子)
        if (shader_) {
            // Manage Clipping Distance 0
            if (enableClip) {
                glEnable(GL_CLIP_DISTANCE0);
            }

            shader_->use();
            
            // Lighting Uniforms
            lightingSystem_->setupCeilingLights(room.size, wallHeight_, tileWorldSize_);
            const auto& pointLights = lightingSystem_->getPointLights();
            shader_->setInt("numPointLights", static_cast<int>(pointLights.size()));
            for (size_t i = 0; i < pointLights.size(); ++i) {
                std::string idx = std::to_string(i);
                shader_->setVec3("pointLightPositions[" + idx + "]", pointLights[i].position);
                shader_->setVec3("pointLightColors[" + idx + "]", pointLights[i].color);
                shader_->setFloat("pointLightIntensities[" + idx + "]", pointLights[i].intensity);
                shader_->setFloat("pointLightRadii[" + idx + "]", pointLights[i].radius);
            }

            const auto& mainLight = lightingSystem_->getMainLight();
            shader_->setVec3("lightDir", mainLight.direction);
            shader_->setVec3("lightColor", mainLight.color);
            shader_->setFloat("lightIntensity", mainLight.intensity);
            shader_->setVec3("ambientLight", lightingSystem_->getAmbientLight());
            shader_->setVec3("viewPos", cameraPosition_);

            shader_->setFloat("metallic", 0.0f);
            shader_->setFloat("roughness", 0.5f);
            shader_->setFloat("ao", 1.0f);
            shader_->setMat4("model", model);
            shader_->setMat4("view", viewToUse);
            shader_->setMat4("projection", projection_);
            shader_->setMat4("lightSpaceMatrix", lightSpaceMatrix);

            shader_->setVec4("clipPlane0", clipPlane);
            shader_->setBool("enableClip0", enableClip);
            shader_->setVec4("clipPlane1", glm::vec4(0.0));
            shader_->setBool("enableClip1", false);
            
            // Shadow Map
            shader_->setInt("shadowMap", 1);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, depthMap_);
            
            // Texture settings
            shader_->setInt("albedoMap", 0);
            shader_->setInt("useTexture", 1);

            auto drawPBRBatch = [&](const std::vector<float>& data, GLuint tex) {
                if (data.empty()) return;
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tex);
                glBindVertexArray(vao_);
                glBindBuffer(GL_ARRAY_BUFFER, vbo_);
                glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
                glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(data.size() / 11));
            };

            drawPBRBatch(floorVertexData_, tileTex_);
            drawPBRBatch(wallVertexData_, wallTex_);
            drawPBRBatch(boxVertexData_, boxTex_);

            for (const auto& obj : loadedObjs) {
                shader_->setMat4("model", obj.model);
                drawPBRBatch(*(obj.vertexData), boxTex_);
            }
            shader_->setMat4("model", model);

            // Draw obj. going through portal
            if (objThroughPortalData.exists && !objThroughPortalData.isPlayer && !objThroughPortalData.vertexData.empty()) {
                // Enable clipping plane 1
                glEnable(GL_CLIP_DISTANCE1);

                if (!objThroughPortalData.renderTwice) {
                    // Draw only once
                    shader_->setVec4("clipPlane1", objThroughPortalData.portal->getPortalClippingPlane());
                    shader_->setBool("enableClip1", true);

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, boxTex_);
                    glBindVertexArray(vao_);
                    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
                    glBufferData(GL_ARRAY_BUFFER, objThroughPortalData.vertexData.size() * sizeof(float), objThroughPortalData.vertexData.data(), GL_DYNAMIC_DRAW);
                    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(objThroughPortalData.vertexData.size() / 11));
                    glBindBuffer(GL_ARRAY_BUFFER, 0);
                    glBindVertexArray(0);
                }
                else {
                    // Draw twice, remember to change the clipping plane & move direction!
                    shader_->setVec4("clipPlane1", objThroughPortalData.portal->getPortalClippingPlane());
                    shader_->setBool("enableClip1", true);

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, boxTex_);
                    glBindVertexArray(vao_);
                    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
                    glBufferData(GL_ARRAY_BUFFER, objThroughPortalData.vertexData.size() * sizeof(float), objThroughPortalData.vertexData.data(), GL_DYNAMIC_DRAW);
                    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>((objThroughPortalData.vertexData.size() / 11) / 2));
                    glBindBuffer(GL_ARRAY_BUFFER, 0);
                    glBindVertexArray(0);

                    shader_->setVec4("clipPlane1", objThroughPortalData.portal->getPairPortalClippingPlane());

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, boxTex_);
                    glBindVertexArray(vao_);
                    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
                    glBufferData(GL_ARRAY_BUFFER, objThroughPortalData.vertexData.size() * sizeof(float), objThroughPortalData.vertexData.data(), GL_DYNAMIC_DRAW);
                    glDrawArrays(GL_TRIANGLES, static_cast<GLsizei>((objThroughPortalData.vertexData.size() / 11) / 2), static_cast<GLsizei>((objThroughPortalData.vertexData.size() / 11) / 2));
                    glBindBuffer(GL_ARRAY_BUFFER, 0);
                    glBindVertexArray(0);
                }

                // Disable clipping plane 1
                glDisable(GL_CLIP_DISTANCE1);
            }
            
            glUseProgram(0);

            // Manage Clipping Distance 0
            if (enableClip) {
                glDisable(GL_CLIP_DISTANCE0);
            }
        }

        
        // 4. SoftCube (Player) Pass (玩家软体渲染)
        // 绘制角色（软体立方体 + 着色器形变）
        // Considers whether the player is going through a portal or not
        if ((!softVertexData_.empty() || (objThroughPortalData.exists && objThroughPortalData.isPlayer && !objThroughPortalData.vertexData.empty())) && softcubeShader_) {
            // Manage Clipping Distance 0
            if (enableClip) {
                glEnable(GL_CLIP_DISTANCE0);
            }
            
            softcubeShader_->use();
            softcubeShader_->setMat4("view", viewToUse);
            softcubeShader_->setMat4("projection", projection_);
            softcubeShader_->setVec2("direction", moveDirection_);
            softcubeShader_->setFloat("moveT", moveT);
            softcubeShader_->setVec4("clipPlane0", clipPlane);
            softcubeShader_->setBool("enableClip0", enableClip);

            // 待机时间（周期 idleDuration_）
            float idleT = 0.0f;
            if (idleDuration_ > 1e-5f) {
                float now = static_cast<float>(glfwGetTime());
                idleT = std::fmod(now, idleDuration_) / idleDuration_;
            }
            softcubeShader_->setFloat("idleT", idleT);

            // PBR Uniforms
            softcubeShader_->setMat4("lightSpaceMatrix", lightSpaceMatrix);
            softcubeShader_->setMat4("model", glm::mat4(1.0f));

            const auto& pointLights = lightingSystem_->getPointLights();
            softcubeShader_->setInt("numPointLights", static_cast<int>(pointLights.size()));
            for (size_t i = 0; i < pointLights.size(); ++i) {
                std::string idx = std::to_string(i);
                softcubeShader_->setVec3("pointLightPositions[" + idx + "]", pointLights[i].position);
                softcubeShader_->setVec3("pointLightColors[" + idx + "]", pointLights[i].color);
                softcubeShader_->setFloat("pointLightIntensities[" + idx + "]", pointLights[i].intensity);
                softcubeShader_->setFloat("pointLightRadii[" + idx + "]", pointLights[i].radius);
            }

            const auto& mainLight = lightingSystem_->getMainLight();
            softcubeShader_->setVec3("lightDir", mainLight.direction);
            softcubeShader_->setVec3("lightColor", mainLight.color);
            softcubeShader_->setFloat("lightIntensity", mainLight.intensity);
            softcubeShader_->setVec3("ambientLight", lightingSystem_->getAmbientLight());
            softcubeShader_->setVec3("viewPos", cameraPosition_);

            softcubeShader_->setFloat("metallic", 0.0f);
            softcubeShader_->setFloat("roughness", 0.5f);
            softcubeShader_->setFloat("ao", 1.0f);
            
            // Shadow Map
            softcubeShader_->setInt("shadowMap", 1);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, depthMap_);
            
            // Texture settings (disable for softcube)
            softcubeShader_->setInt("useTexture", 0);

            if (!(objThroughPortalData.exists && objThroughPortalData.isPlayer)) {
                // Player is not going through portal
                softcubeShader_->setVec4("clipPlane1", glm::vec4(0.0));
                softcubeShader_->setBool("enableClip1", false);

                glBindVertexArray(vaoSoft_);
                glBindBuffer(GL_ARRAY_BUFFER, vboSoft_);
                glBufferData(GL_ARRAY_BUFFER, softVertexData_.size() * sizeof(float), softVertexData_.data(), GL_DYNAMIC_DRAW);
                glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(softVertexData_.size() / 11));
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glBindVertexArray(0);
            }
            else {
                // Player is going through portal
                // Enable clipping plane 1
                glEnable(GL_CLIP_DISTANCE1);

                if (!objThroughPortalData.renderTwice) {
                    // Draw only once
                    softcubeShader_->setVec4("clipPlane1", objThroughPortalData.portal->getPortalClippingPlane());
                    softcubeShader_->setBool("enableClip1", true);

                    glBindVertexArray(vaoSoft_);
                    glBindBuffer(GL_ARRAY_BUFFER, vboSoft_);
                    glBufferData(GL_ARRAY_BUFFER, objThroughPortalData.vertexData.size() * sizeof(float), objThroughPortalData.vertexData.data(), GL_DYNAMIC_DRAW);
                    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(objThroughPortalData.vertexData.size() / 11));
                    glBindBuffer(GL_ARRAY_BUFFER, 0);
                    glBindVertexArray(0);
                }
                else {
                    // Draw twice, remember to change the clipping plane & move direction!
                    softcubeShader_->setVec4("clipPlane1", objThroughPortalData.portal->getPortalClippingPlane());
                    softcubeShader_->setBool("enableClip1", true);

                    glBindVertexArray(vaoSoft_);
                    glBindBuffer(GL_ARRAY_BUFFER, vboSoft_);
                    glBufferData(GL_ARRAY_BUFFER, objThroughPortalData.vertexData.size() * sizeof(float), objThroughPortalData.vertexData.data(), GL_DYNAMIC_DRAW);
                    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>((objThroughPortalData.vertexData.size() / 11) / 2));
                    glBindBuffer(GL_ARRAY_BUFFER, 0);
                    glBindVertexArray(0);

                    softcubeShader_->setVec2("direction", moveDirectionEnd_);
                    softcubeShader_->setVec4("clipPlane1", objThroughPortalData.portal->getPairPortalClippingPlane());

                    glBindVertexArray(vaoSoft_);
                    glBindBuffer(GL_ARRAY_BUFFER, vboSoft_);
                    glBufferData(GL_ARRAY_BUFFER, objThroughPortalData.vertexData.size() * sizeof(float), objThroughPortalData.vertexData.data(), GL_DYNAMIC_DRAW);
                    glDrawArrays(GL_TRIANGLES, static_cast<GLsizei>((objThroughPortalData.vertexData.size() / 11) / 2), static_cast<GLsizei>((objThroughPortalData.vertexData.size() / 11) / 2));
                    glBindBuffer(GL_ARRAY_BUFFER, 0);
                    glBindVertexArray(0);
                }

                // Disable clipping plane 1
                glDisable(GL_CLIP_DISTANCE1);
            }

            glUseProgram(0);

            // Manage Clipping Distance 0
            if (enableClip) {
                glDisable(GL_CLIP_DISTANCE0);
            }
        }
    }

    glm::vec3 getCameraPosition() const {
        return cameraPosition_;
    }

    float appendRoomGeometry(const Room& room, int roomId, const GameState& state, const GameState& next_state, float moveT, float tileSize, bool wallCullOverride = false) {
        const int tileCount = room.size;
        const float boardHalf = tileCount * tileSize * 0.5f;

        std::vector<std::vector<bool>> windowMap(tileCount, std::vector<bool>(tileCount, false));

        playerEyePosition_ = glm::vec3(0.0f, 0.02f, 0.0f);

        // Initialize passing through portal info
        objThroughPortalData.exists = false;
        objThroughPortalData.isPlayer = false;
        objThroughPortalData.renderTwice = false;
        objThroughPortalData.portal = nullptr;

        // Clear vertex vectors
        vertexData_.clear();
        floorVertexData_.clear();
        wallVertexData_.clear();
        boxVertexData_.clear();
        softVertexData_.clear();
        loadedObjs.clear();
        objThroughPortalData.vertexData.clear();

        // 辅助函数：向缓冲区添加顶点数据 (pos3 + normal3 + color3 + tex2)
        auto pushVertex = [](std::vector<float>& buf, const glm::vec3& pos, const glm::vec3& normal, const glm::vec3& color, const glm::vec2& tex) {
            buf.push_back(pos.x);
            buf.push_back(pos.y);
            buf.push_back(pos.z);
            buf.push_back(normal.x);
            buf.push_back(normal.y);
            buf.push_back(normal.z);
            buf.push_back(color.r);
            buf.push_back(color.g);
            buf.push_back(color.b);
            buf.push_back(tex.x);
            buf.push_back(tex.y);
        };

        auto pushQuad = [&](std::vector<float>& buf, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec3& color) {
            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;
            glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

            pushVertex(buf, v0, normal, color, glm::vec2(0.0f, 0.0f));
            pushVertex(buf, v1, normal, color, glm::vec2(1.0f, 0.0f));
            pushVertex(buf, v2, normal, color, glm::vec2(1.0f, 1.0f));
            pushVertex(buf, v0, normal, color, glm::vec2(0.0f, 0.0f));
            pushVertex(buf, v2, normal, color, glm::vec2(1.0f, 1.0f));
            pushVertex(buf, v3, normal, color, glm::vec2(0.0f, 1.0f));
        };

        // 辅助函数：向软体缓冲区添加顶点数据 (pos3 + color3 + normal2 + tex3)
        auto pushSoftVertex = [&](const glm::vec3& pos, const glm::vec3& color, const glm::vec2& n2, const glm::vec3& tex, bool throughPortal = false) {
            std::vector<float>& target = throughPortal ? objThroughPortalData.vertexData : softVertexData_;
            
            target.push_back(pos.x);
            target.push_back(pos.y);
            target.push_back(pos.z);
            target.push_back(color.r);
            target.push_back(color.g);
            target.push_back(color.b);
            target.push_back(n2.x);
            target.push_back(n2.y);
            target.push_back(tex.x);
            target.push_back(tex.y);
            target.push_back(tex.z);
        };

        auto pushSoftQuad = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3,
                                const glm::vec3& color, const glm::vec2& n2, float cx, float cz, float halfW, bool throughPortal = false) {
            // aTex.xy = 相对中心归一化偏移，aTex.z = halfW
            auto makeTex = [&](const glm::vec3& p) -> glm::vec3 {
                float xNorm = (halfW > 1e-6f) ? (p.x - cx) / halfW : 0.0f;
                float zNorm = (halfW > 1e-6f) ? (p.z - cz) / halfW : 0.0f;
                return glm::vec3(xNorm, zNorm, halfW);
            };
            pushSoftVertex(v0, color, n2, makeTex(v0), throughPortal);
            pushSoftVertex(v1, color, n2, makeTex(v1), throughPortal);
            pushSoftVertex(v2, color, n2, makeTex(v2), throughPortal);
            pushSoftVertex(v0, color, n2, makeTex(v0), throughPortal);
            pushSoftVertex(v2, color, n2, makeTex(v2), throughPortal);
            pushSoftVertex(v3, color, n2, makeTex(v3), throughPortal);
        };

        auto appendFloor = [&](std::vector<float>& buf, float minX, float maxX, float minZ, float maxZ, const glm::vec3& color) {
            glm::vec3 v0(minX, 0.0f, minZ);
            glm::vec3 v1(maxX, 0.0f, minZ);
            glm::vec3 v2(maxX, 0.0f, maxZ);
            glm::vec3 v3(minX, 0.0f, maxZ);
            pushQuad(buf, v0, v3, v2, v1, color);
        };

        auto appendWallPart = [&](std::vector<float>& buf, float minX, float maxX, float minZ, float maxZ, float minY, float maxY, const glm::vec3& color) {
            glm::vec3 topColor = color;
            glm::vec3 sideColor = color * 0.85f;
            glm::vec3 top0(minX, maxY, minZ);
            glm::vec3 top1(maxX, maxY, minZ);
            glm::vec3 top2(maxX, maxY, maxZ);
            glm::vec3 top3(minX, maxY, maxZ);
            // 顶部面：原顺序导致法线朝下，改为逆时针(top0->top3->top2->top1)使其朝上
            pushQuad(buf, top0, top3, top2, top1, topColor);

            glm::vec3 bottom0(minX, minY, minZ);
            glm::vec3 bottom1(maxX, minY, minZ);
            glm::vec3 bottom2(maxX, minY, maxZ);
            glm::vec3 bottom3(minX, minY, maxZ);
            // 侧面1 (minZ)：原顺序法线朝内(+Z)，改为(bottom1->bottom0->top0->top1)使其朝外(-Z)
            pushQuad(buf, bottom1, bottom0, top0, top1, sideColor);
            // 侧面2 (maxX)：原顺序法线朝内(-X)，改为(bottom2->bottom1->top1->top2)使其朝外(+X)
            pushQuad(buf, bottom2, bottom1, top1, top2, sideColor);
            pushQuad(buf, bottom3, bottom2, top2, top3, sideColor);
            pushQuad(buf, bottom0, bottom3, top3, top0, sideColor);
        };

        auto appendWindowWall = [&](std::vector<float>& buf, int gx, int gy, float minX, float maxX, float minZ, float maxZ, float minY, float maxY, const glm::vec3& color) {
            float h = maxY - minY;
            float w = maxX - minX;
            float d = maxZ - minZ;
            
            float winBottom = minY + h * 0.3f;
            float winTop = minY + h * 0.7f;
            float pillarRatio = 0.2f;
            float pW = w * pillarRatio;
            float pD = d * pillarRatio;

            // Bottom
            appendWallPart(buf, minX, maxX, minZ, maxZ, minY, winBottom, color);
            // Top
            appendWallPart(buf, minX, maxX, minZ, maxZ, winTop, maxY, color);
            
            // Pillars
            if((gx == 0 || (gx > 0 && windowMap[gy][gx - 1] != true)) && (gy == 0 || (gy > 0 && windowMap[gy - 1][gx] != true)))
                appendWallPart(buf, minX, minX + pW, minZ, minZ + pD, winBottom, winTop, color);
            if((gx == tileCount - 1 || (gx < tileCount - 1 && windowMap[gy][gx + 1] != true)) && (gy == 0 || (gy > 0 && windowMap[gy - 1][gx] != true)))
                appendWallPart(buf, maxX - pW, maxX, minZ, minZ + pD, winBottom, winTop, color);
            if((gx == 0 || (gx > 0 && windowMap[gy][gx - 1] != true)) && (gy == tileCount - 1 || (gy < tileCount - 1 && windowMap[gy + 1][gx] != true)))
                appendWallPart(buf, minX, minX + pW, maxZ - pD, maxZ, winBottom, winTop, color);
            if((gx == tileCount - 1 || (gx < tileCount - 1 && windowMap[gy][gx + 1] != true)) && (gy == tileCount - 1 || (gy < tileCount - 1 && windowMap[gy + 1][gx] != true)))
                appendWallPart(buf, maxX - pW, maxX, maxZ - pD, maxZ, winBottom, winTop, color);
        };

        auto appendBoxColumn = [&](std::vector<float>& buf, float minX, float maxX, float minZ, float maxZ, float minY, float maxY, const glm::vec3& color) {
            glm::vec3 topColor = color;
            glm::vec3 sideColor = color * 0.85f;
            glm::vec3 top0(minX, maxY, minZ);
            glm::vec3 top1(maxX, maxY, minZ);
            glm::vec3 top2(maxX, maxY, maxZ);
            glm::vec3 top3(minX, maxY, maxZ);
            pushQuad(buf, top0, top1, top2, top3, topColor);

            glm::vec3 bottom0(minX, minY, minZ);
            glm::vec3 bottom1(maxX, minY, minZ);
            glm::vec3 bottom2(maxX, minY, maxZ);
            glm::vec3 bottom3(minX, minY, maxZ);
            pushQuad(buf, bottom0, bottom1, top1, top0, sideColor);
            pushQuad(buf, bottom1, bottom2, top2, top1, sideColor);
            pushQuad(buf, bottom2, bottom3, top3, top2, sideColor);
            pushQuad(buf, bottom3, bottom0, top0, top3, sideColor);
        };

        auto boundsForCell = [&](int gridX, int gridY) {
            float minX = -boardHalf + gridX * tileSize;
            float maxX = minX + tileSize;
            float maxZ = boardHalf - gridY * tileSize;
            float minZ = maxZ - tileSize;
            return std::array<float, 4>{minX, maxX, minZ, maxZ};
        };

        auto appendChair = [&](int gx, int gy) {
            if (rawChairData_.empty()) return;
            auto bounds = boundsForCell(gx, gy);
            float centerX = (bounds[0] + bounds[1]) * 0.5f;
            float centerZ = (bounds[2] + bounds[3]) * 0.5f;
            float bottomY = 0.0f;

            LoadedObjData obj;
            obj.model = glm::translate(
                glm::mat4(1.0f),
                glm::vec3(centerX, bottomY, centerZ)
            );

            obj.vertexData = &rawChairData_; // 共享
            loadedObjs.push_back(obj);
        };

        // CPU 端软体立方体：不做待机形变，只输出细分网格与 aNormal/aTex
        auto appendSoftCube = [&](float minX, float maxX, float minZ, float maxZ, float minY, float maxY, 
            const glm::vec3& color, float /*amp*/, float /*timeT*/, bool throughPortal = false) {
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
                        } else if (axis == 0) { // 侧X (y,z)
                            p00 = glm::vec3(fixed, uu0, vv0);
                            p10 = glm::vec3(fixed, uu1, vv0);
                            p11 = glm::vec3(fixed, uu1, vv1);
                            p01 = glm::vec3(fixed, uu0, vv1);
                        } else { // 侧Z (x,y)
                            p00 = glm::vec3(uu0, vv0, fixed);
                            p10 = glm::vec3(uu1, vv0, fixed);
                            p11 = glm::vec3(uu1, vv1, fixed);
                            p01 = glm::vec3(uu0, vv1, fixed);
                        }

                        // 计算纹理坐标（用于软体变形）
                        auto makeTex = [&](const glm::vec3& p) -> glm::vec3 {
                            float xNorm = (halfW > 1e-6f) ? (p.x - cx) / halfW : 0.0f;
                            float zNorm = (halfW > 1e-6f) ? (p.z - cz) / halfW : 0.0f;
                            return glm::vec3(xNorm, zNorm, halfW);
                        };

                        pushSoftQuad(p00, p10, p11, p01, faceColor, n2, cx, cz, halfW, throughPortal);
                    }
                }
            };

            // 顶/底：aNormal = (0,0)
            emitFaceGrid(1, maxY, minX, maxX, minZ, maxZ, topColor,   glm::vec2(0.0f, 0.0f));
            emitFaceGrid(1, minY, minX, maxX, minZ, maxZ, bottomColor,glm::vec2(0.0f, 0.0f));
            // 侧X：aNormal = (±1,0)
            emitFaceGrid(0, minX, minY, maxY, minZ, maxZ, sideColor,  glm::vec2(-1.0f, 0.0f));
            emitFaceGrid(0, maxX, minY, maxY, minZ, maxZ, sideColor,  glm::vec2( 1.0f, 0.0f));
            // 侧Z：aNormal = (0,±1)
            emitFaceGrid(2, minZ, minX, maxX, minY, maxY, sideColor,  glm::vec2(0.0f,-1.0f));
            emitFaceGrid(2, maxZ, minX, maxX, minY, maxY, sideColor,  glm::vec2(0.0f, 1.0f));
        };

        auto centerForCell = [&](int gridX, int gridY) -> glm::vec2 {
            auto b = boundsForCell(gridX, gridY);
            return glm::vec2((b[0] + b[1]) * 0.5f, (b[2] + b[3]) * 0.5f); // x, z
        };

        auto drawAtCenter = [&](const glm::vec2& centerXZ, const glm::vec3& color, float height, bool throughPortal = false) {
            std::vector<float>& target = throughPortal ? objThroughPortalData.vertexData : boxVertexData_;
            
            const float inset = tileSize * 0.02f;
            const float halfW = (tileSize * 0.5f) - inset;
            float minX = centerXZ.x - halfW;
            float maxX = centerXZ.x + halfW;
            float minZ = centerXZ.y - halfW;
            float maxZ = centerXZ.y + halfW;
            appendBoxColumn(target, minX, maxX, minZ, maxZ, 0.02f, 0.02f + height, color);
        };   

        // 判定某墙格到外部的直线路径上是否全为墙，用于让窗洞贯穿厚墙
        auto isWallCell = [&](int gx, int gy) -> bool {
            if (gx < 0 || gx >= tileCount || gy < 0 || gy >= tileCount) return false;
            return room.scene[gy][gx] == "#";
        };
        
        auto hasExteriorWallPath = [&](int gx, int gy) -> bool {
            if (!isWallCell(gx, gy)) return false;
            auto pathClear = [&](int dx, int dy, int max) -> bool {
                int x = gx, y = gy, i = 0;
                if(isWallCell(gx + dx, gx + dy) == isWallCell(gx - dx, gy - dy))return false;
                while (x >= 0 && x < tileCount && y >= 0 && y < tileCount ) {
                    if (!isWallCell(x, y)) return false;
					if (i >= max) return false;
                    if (x == 0 || x == tileCount - 1 || y == 0 || y == tileCount - 1) return true;
                    x += dx;
                    y += dy;
                    i++;
                }
                return false;
            };
            return pathClear(-1, 0, 2) || pathClear(1, 0, 2) || pathClear(0, -1, 2) || pathClear(0, 1, 2);
        };

        // Pre-calculate window positions based on continuous wall segments
        std::vector<std::vector<bool>> isCandidate(tileCount, std::vector<bool>(tileCount, false));

        for (int y = 0; y < tileCount; ++y) {
            for (int x = 0; x < tileCount; ++x) {
                isCandidate[y][x] = hasExteriorWallPath(x, y);
            }
        }

        // Horizontal scan
        int start, end;
        for (int y = 0; y < tileCount; y += tileCount - 1) {
            start = -1, end = -1;
            for (int x = 0; x <= tileCount; ++x) {
                bool cand = (x < tileCount) && isCandidate[y][x];
                if (cand && start == -1) {
                    start = x;
                    end = start;
                } 
                else if (cand) {
                    end = x;
                }
                else if (!cand && start != -1 && end - start > 6) {
                    for (int i = start + 2; i < end - 3; i++) {
                        if (y == 0) {
                            windowMap[y][i] = isCandidate[y + 1][i];
                            windowMap[y + 1][i] = isCandidate[y + 1][i];
                        }
                        else {
                            windowMap[y][i] = isCandidate[y - 1][i];
                            windowMap[y - 1][i] = isCandidate[y - 1][i];
                        }
                    }
                }
            }
        }

        // Vertical scan
        for (int x = 0; x < tileCount; x += tileCount - 1) {
            start = -1, end = -1;
            for (int y = 0; y <= tileCount; ++y) {
                bool cand = (y < tileCount) && isCandidate[y][x];
                if (cand && start == -1) {
                    start = y;
                    end = y;
                }
                else if (cand) {
                    end = y;
                }
                else if (!cand && start != -1 && end - start > 6) {
                    for (int i = start + 2; i < end - 3; i++) {
                        if (x == 0) {
                            windowMap[i][x] = isCandidate[i][x + 1];
                            windowMap[i][x + 1] = isCandidate[i][x + 1];
                        }
                        else {
                            windowMap[i][x] = isCandidate[i][x - 1];
                            windowMap[i][x - 1] = isCandidate[i][x - 1];
                        }
                    }
                }
            }
        }

        auto drawPlayerAtCenter = [&](const glm::vec2& centerXZ, const glm::vec3& color, float height, float idleT, bool throughPortal = false) {
            const float inset = tileSize * 0.10f;
            const float halfW = (tileSize * 0.5f) - inset;
            float minX = centerXZ.x - halfW;
            float maxX = centerXZ.x + halfW;
            float minZ = centerXZ.y - halfW;
            float maxZ = centerXZ.y + halfW;
            appendSoftCube(minX, maxX, minZ, maxZ, 0.02f, 0.02f + height, color, 0.05f, idleT, throughPortal);
        };

        auto camera_in_wall = [&](float wallMinX, float wallMaxX, float wallMinZ, float wallMaxZ) {
            if (wallCullOverride) {
                return false;
            }
            
            float camZ = cameraPosition_.z, camX = cameraPosition_.x;
			float camerayaw = cameraYaw_ >= 0 ? cameraYaw_ : cameraYaw_ + 360.0f;
            if ( wallMinX <= camX && camX <= wallMaxX &&
				wallMinZ <= camZ && camZ <= wallMaxZ) {
                return true;
            }
            if (camerayaw <= 45.0f || camerayaw >= 315.0f) {
                return ((( wallMaxX <= camX ) || ( wallMinX - 1.0f <= camX )) && ( wallMinZ <= camZ + 1.0f && wallMaxZ >= camZ -1.0f ));
            } else if (camerayaw > 45.0f && camerayaw <= 135.0f) {
                return ((( wallMaxZ <= camZ ) || ( wallMinZ - 1.0f <= camZ )) && (wallMinX <= camX + 1.0f && wallMaxX >= camX - 1.0f));
            } else if (camerayaw > 135.0f && camerayaw <= 225.0f) {
                return (((wallMinX >= camX) || (wallMaxX + 1.0f >= camX)) && (wallMinZ <= camZ + 1.0f && wallMaxZ >= camZ - 1.0f));
            } else {
                return (((wallMinZ >= camZ) || (wallMaxZ + 1.0f >= camZ)) && (wallMinX <= camX + 1.0f && wallMaxX >= camX - 1.0f));
			}
		};

        //std::cout << cameraYaw_ << std::endl;
        for (int y = 0; y < tileCount; ++y) {
            for (int x = 0; x < tileCount; ++x) {
                auto bounds = boundsForCell(x, y);
                glm::vec3 baseColor = tileColorForCell(room.scene[y][x]);
                appendFloor(floorVertexData_, bounds[0], bounds[1], bounds[2], bounds[3], baseColor);
                glm::vec3 camPos = cameraPosition_;

                if (room.scene[y][x] == "#") {
                    if (windowMap[y][x] && !camera_in_wall(bounds[0], bounds[1], bounds[2], bounds[3])) {
                        appendWindowWall(wallVertexData_, x, y, bounds[0], bounds[1], bounds[2], bounds[3], 0.0f, wallHeight_, glm::vec3(RGB_2_FLT(0xF4CFE9)));
                    } else {
                        if (!camera_in_wall(bounds[0], bounds[1], bounds[2], bounds[3]))
                            appendWallPart(wallVertexData_, bounds[0], bounds[1], bounds[2], bounds[3], 0.0f, wallHeight_, glm::vec3(RGB_2_FLT(0xF4CFE9)));
                        else {
                            appendWallPart(wallVertexData_, bounds[0], bounds[1], bounds[2], bounds[3], 0.0f, 1.0f, glm::vec3(RGB_2_FLT(0xF4CFE9)));
                        }
                    }
                } else if (room.scene[y][x] == "|") {
                    appendChair(x, y);
                }
            }
        }

        // Determine whether the obj. is going through a portal
        auto isPassingThroughPortal = [&](const Pos& s, const Pos& e) {
            if (!moving_ || (s.room != roomId && e.room != roomId)) {
                // Player not moving, or neither end is in cur. room
                return false;
            }
            else if (s.room != e.room) {
                // Ends not in the same room, certainly going through a portal
                return true;
            }
            else {
                // Ends in the same room, but might be going through a portal
                // Determine by judging whether movement is continuous, or not moving at all
                int dx = e.x - s.x;
                int dy = e.y - s.y;
                if ((moveInput_ == UP && dx == 0 && dy == -1) ||
                    (moveInput_ == DOWN && dx == 0 && dy == 1) ||
                    (moveInput_ == LEFT && dx == -1 && dy == 0) ||
                    (moveInput_ == RIGHT && dx == 1 && dy == 0) ||
                    (dx == 0 && dy == 0))
                {
                    return false;
                }
                else {
                    return true;
                }
            }
        };

        auto getPassingThroughPortal = [&](const Pos& curPos) {
            Portal* target = nullptr;

            int dx = 0, dy = 0;
            switch (moveInput_) {
                case UP: dy = -1; break;
                case DOWN: dy = 1; break;
                case LEFT: dx = -1; break;
                case RIGHT: dx = 1; break;
            }

            for (const auto& portal : portalsList) {
                if (portal->getPortalPos(state.boxrooms) == curPos) {
                    // Currently at entry, only 1 portal per entry
                    target = portal;
                    break;
                }
                else if (Pos(curPos.room, curPos.x + dx, curPos.y + dy) == portal->getPortalPos(state.boxrooms) &&
                        ((moveInput_ == UP && portal->relativePos == ZNeg) ||
                         (moveInput_ == DOWN && portal->relativePos == ZPos) ||
                         (moveInput_ == LEFT && portal->relativePos == XPos) ||
                         (moveInput_ == RIGHT && portal->relativePos == XNeg)))
                {
                    target = portal;
                    break;
                }
            }

            return target;
        };

        // 计算玩家插值 + 设置运动方向
        auto drawPlayer = [&]() {
            const Pos& s = state.player;
            const Pos& e = next_state.player;
            glm::vec3 color(RGB_2_FLT(0x008B45));
            float height = 0.90f;

            // 待机动画时间（周期由 idleDuration_ 控制）
            float idleT = 0.0f;
            if (idleDuration_ > 1e-5f) {
                float now = static_cast<float>(glfwGetTime());
                idleT = std::fmod(now, idleDuration_) / idleDuration_;
            }

            if (isPassingThroughPortal(s, e)) {
                Portal* sPortal = getPassingThroughPortal(s);

                // Store relevant info
                objThroughPortalData.exists = true;
                objThroughPortalData.isPlayer = true;
                objThroughPortalData.portal = (s.room == roomId) ? sPortal : sPortal->pairPortal;

                // If the box is being teleported to the same room, draw twice
                        // The render function will handle drawing the two obj.s
                if (s.room == e.room) {
                    objThroughPortalData.renderTwice = true;
                }

                if (s.room == roomId) {
                    // Calculate "supposedly" position and draw the box
                    int dxs = 0, dys = 0;
                    switch (moveInput_) {
                    case UP: dys = -1; break;
                    case DOWN: dys = 1; break;
                    case LEFT: dxs = -1; break;
                    case RIGHT: dxs = 1; break;
                    }

                    glm::vec2 csNow = centerForCell(s.x, s.y);
                    glm::vec2 csNext = centerForCell(s.x + dxs, s.y + dys);
                    glm::vec2 cs = csNow + (csNext - csNow) * moveT;

                    glm::vec2 dirs = csNext - csNow;
                    if (glm::dot(dirs, dirs) > 1e-6f) dirs = glm::normalize(dirs);
                    else dirs = glm::vec2(0.0f);
                    moveDirection_ = dirs; // 供 softcubeShader_ 使用

                    drawPlayerAtCenter(cs, color, height, idleT, true);
                }

                // If the player is being teleported to the same room, draw twice
                if (e.room == roomId) {
                    // Calculate "supposedly" position before exiting pairPortal (of sPortal) and draw the box
                    int dxe = 0, dye = 0;
                    switch (objThroughPortalData.portal->pairPortal->relativePos) {
                        case XPos: dxe = sPortal->pairPortal->stationary ? 1 : -1; break;
                        case XNeg: dxe = sPortal->pairPortal->stationary ? -1 : 1; break;
                        case ZPos: dye = sPortal->pairPortal->stationary ? -1 : 1; break;
                        case ZNeg: dye = sPortal->pairPortal->stationary ? 1 : -1; break;
                    }

                    glm::vec2 ceNow = centerForCell(e.x + dxe, e.y + dye);
                    glm::vec2 ceNext = centerForCell(e.x, e.y);
                    glm::vec2 ce = ceNow + (ceNext - ceNow) * moveT;

                    glm::vec2 dire = ceNext - ceNow;
                    if (glm::dot(dire, dire) > 1e-6f) dire = glm::normalize(dire);
                    else dire = glm::vec2(0.0f);

                    if (objThroughPortalData.renderTwice) {
                        moveDirectionEnd_ = dire; // 供 softcubeShader_ 使用
                    }
                    else {
                        // Only rendered once, use moveDirection_
                        moveDirection_ = dire; // 供 softcubeShader_ 使用
                    }

                    drawPlayerAtCenter(ce, color, height, idleT, true);
                }
            }
            else if (s.room == roomId && e.room == roomId && (s.x != e.x || s.y != e.y) && moving_) {
                glm::vec2 cs = centerForCell(s.x, s.y);
                glm::vec2 ce = centerForCell(e.x, e.y);
                glm::vec2 dir = ce - cs;
                if (glm::dot(dir, dir) > 1e-6f) dir = glm::normalize(dir);
                else dir = glm::vec2(0.0f);
                moveDirection_ = dir; // 供 softcubeShader_ 使用

                glm::vec2 c = cs + (ce - cs) * moveT;
                drawPlayerAtCenter(c, color, height, idleT);
            } 
            else if (s.room == roomId && (!moving_ || (s.room != e.room || (s.x == e.x && s.y == e.y)))) {
                moveDirection_ = glm::vec2(0.0f);
                glm::vec2 c = centerForCell(s.x, s.y);
                drawPlayerAtCenter(c, color, height, idleT);
            }
            else if (!moving_ && e.room == roomId) {
                moveDirection_ = glm::vec2(0.0f);
                glm::vec2 c = centerForCell(e.x, e.y);
                drawPlayerAtCenter(c, color, height, idleT);
            }
        };

        // 计算箱子插值（保持基础几何）
        auto drawBoxes = [&](std::map<int, Pos> boxes, std::map<int, Pos> next_boxes, glm::vec3 color, bool isBoxroom = false) {
            for (const auto& kv : boxes) {
                int id = kv.first;
                const Pos& s = kv.second;
                float height = isBoxroom ? portalHeight : 0.96f;

                auto it = next_boxes.find(id);
                if (it != next_boxes.end()) {
                    const Pos& e = it->second;

                    if (isPassingThroughPortal(s, e)) {
                        Portal* sPortal = getPassingThroughPortal(s);

                        // Store relevant info
                        objThroughPortalData.exists = true;
                        objThroughPortalData.isPlayer = false;
                        objThroughPortalData.portal = (s.room == roomId) ? sPortal : sPortal->pairPortal;
                        
                        // If the box is being teleported to the same room, draw twice
                        // The render function will handle drawing the two obj.s
                        if (s.room == e.room) {
                            objThroughPortalData.renderTwice = true;
                        }

                        if (s.room == roomId) {
                            // Calculate "supposedly" position and draw the box
                            int dxs = 0, dys = 0;
                            switch (moveInput_) {
                            case UP: dys = -1; break;
                            case DOWN: dys = 1; break;
                            case LEFT: dxs = -1; break;
                            case RIGHT: dxs = 1; break;
                            }

                            glm::vec2 csNow = centerForCell(s.x, s.y);
                            glm::vec2 csNext = centerForCell(s.x + dxs, s.y + dys);
                            glm::vec2 cs = csNow + (csNext - csNow) * moveT;

                            drawAtCenter(cs, color, height, true);
                        }

                        if (e.room == roomId) {
                            // Calculate "supposedly" position before exiting pairPortal (of sPortal) and draw the box
                            int dxe = 0, dye = 0;
                            switch (objThroughPortalData.portal->pairPortal->relativePos) {
                                case XPos: dxe = sPortal->pairPortal->stationary ? 1 : -1; break;
                                case XNeg: dxe = sPortal->pairPortal->stationary ? -1 : 1; break;
                                case ZPos: dye = sPortal->pairPortal->stationary ? -1 : 1; break;
                                case ZNeg: dye = sPortal->pairPortal->stationary ? 1 : -1; break;
                            }

                            glm::vec2 ceNow = centerForCell(e.x + dxe, e.y + dye);
                            glm::vec2 ceNext = centerForCell(e.x, e.y);
                            glm::vec2 ce = ceNow + (ceNext - ceNow) * moveT;

                            drawAtCenter(ce, color, height, true);
                        }
                    }
                    else if (s.room == roomId && e.room == roomId && (s.x != e.x || s.y != e.y) && moving_) {
                        glm::vec2 cs = centerForCell(s.x, s.y);
                        glm::vec2 ce = centerForCell(e.x, e.y);
                        glm::vec2 c = cs + (ce - cs) * moveT;
                        drawAtCenter(c, color, height);
                    }
                    else if (s.room == roomId && (!moving_ || (s.room != e.room || (s.x == e.x && s.y == e.y)))) {
                        glm::vec2 c = centerForCell(s.x, s.y);
                        drawAtCenter(c, color, height);
                    }
                    else if (!moving_ && e.room == roomId) {
                        glm::vec2 c = centerForCell(e.x, e.y);
                        drawAtCenter(c, color, height);
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

        drawBoxes(state.boxes, next_state.boxes, glm::vec3(RGB_2_FLT(0xCFD6F4)));
        drawBoxes(state.boxrooms, next_state.boxrooms, glm::vec3(RGB_2_FLT(0xCFF4ED)), true);
        drawPlayer();

        return boardHalf;
    }

    glm::vec3 tileColorForCell(const std::string& cell) const {
        if (cell == "#") return glm::vec3(RGB_2_FLT(0xF4CFE9));
        if (cell == "=") return {0.25f, 0.6f, 0.3f};
        if (cell == "_") return {0.7f, 0.6f, 0.25f};
        return {0.9f, 0.9f, 1.0f};
    }

    void pushVertex(const glm::vec3& pos, const glm::vec3& color) {
        glm::vec3 normal = glm::normalize(pos - glm::vec3(0.0f, 0.0f, 0.0f));
        if (glm::length(normal) < 0.01f) {
            normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        
        vertexData_.push_back(pos.x);
        vertexData_.push_back(pos.y);
        vertexData_.push_back(pos.z);
        vertexData_.push_back(normal.x);
        vertexData_.push_back(normal.y);
        vertexData_.push_back(normal.z);
        vertexData_.push_back(color.r);
        vertexData_.push_back(color.g);
        vertexData_.push_back(color.b);
    }
    
    void pushVertexWithNormal(const glm::vec3& pos, const glm::vec3& normal, const glm::vec3& color) {
        pushVertexWithNormalTo(vertexData_, pos, normal, color);
    }

    void pushVertexWithNormalTo(std::vector<float>& buffer, const glm::vec3& pos, const glm::vec3& normal, const glm::vec3& color) {
        buffer.push_back(pos.x);
        buffer.push_back(pos.y);
        buffer.push_back(pos.z);
        buffer.push_back(normal.x);
        buffer.push_back(normal.y);
        buffer.push_back(normal.z);
        buffer.push_back(color.r);
        buffer.push_back(color.g);
        buffer.push_back(color.b);
    }

    void loadModel(const std::string& path, std::vector<float>& outData, glm::vec3 color) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Failed to open model file: " << path << std::endl;
            return;
        }

        std::vector<glm::vec3> temp_pos;
        std::vector<glm::vec3> temp_norm;
        std::vector<glm::vec2> temp_tex;
        
        struct VertexIdx { int v, vt, vn; };
        std::vector<VertexIdx> indices;

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::stringstream ss(line);
            std::string type;
            ss >> type;

            if (type == "v") {
                glm::vec3 p;
                ss >> p.x >> p.y >> p.z;
                temp_pos.push_back(p);
            } else if (type == "vn") {
                glm::vec3 n;
                ss >> n.x >> n.y >> n.z;
                temp_norm.push_back(n);
            } else if (type == "vt") {
                glm::vec2 t;
                ss >> t.x >> t.y;
                temp_tex.push_back(t);
            } else if (type == "f") {
                std::string vertexStr;
                std::vector<VertexIdx> faceIndices;
                while (ss >> vertexStr) {
                    VertexIdx idx = {0, 0, 0};
                    size_t firstSlash = vertexStr.find('/');
                    size_t secondSlash = vertexStr.find('/', firstSlash + 1);
                    
                    idx.v = std::stoi(vertexStr.substr(0, firstSlash));
                    
                    if (firstSlash != std::string::npos) {
                        if (secondSlash != std::string::npos) {
                            if (secondSlash > firstSlash + 1) {
                                idx.vt = std::stoi(vertexStr.substr(firstSlash + 1, secondSlash - firstSlash - 1));
                            }
                            idx.vn = std::stoi(vertexStr.substr(secondSlash + 1));
                        } else {
                            idx.vt = std::stoi(vertexStr.substr(firstSlash + 1));
                        }
                    }
                    faceIndices.push_back(idx);
                }
                
                if (faceIndices.size() >= 3) {
                    for (size_t i = 1; i < faceIndices.size() - 1; ++i) {
                        indices.push_back(faceIndices[0]);
                        indices.push_back(faceIndices[i]);
                        indices.push_back(faceIndices[i+1]);
                    }
                }
            }
        }

        if (temp_pos.empty()) return;

        glm::vec3 minPos(std::numeric_limits<float>::max());
        glm::vec3 maxPos(std::numeric_limits<float>::lowest());
        for (const auto& p : temp_pos) {
            minPos = glm::min(minPos, p);
            maxPos = glm::max(maxPos, p);
        }

        glm::vec3 center = (minPos + maxPos) * 0.5f;
        glm::vec3 size = maxPos - minPos;
        float maxDim = std::max(std::max(size.x, size.y), size.z);
        float scale = 0.8f / maxDim; // Scale to fit in 0.8 unit box

        glm::vec3 offset = -center;
        offset.y = -minPos.y; // Align bottom to 0
        offset.y -= (maxPos.y - minPos.y) * 0.5f; // Actually, let's center it vertically too? No, sit on floor.
        // Wait, offset.y = -minPos.y means p.y + offset.y starts at 0.
        // Then we scale.
        
        // Correct logic:
        // 1. Translate so min.y is 0, and center.x/z is 0.
        // 2. Scale.
        
        glm::vec3 translation;
        translation.x = -center.x;
        translation.y = -minPos.y;
        translation.z = -center.z;

        for (const auto& idx : indices) {
            glm::vec3 p = temp_pos[idx.v - 1];
            p += translation;
            p *= scale;

            glm::vec3 n = (idx.vn > 0 && idx.vn <= static_cast<int>(temp_norm.size())) ? temp_norm[idx.vn - 1] : glm::vec3(0, 1, 0);
            glm::vec2 t = (idx.vt > 0 && idx.vt <= static_cast<int>(temp_tex.size())) ? temp_tex[idx.vt - 1] : glm::vec2(0, 0);

            outData.push_back(p.x);
            outData.push_back(p.y);
            outData.push_back(p.z);

            outData.push_back(n.x);
            outData.push_back(n.y);
            outData.push_back(n.z);

            outData.push_back(color.r);
            outData.push_back(color.g);
            outData.push_back(color.b);

            outData.push_back(t.x);
            outData.push_back(t.y);
        }
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
    void renderRoomIndexWithPortals(int roomId, std::vector<Portal*> portalsToRender, Portal* checkCurrent, const GameState& state, const Level& level, const GameState& next_state, float moveT, bool useFinal = false,
        bool enableClip = false, glm::vec4 clipPlane = glm::vec4(0.0f), bool enableVirtualView = false, glm::mat4 virtualView = glm::mat4(0.0f), glm::mat4 virtualSkyboxView = glm::mat4(0.0f))
    {
        if (roomId < 0 || roomId >= static_cast<int>(level.rooms.size())) return;
        const Room& room = level.rooms[roomId];
        if (room.size <= 0) return;
        glm::mat4 viewToUse = enableVirtualView ? virtualView : getCameraView(room, tileWorldSize_, 1.0f, 3.0f);

        // Render the rest of the room first, without portals
        renderRoomIndex(roomId, state, level, next_state, moveT,
            enableClip, clipPlane,
            enableVirtualView, virtualView, virtualSkyboxView);

        // Manage Clipping Distance 0
        if (enableClip) {
            glEnable(GL_CLIP_DISTANCE0);
        }

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
            unsigned int texID = useFinal ? portal->finalTexture : ((portal == checkCurrent) ? portal->tempTexture : portal->texture);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texID);
            glDrawArrays(GL_TRIANGLES, 0, portal->portalVertexNum);
            glBindVertexArray(0);
        }

        // Manage Clipping Distance 0
        if (enableClip) {
            glDisable(GL_CLIP_DISTANCE0);
        }

        glUseProgram(0);
    }

    // A recursive function that renders portal textures.
    // currentPortal: The portal currently being rendered
    // view: Current view matrix to observe the current portal
    // depth: Remaining recursion depth
    void renderPortalRecursive(Portal* currentPortal, glm::mat4 view, glm::mat4 skyboxView, int depth, const GameState& state, const Level& level, const GameState& next_state, float moveT, bool final = false) {
        // Between a pair of portals, the recursion *only* updates the texture of the *current* portal!
        // The other portal is always used as a view point, and is NEVER visible!

        // 0. Calculate Virtual View
        // This is the view out of pairPortal.
        glm::mat4 virtualView = currentPortal->getPortalCameraView(view);
        glm::mat4 virtualSkyboxView = currentPortal->getPortalCameraView(skyboxView);
        Portal* pairPortal = currentPortal->pairPortal;

        // 1. Base Case: End of recursion
        // At deepest level: render scene without portal
        if (depth <= 0) {
            // Set Up: Render pairPortal's view to FBO (currentPortal)
            glBindFramebuffer(GL_FRAMEBUFFER, currentPortal->fbo);
            glEnable(GL_DEPTH_TEST);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Render the scene without portals
            renderRoomIndex(pairPortal->getPortalPos(state.boxrooms).room, state, level, next_state, moveT,
                true, currentPortal->getPairPortalClippingPlane(),
                true, virtualView, virtualSkyboxView);

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
            renderPortalRecursive(portal, virtualView, virtualSkyboxView, nextDepth, state, level, next_state, moveT);
        }

        // 3. Texture Copy (Optional)
        // This is only necessary if we are about to render currentPortal,
        // i.e. currentPortal is in the same room as pairPortal.
        // currentPortal->texture contains the texture from level depth-1.
        // We need to copy it to tempTexture, since we are about to write to texture (FBO).

        if (renderCurrent) {
            // Bind FBO as source
            glBindFramebuffer(GL_READ_FRAMEBUFFER, currentPortal->fbo);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, currentPortal->tempTexture);

            // Copy content from FBO to tempTexture
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, currentPortal->scrWidth, currentPortal->scrHeight);

            glBindTexture(GL_TEXTURE_2D, 0);
        }

        // 4. Set Up: Render pairPortal's view to FBO (currentPortal)
        glBindFramebuffer(GL_FRAMEBUFFER, currentPortal->fbo);
        glEnable(GL_DEPTH_TEST);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Set viewport
        GLint prevViewport[4];
        glGetIntegerv(GL_VIEWPORT, prevViewport);
        glViewport(0, 0, currentPortal->scrWidth, currentPortal->scrHeight);

        // 5. Render the Scene
        // Draw other scene obj.s and all portals in the room
        renderRoomIndexWithPortals(pairRoomID, portalsToRender, currentPortal, state, level, next_state, moveT, false,
            true, currentPortal->getPairPortalClippingPlane(),
            true, virtualView, virtualSkyboxView);

        // Reset viewport
        glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

        // Unbind
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 6. Final Texture Copy (Optional)
        // If this is the outmost layer of recursion, store the result in finalTexture.
        // This is to avoid overwrites in later recursions.

        if (final) {
            // Bind FBO as source
            glBindFramebuffer(GL_READ_FRAMEBUFFER, currentPortal->fbo);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, currentPortal->finalTexture);

            // Copy content from FBO to tempTexture
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, currentPortal->scrWidth, currentPortal->scrHeight);

            glBindTexture(GL_TEXTURE_2D, 0);
        }
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
    std::unique_ptr<Shader> shader_;
    std::unique_ptr<Shader> depthShader_;
    std::unique_ptr<Shader> softDepthShader_;
    GLuint vaoSoft_ = 0;
    GLuint vboSoft_ = 0;
    std::unique_ptr<Shader> basicShader_;
    std::unique_ptr<Shader> softcubeShader_;
    std::unique_ptr<Shader> skyboxShader_;
    std::unique_ptr<SkyBox> skybox_;
    GLuint fbo_ = 0;
    GLuint depthMapFBO_ = 0;
    GLuint depthMap_ = 0;
    const unsigned int shadowMapResolution_ = 2048;
    std::vector<GLuint> roomTextures_;
    int textureWidth_ = 0;
    int textureHeight_ = 0;

    GLuint tileTex_ = 0;
    GLuint wallTex_ = 0;
    GLuint boxTex_ = 0;

    // 矩阵与窗口
    glm::mat4 projection_ = glm::mat4(1.0f);
    glm::mat4 view_ = glm::mat4(1.0f);
    int windowWidth_ = 0;
    int windowHeight_ = 0;
    std::vector<float> vertexData_;
    std::vector<float> skyVertexData_;
    std::vector<float> floorVertexData_;
    std::vector<float> wallVertexData_;
    std::vector<float> boxVertexData_;
    std::vector<float> softVertexData_;
    std::vector<LoadedObjData> loadedObjs;
    ObjThroughPortal objThroughPortalData;
    std::vector<float> rawChairData_;
    glm::vec2 moveDirection_{0.0f, 0.0f};
    glm::vec2 moveDirectionEnd_{ 0.0f, 0.0f };
    
    // Color for chair
    glm::vec3 chairColor = glm::vec3(RGB_2_FLT(0x8B5A2B));

    // Height of portal
    float portalHeight = 2.0f;

    // 2.5D 离散相机
    float cameraYaw_ = 0.0f;                // 0 -> +X
    float cameraPitch_ = -45.0f;            // 固定俯仰
    const float fixedPitch_ = -45.0f;
    glm::vec3 cameraPosition_{ 0.0f, 3.0f, 0.0f };
    glm::vec3 playerEyePosition_{0.0f, 0.02f, 0.0f};
    const float tileWorldSize_ = 1.0f;
    const float wallHeight_ = 5.0f;
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
    Input moveInput_ = UP;
    float moveDuration_ = 0.0f;
    float moveStartTime_ = 0.0f;
    
    // 光照系统
    std::unique_ptr<LabLightingSystem> lightingSystem_;

    // 待机动画状态（玩家）
    float idleDuration_ = 3.0f;

    // New: portals
    std::vector<Portal*> portalsList;
    std::unique_ptr<Shader> portalSurfaceShader_;

    GameState lastState_;
    int lastRoomId_ = -1;

    bool isStateChanged(const GameState& a, const GameState& b) const {
        if (!(a.player == b.player)) return true;
        if (a.boxes != b.boxes) return true;
        if (a.boxrooms != b.boxrooms) return true;
        return false;
    }
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

    // New: switch level logic
    void switchLevel() {
        if (!initialized_) return;
        renderer_.clearPortals();
        renderer_.resetCamera();
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

    bool isCameraRotating() const {
        return renderer_.isRotating();
    }

    void beginMoveAnimation(float duration, Input input) {
        renderer_.beginMoveAnimation(duration, input);
    }

    // Call from GLFW fb resize callback
    void handleResize(int width, int height) {
        renderer_.handleResize(width, height);
        uiManager_.handleResize(width, height);
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
