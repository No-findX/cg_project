#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

class Shader
{
public:
    unsigned int ID;
    // constructor generates the shader on the fly
    // ------------------------------------------------------------------------
    Shader(const char* vertexPath, const char* fragmentPath,
        const char* tscPath = NULL, const char* tesPath = NULL)
    {
        if (!(std::filesystem::exists(vertexPath)) || !(std::filesystem::exists(fragmentPath)))
        {
            std::cout << "ERROR::FILE_DOES_NOT_EXIST in " << std::filesystem::current_path() << std::endl;
        }
        if (tscPath != NULL && !std::filesystem::exists(tscPath))
        {
            std::cout << "WARNING::TESS_CONTROL_SHADER_FILE_DOES_NOT_EXIST: " << tscPath << std::endl;
            tscPath = NULL;
        }
        if (tesPath != NULL && !std::filesystem::exists(tesPath))
        {
            std::cout << "WARNING::TESS_EVALUATION_SHADER_FILE_DOES_NOT_EXIST: " << tesPath << std::endl;
            tesPath = NULL;
        }

        // 1. retrieve the vertex/fragment (and optional tessellation) source code from filePath
        std::string vertexCode;
        std::string fragmentCode;
        std::string tcsCode;
        std::string tesCode;
        std::ifstream vShaderFile;
        std::ifstream fShaderFile;
        std::ifstream tcsShaderFile;
        std::ifstream tesShaderFile;
        // ensure ifstream objects can throw exceptions:
        vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        try
        {
            // open files
            vShaderFile.open(vertexPath);
            fShaderFile.open(fragmentPath);
            std::stringstream vShaderStream, fShaderStream;
            // read file's buffer contents into streams
            vShaderStream << vShaderFile.rdbuf();
            fShaderStream << fShaderFile.rdbuf();
            // close file handlers
            vShaderFile.close();
            fShaderFile.close();
            // convert stream into string
            vertexCode = vShaderStream.str();
            fragmentCode = fShaderStream.str();

            // read tessellation control shader if provided
            if (tscPath != NULL)
            {
                tcsShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
                tcsShaderFile.open(tscPath);
                std::stringstream tcsShaderStream;
                tcsShaderStream << tcsShaderFile.rdbuf();
                tcsShaderFile.close();
                tcsCode = tcsShaderStream.str();
            }

            // read tessellation evaluation shader if provided
            if (tesPath != NULL)
            {
                tesShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
                tesShaderFile.open(tesPath);
                std::stringstream tesShaderStream;
                tesShaderStream << tesShaderFile.rdbuf();
                tesShaderFile.close();
                tesCode = tesShaderStream.str();
            }
        }
        catch (std::ifstream::failure& e)
        {
            std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
        }
        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();
        const char* tcsShaderCode = tcsCode.empty() ? NULL : tcsCode.c_str();
        const char* tesShaderCode = tesCode.empty() ? NULL : tesCode.c_str();

        // 2. compile shaders
        unsigned int vertex, fragment, tcs = 0, tes = 0;
        // vertex shader
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");
        // fragment Shader
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");

        // optional tessellation control shader
        if (tcsShaderCode != NULL)
        {
            tcs = glCreateShader(GL_TESS_CONTROL_SHADER);
            glShaderSource(tcs, 1, &tcsShaderCode, NULL);
            glCompileShader(tcs);
            checkCompileErrors(tcs, "TESS_CONTROL");
        }

        // optional tessellation evaluation shader
        if (tesShaderCode != NULL)
        {
            tes = glCreateShader(GL_TESS_EVALUATION_SHADER);
            glShaderSource(tes, 1, &tesShaderCode, NULL);
            glCompileShader(tes);
            checkCompileErrors(tes, "TESS_EVALUATION");
        }

        // shader Program
        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        if (tcs != 0) glAttachShader(ID, tcs);
        if (tes != 0) glAttachShader(ID, tes);

        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");

        // delete the shaders as they're linked into our program now and no longer necessary
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        if (tcs != 0) glDeleteShader(tcs);
        if (tes != 0) glDeleteShader(tes);

        std::cout << "Compiling Accomplished" << std::endl;
    }
    // activate the shader
    // ------------------------------------------------------------------------
    void use()
    {
        glUseProgram(ID);
    }
    // utility uniform functions
    // ------------------------------------------------------------------------
    void setBool(const std::string& name, bool value) const
    {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
    }
    // ------------------------------------------------------------------------
    void setInt(const std::string& name, int value) const
    {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }
    // ------------------------------------------------------------------------
    void setFloat(const std::string& name, float value) const
    {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }
    void setVec2(const std::string& name, const glm::vec2& value) const
    {
        glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
    }
    void setVec2(const std::string& name, float x, float y) const
    {
        glUniform2f(glGetUniformLocation(ID, name.c_str()), x, y);
    }
    // ------------------------------------------------------------------------
    void setVec3(const std::string& name, const glm::vec3& value) const
    {
        glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
    }
    void setVec3(const std::string& name, float x, float y, float z) const
    {
        glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
    }
    // ------------------------------------------------------------------------
    void setVec4(const std::string& name, const glm::vec4& value) const
    {
        glUniform4fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
    }
    void setVec4(const std::string& name, float x, float y, float z, float w) const
    {
        glUniform4f(glGetUniformLocation(ID, name.c_str()), x, y, z, w);
    }
    // ------------------------------------------------------------------------
    void setMat2(const std::string& name, const glm::mat2& mat) const
    {
        glUniformMatrix2fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }
    // ------------------------------------------------------------------------
    void setMat3(const std::string& name, const glm::mat3& mat) const
    {
        glUniformMatrix3fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }
    // ------------------------------------------------------------------------
    void setMat4(const std::string& name, const glm::mat4& mat) const
    {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }

private:
    // utility function for checking shader compilation/linking errors.
    // ------------------------------------------------------------------------
    void checkCompileErrors(unsigned int shader, std::string type)
    {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM")
        {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success)
            {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
        else
        {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success)
            {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
    }
};
#endif