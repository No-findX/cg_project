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
#include <stb_image.h>

#include "model/include/gameplay.hpp"
#include "model/include/level_loader.hpp"
#include "view/uimanager.hpp"
#include "view/shader.hpp"
#include "view/gamelight.hpp"
#include "view/skybox_loader.hpp"

namespace detail {

#define RGB_2_FLT(x) ((((x)>>16) & 0xff) / 255.0f), ((((x)>>8) & 0xff) / 255.0f), (((x) & 0xff) / 255.0f)

class GameRenderer {
public:
    void init(int windowWidth, int windowHeight) {
        windowWidth_ = windowWidth;
        windowHeight_ = windowHeight;
        textureWidth_ = windowWidth_;
        textureHeight_ = windowHeight_;

        // ʹ�÷�װ�� Shader ����ļ�����
        shader_ = std::make_unique<Shader>("view/shader/pbr.vert", "view/shader/pbr.frag");
        
        // ��ʼ������ϵͳ
        lightingSystem_ = std::make_unique<LabLightingSystem>();
        depthShader_ = std::make_unique<Shader>("view/shader/shadow_depth.vert", "view/shader/shadow_depth.frag");
        setupShadowResources();
        basicShader_ = std::make_unique<Shader>("view/shader/basic.vert", "view/shader/basic.frag");
        softcubeShader_ = std::make_unique<Shader>("view/shader/softcube.vert", "view/shader/softcube.frag");
        skyboxShader_ = std::make_unique<Shader>("view/shader/skybox.vert", "view/shader/skybox.frag");

        // ��ʼ�� SkyBox����������·����
        std::vector<std::string> faces = {
            "view/assest/skybox/px.png",
            "view/assest/skybox/nx.png",
            "view/assest/skybox/py.png",
            "view/assest/skybox/ny.png",
            "view/assest/skybox/pz.png",
            "view/assest/skybox/nz.png"
        };
        skybox_ = std::make_unique<SkyBox>(faces);
        // Լ�� skyboxShader_ �Ĳ�������Ϊ "skybox"���󶨵� GL_TEXTURE0��
        if (skyboxShader_) {
            skyboxShader_->use();
            skyboxShader_->setInt("skybox", 0);
            glUseProgram(0);
        }

        // �������Σ�pos3 + color3 + tex2��
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

        const GLsizei basicStride = static_cast<GLsizei>(8 * sizeof(float));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, basicStride, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, basicStride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, basicStride, (void*)(6 * sizeof(float)));

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        tileTex_ = loadTexture2D("view/assest/texture/tile.png");
        wallTex_ = loadTexture2D("view/assest/texture/wall.png");
        boxTex_ = loadTexture2D("view/assest/texture/box.png");

        // ���� basicShader_ �Ĳ�����
        if (basicShader_) {
            basicShader_->use();
            basicShader_->setInt("Texture", 0);
            glUseProgram(0);
        }

        // ���������壨pos3 + color3 + normal2 + tex3��
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

    // ��תʱ�ܾ��µ���ת���󣬱���Ƕ�Ư��
    void rotateCameraBy90(bool left, float rotationtime = 0.0) {
        const float step = left ? -90.0f : 90.0f;

        // ��������ת��������µ���ת���루����������
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

    // ��ʼһ��λ�ƶ������������/�����ƽ���
    void beginMoveAnimation(float duration) {
        // ����ʱ�����������У����������ṩ����ʱ�����ɰ�������ֵ
        const float preferred = 0.3f;
        moveDuration_ = std::min(duration > 0.0f ? duration : preferred, preferred);
        moveStartTime_ = static_cast<float>(glfwGetTime());
        moving_ = true;
    }
        
    Input remapInputForCamera(Input input) const {
        return mapCameraRelativeInput(input);
    }

    // ��ѯ��ǰ�Ƿ��������ת�У�������������
    bool isRotating() const {
        return rotating_;
    }

    void render(const GameState& state, const Level& level, const GameState& next_state) {
        if (!basicShader_) return;
        if (level.rooms.empty()) return;

        // �����ת������ƽ����
        if (rotating_) {
            const float now = static_cast<float>(glfwGetTime());
            float elapsed = now - rotateStartTime_;
            if (elapsed >= rotateDuration_ * 0.98f) { // ����2%�ݴ���ǰ���
                cameraYaw_ = rotateTargetYaw_;
                // �淶���� [-180, 180] ��Χ����ֹ�����ۻ�
                while (cameraYaw_ > 180.0f) cameraYaw_ -= 360.0f;
                while (cameraYaw_ < -180.0f) cameraYaw_ += 360.0f;
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

        // λ�ƶ���ʱ��������ȼ���-����-�ȼ��٣�
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

                // �ٶ����Σ�ǰ 20% ���٣��� 20% ���٣��м�����
                const float acc = 0.2f;
                const float dec = 0.2f;
                const float constv = 1.0f - acc - dec; // 0.6
                // ��һ������ٶȣ�ʹ��λ��Ϊ 1
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
        if (vboSoft_) {
            glDeleteBuffers(1, &vboSoft_);
            vboSoft_ = 0;
        }
        if (depthShader_) {
            glDeleteProgram(depthShader_->ID);
            depthShader_.reset();
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

private:
    void updateProjection() {
        const float aspect = windowHeight_ == 0 ? 1.0f : static_cast<float>(windowWidth_) / static_cast<float>(windowHeight_);
        projection_ = glm::perspective(glm::radians(55.0f), aspect, 0.1f, 200.0f);
    }

    // �������ع���
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
        glm::vec3 lightPos = roomCenter - lightDir * 20.0f; // ��Զ��Դλ�������ɸ�������ͶӰ
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

    void renderRoomIndex(int roomId, const GameState& state, const Level& level, const GameState& next_state, float moveT) {
        if (roomId < 0 || roomId >= static_cast<int>(level.rooms.size())) return;
        const Room& room = level.rooms[roomId];
        if (room.size <= 0) return;

        vertexData_.clear();
        skyVertexData_.clear();
        floorVertexData_.clear();
        wallVertexData_.clear();
        boxVertexData_.clear();
        softVertexData_.clear();
        moveDirection_ = glm::vec2(0.0f); // Ĭ���޷������ڷ��ƶ���Ǳ����䣩

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
        cameraPosition_ = cameraPos;
        view_ = glm::lookAt(cameraPos, roomCenter, glm::vec3(0.0f, 1.0f, 0.0f));

        if (vertexData_.empty()) return;
        GLsizei mainVertexCount = static_cast<GLsizei>(vertexData_.size() / 9);
        GLsizei skyVertexCount = static_cast<GLsizei>(skyVertexData_.size() / 9);
        if (mainVertexCount == 0 && skyVertexCount == 0) return;

        glm::mat4 model = glm::mat4(1.0f);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        if (mainVertexCount > 0) {
            glBufferData(GL_ARRAY_BUFFER, vertexData_.size() * sizeof(float), vertexData_.data(), GL_DYNAMIC_DRAW);
        }

        glm::mat4 lightSpaceMatrix = calculateLightSpaceMatrix(roomCenter, currentRoomHalfExtent_);
        if (mainVertexCount > 0) {
            renderShadowPass(lightSpaceMatrix, model, mainVertexCount);
        }

        shader_->use();

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
        shader_->setVec3("viewPos", cameraPos);

        shader_->setFloat("metallic", 0.0f);
        shader_->setFloat("roughness", 0.5f);
        shader_->setFloat("ao", 1.0f);
        shader_->setMat4("model", model);
        shader_->setMat4("view", view_);
        shader_->setMat4("projection", projection_);
        shader_->setMat4("lightSpaceMatrix", lightSpaceMatrix);
        shader_->setInt("shadowMap", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthMap_);

        if (mainVertexCount > 0) {
            glBufferData(GL_ARRAY_BUFFER, vertexData_.size() * sizeof(float), vertexData_.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, mainVertexCount);
        }

        if (skyVertexCount > 0) {
            glBufferData(GL_ARRAY_BUFFER, skyVertexData_.size() * sizeof(float), skyVertexData_.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, skyVertexCount);
        }
        glm::vec3 cameraPos = roomCenter + offsetDir * glm::vec3(1.25f, 1.25f, 1.25f) * currentRoomHalfExtent_ + glm::vec3(0.0f, 4.0f, 0.0f);
        view_ = glm::lookAt(cameraPos, roomCenter, glm::vec3(0.0f, 1.0f, 0.0f));

        // ������պУ��Ȼ��ƣ�ʹ����ƽ�Ƶ��Ӿ���
        if (skybox_ && skyboxShader_) {
            glDepthFunc(GL_LEQUAL);
            skyboxShader_->use();
            glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(view_));
            skyboxShader_->setMat4("view", viewNoTranslation);
            skyboxShader_->setMat4("projection", projection_);
            skybox_->Draw(*skyboxShader_);
            glUseProgram(0);
            glDepthFunc(GL_LESS);
        }

        // ���ƾ�̬���Σ�����/ǽ/���ӣ�- �����󶨲�ͬ����
        basicShader_->use();
        basicShader_->setMat4("view", view_);
        basicShader_->setMat4("projection", projection_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);

        auto drawBatch = [&](const std::vector<float>& data, GLuint tex) {
            if (data.empty()) return;
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);
            glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(data.size() / 8));
        };

        drawBatch(floorVertexData_, tileTex_);
        drawBatch(wallVertexData_, wallTex_);
        drawBatch(boxVertexData_, boxTex_);

        glBindTexture(GL_TEXTURE_2D, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);

        // ���ƽ�ɫ������������ + ��ɫ���α䣩
        if (!softVertexData_.empty() && softcubeShader_) {
            softcubeShader_->use();
            softcubeShader_->setMat4("view", view_);
            softcubeShader_->setMat4("projection", projection_);
            softcubeShader_->setVec2("direction", moveDirection_);
            softcubeShader_->setFloat("moveT", moveT);

            // ����ʱ�䣨���� idleDuration_��
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

    glm::vec3 getCameraPosition() const {
        return cameraPosition_;
    }

    float appendRoomGeometry(const Room& room, int roomId, const GameState& state, const GameState& next_state, float moveT, float tileSize) {
        const int tileCount = room.size;
        const float boardHalf = tileCount * tileSize * 0.5f;

        playerEyePosition_ = glm::vec3(0.0f, 0.02f, 0.0f);

        // �������ζ��� push��pos3 + color3 + tex2
        auto pushVertex = [](std::vector<float>& buf, const glm::vec3& pos, const glm::vec3& color, const glm::vec2& tex) {
            buf.push_back(pos.x);
            buf.push_back(pos.y);
            buf.push_back(pos.z);
            buf.push_back(color.r);
            buf.push_back(color.g);
            buf.push_back(color.b);
            buf.push_back(tex.x);
            buf.push_back(tex.y);
        };

        auto pushQuad = [&](std::vector<float>& buf, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec3& color) {
            pushVertex(buf, v0, color, glm::vec2(0.0f, 0.0f));
            pushVertex(buf, v1, color, glm::vec2(1.0f, 0.0f));
            pushVertex(buf, v2, color, glm::vec2(1.0f, 1.0f));
            pushVertex(buf, v0, color, glm::vec2(0.0f, 0.0f));
            pushVertex(buf, v2, color, glm::vec2(1.0f, 1.0f));
            pushVertex(buf, v3, color, glm::vec2(0.0f, 1.0f));
        };

        // ���������壺��չ���� push
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
            // aTex.xy = ������Ĺ�һ��ƫ�ƣ�aTex.z = halfW
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
            pushQuad(floorVertexData_, v0, v3, v2, v1, color);
        };

        auto appendWallColumn = [&](float minX, float maxX, float minZ, float maxZ, float minY, float maxY, const glm::vec3& color) {
            glm::vec3 topColor = color;
            glm::vec3 sideColor = color * 0.85f;
            glm::vec3 top0(minX, maxY, minZ);
            glm::vec3 top1(maxX, maxY, minZ);
            glm::vec3 top2(maxX, maxY, maxZ);
            glm::vec3 top3(minX, maxY, maxZ);
            // �����棺ԭ˳���·��߳��£���Ϊ��ʱ��(top0->top3->top2->top1)ʹ�䳯��
            pushQuad(top0, top3, top2, top1, topColor);

            glm::vec3 bottom0(minX, minY, minZ);
            glm::vec3 bottom1(maxX, minY, minZ);
            glm::vec3 bottom2(maxX, minY, maxZ);
            glm::vec3 bottom3(minX, minY, maxZ);
            pushQuad(wallVertexData_, bottom1, bottom0, top0, top0=1, sideColor);
            pushQuad(wallVertexData_, bottom2, bottom1, top1, top2, sideColor);
            pushQuad(wallVertexData_, bottom3, bottom2, top2, top3, sideColor);
            pushQuad(wallVertexData_, bottom0, bottom3, top3, top0, sideColor);
        };

        auto appendBoxColumn = [&](float minX, float maxX, float minZ, float maxZ, float minY, float maxY, const glm::vec3& color) {
            glm::vec3 topColor = color;
            glm::vec3 sideColor = color * 0.85f;
            glm::vec3 top0(minX, maxY, minZ);
            glm::vec3 top1(maxX, maxY, minZ);
            glm::vec3 top2(maxX, maxY, maxZ);
            glm::vec3 top3(minX, maxY, maxZ);
            pushQuad(boxVertexData_, top0, top1, top2, top3, topColor);

            glm::vec3 bottom0(minX, minY, minZ);
            glm::vec3 bottom1(maxX, minY, minZ);
            glm::vec3 bottom2(maxX, minY, maxZ);
            glm::vec3 bottom3(minX, minY, maxZ);
            pushQuad(boxVertexData_, bottom0, bottom1, top1, top0, sideColor);
            pushQuad(boxVertexData_, bottom1, bottom2, top2, top1, sideColor);
            pushQuad(boxVertexData_, bottom2, bottom3, top3, top2, sideColor);
            pushQuad(boxVertexData_, bottom3, bottom0, top0, top3, sideColor);
        };

        // CPU �����������壺���������α䣬ֻ���ϸ�������� aNormal/aTex
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
                        if (axis == 1) { // ��/�� (x,z)
                            p00 = glm::vec3(uu0, fixed, vv0);
                            p10 = glm::vec3(uu1, fixed, vv0);
                            p11 = glm::vec3(uu1, fixed, vv1);
                            p01 = glm::vec3(uu0, fixed, vv1);
                        } else if (axis == 0) { // ��X (y,z)
                            p00 = glm::vec3(fixed, uu0, vv0);
                            p10 = glm::vec3(fixed, uu1, vv0);
                            p11 = glm::vec3(fixed, uu1, vv1);
                            p01 = glm::vec3(fixed, uu0, vv1);
                        } else { // ��Z (x,y)
                            p00 = glm::vec3(uu0, vv0, fixed);
                            p10 = glm::vec3(uu1, vv0, fixed);
                            p11 = glm::vec3(uu1, vv1, fixed);
                            p01 = glm::vec3(uu0, vv1, fixed);
                        }

                        // ������ CPU ���α�
                        auto makeTex = [&](const glm::vec3& p) -> glm::vec3 {
                            float xNorm = (halfW > 1e-6f) ? (p.x - cx) / halfW : 0.0f;
                            float zNorm = (halfW > 1e-6f) ? (p.z - cz) / halfW : 0.0f;
                            return glm::vec3(xNorm, zNorm, halfW);
                        };

                        pushSoftQuad(p00, p10, p11, p01, faceColor, n2, cx, cz, halfW);
                    }
                }
            };

            // ��/�ף�aNormal = (0,0)
            emitFaceGrid(1, maxY, minX, maxX, minZ, maxZ, topColor,   glm::vec2(0.0f, 0.0f));
            emitFaceGrid(1, minY, minX, maxX, minZ, maxZ, bottomColor,glm::vec2(0.0f, 0.0f));
            // ��X��aNormal = (��1,0)
            emitFaceGrid(0, minX, minY, maxY, minZ, maxZ, sideColor,  glm::vec2(-1.0f, 0.0f));
            emitFaceGrid(0, maxX, minY, maxY, minZ, maxZ, sideColor,  glm::vec2( 1.0f, 0.0f));
            // ��Z��aNormal = (0,��1)
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
            appendBoxColumn(minX, maxX, minZ, maxZ, 0.02f, 0.02f + height, color);
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

        auto camera_in_wall = [&](float wallMinX, float wallMaxX, float wallMinZ, float wallMaxZ) {
			float camZ = cameraPosition_.z, camX = cameraPosition_.x;
			float camerayaw = cameraYaw_ >= 0 ? cameraYaw_ : cameraYaw_ + 360.0f;
            if ( wallMinX <= camX && camX <= wallMaxX &&
				wallMinZ <= camZ && camZ <= wallMaxZ) {
                return true;
            }
            if (camerayaw <= 45.0f || camerayaw >= 315.0f) {
                return ( wallMaxX <= camX ) || ( wallMinX - 1.0f <= camX );
            } else if (camerayaw > 45.0f && camerayaw <= 135.0f) {
                return ( wallMaxZ <= camZ ) || ( wallMinZ - 1.0f <= camZ );
            } else if (camerayaw > 135.0f && camerayaw <= 225.0f) {
                return (wallMinX >= camX) || (wallMaxX + 1.0f >= camX);
            } else {
                return (wallMinZ >= camZ) || (wallMaxZ + 1.0f >= camZ);
			}
		};

        // ����/ǽ��
        for (int y = 0; y < tileCount; ++y) {
            for (int x = 0; x < tileCount; ++x) {
                auto bounds = boundsForCell(x, y);
                glm::vec3 baseColor = tileColorForCell(room.scene[y][x]);
                appendFloor(bounds[0], bounds[1], bounds[2], bounds[3], baseColor);
			appendSky(bounds[0], bounds[1], bounds[2], bounds[3], baseColor);
                glm::vec3 camPos = cameraPosition_;

                if (room.scene[y][x] == "#" && !camera_in_wall(bounds[0], bounds[1], bounds[2], bounds[3])) {
                    appendWallColumn(bounds[0], bounds[1], bounds[2], bounds[3], 0.0f, wallHeight_, glm::vec3(RGB_2_FLT(0xF4CFE9)));
                }
            }
        }

        // ������Ҳ�ֵ + �����˶�����
        auto drawPlayer = [&]() {
            const Pos& s = state.player;
            const Pos& e = next_state.player;
            glm::vec3 color(RGB_2_FLT(0xA6D6EA));
            float height = 0.90f;

            // ��������ʱ�䣨������ idleDuration_ ���ƣ�
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
                moveDirection_ = dir; // �� softcubeShader_ ʹ��

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

        // �������Ӳ�ֵ�����ֻ������Σ�
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
                    // Ŀ��״̬�в����ڸ����ӣ����类����/��ʧ�������ֵ�ǰ��ʾ���Ƕ�����
                    if (s.room == roomId) {
                        glm::vec2 c = centerForCell(s.x, s.y);
                        drawAtCenter(c, color, height);
                    }
                }
            }
        };

        drawBoxes(state.boxes, next_state.boxes, glm::vec3(RGB_2_FLT(0xCFD6F4)));
        drawBoxes(state.boxrooms, next_state.boxrooms, glm::vec3(RGB_2_FLT(0xCFF4ED)));
        drawPlayer();

        return boardHalf;
    }

    glm::vec3 tileColorForCell(const std::string& cell) const {
        if (cell == "#") return {0.25f, 0.27f, 0.32f};
        if (cell == "=") return {0.25f, 0.6f, 0.3f};
        if (cell == "_") return {0.7f, 0.6f, 0.25f};
        return {0.3f, 0.32f, 0.35f};
    }

    void pushVertex(const glm::vec3& pos, const glm::vec3& color) {
        // ����λ���Զ����㷨�ߣ��򻯰棺���ڶ���λ���ƶϣ�
        glm::vec3 normal = glm::normalize(pos - glm::vec3(0.0f, 0.0f, 0.0f));
        if (glm::length(normal) < 0.01f) {
            normal = glm::vec3(0.0f, 1.0f, 0.0f); // Ĭ������
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

    // GL ��Դ
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    std::unique_ptr<Shader> shader_;
    std::unique_ptr<Shader> depthShader_;
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

    // �����봰��
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
    glm::vec2 moveDirection_{0.0f, 0.0f};

    // 2.5D ��ɢ���
    float cameraYaw_ = 0.0f;                // 0 -> +X
    float cameraPitch_ = -45.0f;            // �̶�����
    const float fixedPitch_ = -45.0f;
    float currentRoomHalfExtent_ = 5.0f;
    glm::vec3 cameraPosition_{ 0.0f, 3.0f, 0.0f };
    glm::vec3 playerEyePosition_{0.0f, 0.02f, 0.0f};
    glm::vec2 cameraForward2D_{1.0f, 0.0f};
    const float tileWorldSize_ = 1.0f;
    const float wallHeight_ = 5.0f;
    const float eyeHeightOffset_ = 3.0f;
    const bool hidePlayerMesh_ = false;

    // ��ת����״̬
    bool rotating_ = false;
    float rotateStartYaw_ = 0.0f;
    float rotateTargetYaw_ = 0.0f;
    float rotateDuration_ = 0.0f;
    float rotateStartTime_ = 0.0f;

    // λ�ƶ���״̬�����/���ӣ�
    bool moving_ = false;
    float moveDuration_ = 0.0f;
    float moveStartTime_ = 0.0f;
    
    // ����ϵͳ
    std::unique_ptr<LabLightingSystem> lightingSystem_;

    // ��������״̬����ң�
    float idleDuration_ = 3.0f;
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
            // �ݴ�����δ�ṩ next_state����ʹ�� state ����
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

    // call from GLFW key callback: left/right arrows rotate camera 90�� and camera position updates next render
    void handleKey(int key) {
        if (!showGameScene_) return;

        // �����������ת�������µ���ת����
        if (renderer_.isRotating()) return;

        if (key == GLFW_KEY_U) renderer_.rotateCameraBy90(true, 0.5);
        else if (key == GLFW_KEY_I) renderer_.rotateCameraBy90(false, 0.5);
    }

    // ��������Ӧ�ò�����λ�ƶ���
    void beginMoveAnimation(float duration) {
        if (!showGameScene_) return;
        renderer_.beginMoveAnimation(duration);
    }

    // ��������ѯ����Ƿ�����ת�У��������������
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
