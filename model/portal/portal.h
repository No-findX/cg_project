#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include <algorithm>
#include <iostream>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "../include/gameplay.hpp"


// Describe which side of the block the portal is on
enum PortalPosition {
	XPos,
	XNeg,
	ZPos,
	ZNeg
};

class Portal {
public:
	glm::vec3 position;
	glm::vec3 normal;
	
	unsigned int fbo;
	unsigned int texture;
	unsigned int tempTexture;
    unsigned int finalTexture;
	unsigned int rbo;
	int scrWidth;
	int scrHeight;

	unsigned int portalVAO_;
	unsigned int portalVBO_;
	GLsizei portalVertexNum;
	unsigned int wrapperVAO_;
	unsigned int wrapperVBO_;
	GLsizei wrapperVertexNum;

	Portal* pairPortal;
	PortalPosition relativePos;
	int boxroomID;
	
	// A note: portalPos *only* applies to stationary portals, dynamic portals use getPortalPos to calculate!
	bool stationary = false;
	Pos portalPos;

	float height;
	float width;

	Portal(Pos portalPos, PortalPosition relativePos, int boxroomID, int scrHeight, int scrWidth, glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 normal = glm::vec3(0.0f, 0.0f, 1.0f), float height = 2, float width = 1) {
		this->position = position;
		this->normal = glm::normalize(normal);

		fbo = texture = rbo = 0;
		pairPortal = nullptr;

		this->height = height;
		this->width = width;
		this->portalPos = portalPos;
		this->relativePos = relativePos;
		this->boxroomID = boxroomID;

		this->scrWidth = scrWidth;
		this->scrHeight = scrHeight;

		createFBO(scrHeight, scrWidth);
		createVAOs();
	}

	~Portal() {
		if (portalVBO_) {
			glDeleteBuffers(1, &portalVBO_);
			portalVBO_ = 0;
		}
		if (portalVAO_) {
			glDeleteVertexArrays(1, &portalVAO_);
			portalVAO_ = 0;
		}
		if (wrapperVBO_) {
			glDeleteBuffers(1, &wrapperVBO_);
			wrapperVBO_ = 0;
		}
		if (wrapperVAO_) {
			glDeleteVertexArrays(1, &wrapperVAO_);
			wrapperVAO_ = 0;
		}
		if (texture) {
			glDeleteTextures(GL_TEXTURE_2D, &texture);
			texture = 0;
		}
		if (tempTexture) {
			glDeleteTextures(GL_TEXTURE_2D, &tempTexture);
			tempTexture = 0;
		}
		if (rbo) {
			glDeleteRenderbuffers(1, &rbo);
			rbo = 0;
		}
		if (fbo) {
			glDeleteFramebuffers(1, &fbo);
			fbo = 0;
		}
	}

	Pos getPortalPos(std::map<int, Pos> boxrooms) {
		if (stationary) {
			return portalPos;
		}
		
		auto boxroom = boxrooms.find(boxroomID);
		const Pos& pos = boxroom->second;
		return pos;
	}

	void setPairPortal(Portal* pair) {
		this->pairPortal = pair;
		pair->pairPortal = this;
	}

	void setPosition(glm::vec3 newPos) {
		this->position = newPos;
	}

	void createFBO(int scrHeight, int scrWidth) {
		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);

		// Create color texture
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGB,
			scrWidth,
			scrHeight,
			0,
			GL_RGB,
			GL_UNSIGNED_BYTE,
			NULL
		);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glFramebufferTexture2D(
			GL_FRAMEBUFFER,
			GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D,
			texture,
			0
		);

		// Create depth buffer
		glGenRenderbuffers(1, &rbo);
		glBindRenderbuffer(GL_RENDERBUFFER, rbo);
		glRenderbufferStorage(
			GL_RENDERBUFFER,
			GL_DEPTH24_STENCIL8,
			scrWidth,
			scrHeight
		);

		glFramebufferRenderbuffer(
			GL_FRAMEBUFFER,
			GL_DEPTH_STENCIL_ATTACHMENT,
			GL_RENDERBUFFER,
			rbo
		);

		// Create temp texture for recursive rendering
		glGenTextures(1, &tempTexture);
		glBindTexture(GL_TEXTURE_2D, tempTexture);
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGB,
			scrWidth,
			scrHeight,
			0,
			GL_RGB,
			GL_UNSIGNED_BYTE,
			NULL
		);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Create final texture to store the result of each recursive rendering
        // Create temp texture for recursive rendering
        glGenTextures(1, &finalTexture);
        glBindTexture(GL_TEXTURE_2D, finalTexture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGB,
            scrWidth,
            scrHeight,
            0,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            NULL
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// Verify
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cout << "FBO ERROR\n";

		// Unbind
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	// New: Resize fb when window is resized
	void resizeFrameBuffer(int newWidth, int newHeight) {
		// 1. Safeguard: width / height = 0 when window minimized
		if (newWidth <= 0 || newHeight <= 0) return;

		// 2. Update member var.s
		this->scrWidth = newWidth;
		this->scrHeight = newHeight;

		// 3. Adjust texture size
		// Calling glTexImage2D redistributes GPU memory, but keeps the texture ID.
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(
			GL_TEXTURE_2D, 0, GL_RGB,
			newWidth, newHeight,
			0, GL_RGB, GL_UNSIGNED_BYTE, NULL
		);

		// 4. Adjust temp texture size
		glBindTexture(GL_TEXTURE_2D, tempTexture);
		glTexImage2D(
			GL_TEXTURE_2D, 0, GL_RGB,
			newWidth, newHeight,
			0, GL_RGB, GL_UNSIGNED_BYTE, NULL
		);

        // 5. Adjust final texture size
        glBindTexture(GL_TEXTURE_2D, finalTexture);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGB,
            newWidth, newHeight,
            0, GL_RGB, GL_UNSIGNED_BYTE, NULL
        );

		// Unbind texture
		glBindTexture(GL_TEXTURE_2D, 0);

		// 6. Adjust RBO size
		glBindRenderbuffer(GL_RENDERBUFFER, rbo);
		glRenderbufferStorage(
			GL_RENDERBUFFER,
			GL_DEPTH24_STENCIL8,
			newWidth,
			newHeight
		);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);

		// 注意：只要 Texture ID 和 RBO ID 没有变，就不需要重新调用 glFramebufferTexture2D 或 glFramebufferRenderbuffer
		// FBO 会自动链接到新的内存大小。
	}

	glm::mat4 getModelMatrix() {
		glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
		// Change reference if normal is parallel to Up vector
		if (glm::abs(glm::dot(normal, up)) > 0.99f) {
			up = glm::vec3(0.0f, 0.0f, 1.0f);
		}
		glm::vec3 right = glm::normalize(glm::cross(up, normal));
		glm::vec3 actualUp = glm::cross(normal, right);

		glm::mat4 model = glm::mat4(1.0f);
		model[0] = glm::vec4(right, 0.0f);
		model[1] = glm::vec4(actualUp, 0.0f);
		model[2] = glm::vec4(normal, 0.0f);
		model[3] = glm::vec4(position, 1.0f);
		return model;
	}

	glm::mat4 getPortalCameraView(glm::mat4 camView) {
		glm::mat4 camWorld = glm::inverse(camView);
		glm::mat4 camInA = glm::inverse(this->getModelMatrix()) * camWorld;

		glm::mat4 flip = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		camInA = flip * camInA;

		glm::mat4 portalCamWorldB = pairPortal->getModelMatrix() * camInA;
		glm::mat4 portalViewB = glm::inverse(portalCamWorldB);

		return portalViewB;
	}

	glm::vec4 getPortalClippingPlane() {
		glm::vec4 portalPlane(
			this->normal,
			-glm::dot(this->normal, this->position)
		);
		return portalPlane;
	}

	glm::vec4 getPairPortalClippingPlane() {
		return pairPortal->getPortalClippingPlane();
	}

	void createVAOs() {
		// Portal VAO
		glGenVertexArrays(1, &portalVAO_);
		glGenBuffers(1, &portalVBO_);
		glBindVertexArray(portalVAO_);
		glBindBuffer(GL_ARRAY_BUFFER, portalVBO_);
		glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
		
		// position attribute
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		// texture coord attribute
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		// Wrapper VAO
		glGenVertexArrays(1, &wrapperVAO_);
		glGenBuffers(1, &wrapperVBO_);
		glBindVertexArray(wrapperVAO_);
		glBindBuffer(GL_ARRAY_BUFFER, wrapperVBO_);
		glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);

		// position attribute
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		// color attribute
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	void setVAOs(glm::vec3 color = glm::vec3(0.0f, 0.0f, 0.0f), float thickness = 0.03f) {
		// Build wrapper (frame) vertices programmatically. The frame is the outer rectangle minus an inner rectangle
		// Outer rectangle extents
		float halfW = width / 2.0f;
		float halfH = height / 2.0f;
		// Thickness defines the frame inset and the depth (z size)
		float t = thickness;
		float halfT = t / 2.0f;
		float innerHalfW = std::max(0.0f, halfW - t);
		float innerHalfH = std::max(0.0f, halfH - t);
		float zFront = t;
		float zBack = 0.0f;

		// Build portal surface by hand first
		float portalVertices[] = {
			-innerHalfW, -innerHalfH, halfT,		0.0f, 0.0f,
			 innerHalfW, -innerHalfH, halfT,		1.0f, 0.0f,
			 innerHalfW,  innerHalfH, halfT,		1.0f, 1.0f,
			 innerHalfW,  innerHalfH, halfT,		1.0f, 1.0f,
			-innerHalfW,  innerHalfH, halfT,		0.0f, 1.0f,
			-innerHalfW, -innerHalfH, halfT,		0.0f, 0.0f,
		};

		auto pushVertex = [&](std::vector<float>& buf, float x, float y, float z) {
			buf.push_back(x);
			buf.push_back(y);
			buf.push_back(z);
			buf.push_back(color.x);
			buf.push_back(color.y);
			buf.push_back(color.z);
		};

		auto addQuad = [&](std::vector<float>& buf,
			const glm::vec3& v0,
			const glm::vec3& v1,
			const glm::vec3& v2,
			const glm::vec3& v3) {
			// two triangles: v0,v1,v2 and v2,v3,v0
			pushVertex(buf, v0.x, v0.y, v0.z);
			pushVertex(buf, v1.x, v1.y, v1.z);
			pushVertex(buf, v2.x, v2.y, v2.z);
			pushVertex(buf, v2.x, v2.y, v2.z);
			pushVertex(buf, v3.x, v3.y, v3.z);
			pushVertex(buf, v0.x, v0.y, v0.z);
		};

		std::vector<float> wrapper;

		// Define corner arrays (order: 0 = bottom-left, 1 = bottom-right, 2 = top-right, 3 = top-left)
		glm::vec3 outerFront[4] = {
			glm::vec3(-halfW, -halfH, zFront),
			glm::vec3( halfW, -halfH, zFront),
			glm::vec3( halfW,  halfH, zFront),
			glm::vec3(-halfW,  halfH, zFront)
		};
		glm::vec3 outerBack[4] = {
			glm::vec3(-halfW, -halfH, zBack),
			glm::vec3( halfW, -halfH, zBack),
			glm::vec3( halfW,  halfH, zBack),
			glm::vec3(-halfW,  halfH, zBack)
		};

		glm::vec3 innerFront[4] = {
			glm::vec3(-innerHalfW, -innerHalfH, zFront),
			glm::vec3( innerHalfW, -innerHalfH, zFront),
			glm::vec3( innerHalfW,  innerHalfH, zFront),
			glm::vec3(-innerHalfW,  innerHalfH, zFront)
		};
		glm::vec3 innerBack[4] = {
			glm::vec3(-innerHalfW, -innerHalfH, zBack),
			glm::vec3( innerHalfW, -innerHalfH, zBack),
			glm::vec3( innerHalfW,  innerHalfH, zBack),
			glm::vec3(-innerHalfW,  innerHalfH, zBack)
		};

		// Front quads for each edge (top, bottom, left, right)
		// Top
		addQuad(wrapper, outerFront[3], outerFront[2], innerFront[2], innerFront[3]);
		// Right
		addQuad(wrapper, outerFront[2], outerFront[1], innerFront[1], innerFront[2]);
		// Bottom
		addQuad(wrapper, outerFront[1], outerFront[0], innerFront[0], innerFront[1]);
		// Left
		addQuad(wrapper, outerFront[0], outerFront[3], innerFront[3], innerFront[0]);

		// Back quads (same as front but on back plane)
		// Top
		addQuad(wrapper, innerBack[3], innerBack[2], outerBack[2], outerBack[3]);
		// Right
		addQuad(wrapper, innerBack[2], innerBack[1], outerBack[1], outerBack[2]);
		// Bottom
		addQuad(wrapper, innerBack[1], innerBack[0], outerBack[0], outerBack[1]);
		// Left
		addQuad(wrapper, innerBack[0], innerBack[3], outerBack[3], outerBack[0]);

		// Side faces for outer rectangle (connect front->back)
		for (int i = 0; i < 4; ++i) {
			int ni = (i + 1) % 4;
			addQuad(wrapper, outerFront[i], outerFront[ni], outerBack[ni], outerBack[i]);
		}

		// Side faces for inner rectangle (connect front->back), wound opposite so normals point inward correctly
		for (int i = 0; i < 4; ++i) {
			int ni = (i + 1) % 4;
			// reverse order to flip normal
			addQuad(wrapper, innerFront[ni], innerFront[i], innerBack[i], innerBack[ni]);
		}

		// Upload portalVertices to portal VBO
		glBindVertexArray(portalVAO_);
		glBindBuffer(GL_ARRAY_BUFFER, portalVBO_);
		glBufferData(GL_ARRAY_BUFFER, sizeof(portalVertices), portalVertices, GL_STATIC_DRAW);
		this->portalVertexNum = static_cast<GLsizei>(6);

		// Upload wrapper vertices to wrapper VBO
		glBindVertexArray(wrapperVAO_);
		glBindBuffer(GL_ARRAY_BUFFER, wrapperVBO_);
		glBufferData(GL_ARRAY_BUFFER, wrapper.size() * sizeof(float), wrapper.data(), GL_STATIC_DRAW);
		this->wrapperVertexNum = static_cast<GLsizei>(wrapper.size() / 6);

		// Unbind
		glBindVertexArray(0);
	}
};
