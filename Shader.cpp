#include "Shader.h"

#include <fstream>
#include <iostream>

#include <Utils.h>

ShaderSource::ShaderSource(GLenum shaderType, const std::string& filepath)
    : _id(0), _type(shaderType), _name(filepath) {

    std::ifstream file(filepath, std::ios_base::in);
    if (file.fail()) {
        perror(filepath.c_str());
        std::cerr << "ERROR: Failed to load shader: " << filepath << std::endl;
    }
    else {
        file.seekg(0, std::ios::end);
        _source.reserve((size_t)file.tellg());
        file.seekg(0, std::ios::beg);

        _source.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        if (!compile())
            std::cerr << "ERROR: Failed to compile shader: " << filepath << std::endl;
    }
}

ShaderSource::~ShaderSource() {
    if (_id != 0) {
        glDeleteShader(_id);
    }
}

GLuint ShaderSource::id() const {
    return _id;
}

GLenum ShaderSource::type() const {
    return _type;
}

const std::string& ShaderSource::name() const {
    return _name;
}

const std::string& ShaderSource::source() const {
    return _source;
}

void ShaderSource::inject(const std::string& code) {
    _source.insert(0, code);
}

bool ShaderSource::compile() {
    if (_id != 0) {
        std::cerr << "ERROR: Shader has already been compiled: " << _name << std::endl;
        return false;
    }

    GLuint id = glCreateShader(_type);
    if (id == 0) {
        std::cerr << "ERROR: Could not create shader: " << _name << std::endl;
        return false;
    }

    const char* str = _source.c_str();
    glShaderSource(id, 1, &str, 0);
    glCompileShader(id);

    // Check if shader compiled
    GLint result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);

    if (result != GL_TRUE) {
        GLint logLen;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &logLen);

        char* log = new char[logLen];
        glGetShaderInfoLog(id, logLen, &logLen, log);

        std::string message(log);
        delete[] log;

        std::cerr << "ERROR: Shader " << _name << " compilation log:\n" << message;

        glDeleteShader(id);
        return false;
    }
    else {
        _id = id;
        return true;
    }
}

Shader::Shader(const std::string& name) 
    : _id(0), _name(name) {

}


void Shader::addShader(const ShaderSource& shader) {
    _shaders.push_back(shader.id());
}

bool Shader::link() {

    // Create program and check for errors
    _id = glCreateProgram();
    if (_id == 0) {
        GLint logLen;
        glGetProgramiv(_id, GL_INFO_LOG_LENGTH, &logLen);    

        char* log = new char[logLen];
        glGetProgramInfoLog(_id, logLen, &logLen, log);

        std::string strLog(log);
        delete[] log;

        std::cerr << "ERROR: Could not create program: " << _name << std::endl;
        std::cerr << strLog << std::endl;

        return false;
    }

    // Attach shaders and check for attachment errors
    for (GLuint sid : _shaders) {
        glAttachShader(_id, sid);
        Utils::checkOpenGLError("Could not attach shader " + std::to_string(sid) + " to program " + _name + " (" + std::to_string(_id) + " )");
    }

    glLinkProgram(_id);

    // Check program log for the error and print it
    GLint res;
    glGetProgramiv(_id, GL_LINK_STATUS, &res);
    if (res != GL_TRUE) {
        GLint logLen;
        glGetProgramiv(_id, GL_INFO_LOG_LENGTH, &logLen);

        char* log = new char[logLen];
        glGetProgramInfoLog(_id, logLen, &logLen, log);

        std::string strLog(log);
        delete[] log;

        std::cerr << "ERROR: Could not link program: " << _name << std::endl;
        std::cerr << strLog << std::endl;

        for (GLuint sid : _shaders)
            glDetachShader(_id, sid);

        glDeleteProgram(_id);

        return false;
    }

    // Detach shaders after successful linking
    for (GLuint sid : _shaders)
        glDetachShader(_id, sid);
    
    return true;
}

GLuint Shader::id() const {
    return _id;
}

const std::string& Shader::name() const {
    return _name;
}

const std::vector<GLuint>& Shader::shaders() const {
    return _shaders;
}