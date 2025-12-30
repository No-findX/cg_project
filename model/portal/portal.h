#pragma once

#include <algorithm>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vector_angle.hpp>


class Portal {
public:
	glm::vec3 position;
	glm::vec3 normal;
	unsigned int fbo;
	unsigned int texture;
	unsigned int tempTexture;
	unsigned int rbo;

	unsigned int VAO_;
	unsigned int wrapperVAO_;

	Portal* pairPortal;

	float height;
	float width;

	Portal(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 normal = glm::vec3(0.0f, 0.0f, 1.0f), GLFWwindow* window, float height = 2, float width = 1) {
		this->position = position;
		this->normal = glm::normalize(normal);

		fbo = texture = rbo = 0;
		pairPortal = nullptr;

		this->height = height;
		this->width = width;

		createFBO(window);
	}

	Portal(float posX = 0.0f, float posY = 0.0f, float posZ = 0.0f, float normX = 0.0f, float normY = 0.0f, GLFWwindow* window, float normZ = 1.0f, float height = 2, float width = 1) {
		this->position = glm::vec3(posX, posY, posZ);
		this->normal = glm::normalize(glm::vec3(normX, normY, normZ));

		fbo = texture = rbo = 0;
		pairPortal = nullptr;

		this->height = height;
		this->width = width;

		createFBO(window);
	}

	void setPairPortal(Portal* pair) {
		this->pairPortal = pair;
		pair->pairPortal = this;
	}

	void createFBO(GLFWwindow* window) {
		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);

		// Get screen dimensions
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		// Create color texture
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGB,
			width,
			height,
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
			width,
			height
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
			width,
			height,
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

	// Only call this function once!
	unsigned int getPortalVAO() {
		float vertices[] = {
			-(width / 2), -(height / 2), 0.0f,		0.0f, 0.0f,
			 (width / 2), -(height / 2), 0.0f,		1.0f, 0.0f,
			 (width / 2),  (height / 2), 0.0f,		1.0f, 1.0f,
			 (width / 2),  (height / 2), 0.0f,		1.0f, 1.0f,
			-(width / 2),  (height / 2), 0.0f,		0.0f, 1.0f,
			-(width / 2), -(height / 2), 0.0f,		0.0f, 0.0f,
		};
		unsigned portalVBO, portalVAO;
		glGenVertexArrays(1, &portalVAO);
		glGenBuffers(1, &portalVBO);

		glBindVertexArray(portalVAO);

		glBindBuffer(GL_ARRAY_BUFFER, portalVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		
		// position attribute
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		// texture coord attribute
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		this->VAO_ = portalVAO;

		return portalVAO;
	}

	// Call only once per portal!
	unsigned int getWrapperVAO(float height = 2.1, float width = 1.1, glm::vec3 color = glm::vec3(0.0f, 0.0f, 0.0f)) {
		float vertices[] = {
		-(width / 2), -(height / 2), -0.5f,    color.x, color.y, color.z,
		 (width / 2), -(height / 2), -0.5f,    color.x, color.y, color.z,
		 (width / 2),  (height / 2), -0.5f,    color.x, color.y, color.z,
		 (width / 2),  (height / 2), -0.5f,    color.x, color.y, color.z,
		-(width / 2),  (height / 2), -0.5f,    color.x, color.y, color.z,
		-(width / 2), -(height / 2), -0.5f,    color.x, color.y, color.z,

		-(width / 2), -(height / 2), -0.1f,    color.x, color.y, color.z,
		 (width / 2), -(height / 2), -0.1f,    color.x, color.y, color.z,
		 (width / 2),  (height / 2), -0.1f,    color.x, color.y, color.z,
		 (width / 2),  (height / 2), -0.1f,    color.x, color.y, color.z,
		-(width / 2),  (height / 2), -0.1f,    color.x, color.y, color.z,
		-(width / 2), -(height / 2), -0.1f,    color.x, color.y, color.z,

		-(width / 2),  (height / 2), -0.1f,    color.x, color.y, color.z,
		-(width / 2),  (height / 2), -0.5f,    color.x, color.y, color.z,
		-(width / 2), -(height / 2), -0.5f,    color.x, color.y, color.z,
		-(width / 2), -(height / 2), -0.5f,    color.x, color.y, color.z,
		-(width / 2), -(height / 2), -0.1f,    color.x, color.y, color.z,
		-(width / 2),  (height / 2), -0.1f,    color.x, color.y, color.z,

		 (width / 2),  (height / 2), -0.1f,    color.x, color.y, color.z,
		 (width / 2),  (height / 2), -0.5f,    color.x, color.y, color.z,
		 (width / 2), -(height / 2), -0.5f,    color.x, color.y, color.z,
		 (width / 2), -(height / 2), -0.5f,    color.x, color.y, color.z,
		 (width / 2), -(height / 2), -0.1f,    color.x, color.y, color.z,
		 (width / 2),  (height / 2), -0.1f,    color.x, color.y, color.z,

		-(width / 2), -(height / 2), -0.5f,    color.x, color.y, color.z,
		 (width / 2), -(height / 2), -0.5f,    color.x, color.y, color.z,
		 (width / 2), -(height / 2), -0.1f,    color.x, color.y, color.z,
		 (width / 2), -(height / 2), -0.1f,    color.x, color.y, color.z,
		-(width / 2), -(height / 2), -0.1f,    color.x, color.y, color.z,
		-(width / 2), -(height / 2), -0.5f,    color.x, color.y, color.z,

		-(width / 2),  (height / 2), -0.5f,    color.x, color.y, color.z,
		 (width / 2),  (height / 2), -0.5f,    color.x, color.y, color.z,
		 (width / 2),  (height / 2), -0.1f,    color.x, color.y, color.z,
		 (width / 2),  (height / 2), -0.1f,    color.x, color.y, color.z,
		-(width / 2),  (height / 2), -0.1f,    color.x, color.y, color.z,
		-(width / 2),  (height / 2), -0.5f,    color.x, color.y, color.z,
		};
		unsigned wrapperVBO, wrapperVAO;
		glGenVertexArrays(1, &wrapperVAO);
		glGenBuffers(1, &wrapperVBO);

		glBindVertexArray(wrapperVAO);

		glBindBuffer(GL_ARRAY_BUFFER, wrapperVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		// position attribute
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		// color attribute
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		this->wrapperVAO_ = wrapperVAO;

		return wrapperVAO;
	}
};