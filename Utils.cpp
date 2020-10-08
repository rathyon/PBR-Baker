#include "Utils.h"

#include <iostream>

const char* Utils::getGLErrorString(GLenum err)
{
    switch (err) {
    case GL_NO_ERROR:
        return "No error";
    case GL_INVALID_ENUM:
        return "Invalid enum";
    case GL_INVALID_VALUE:
        return "Invalid value";
    case GL_INVALID_OPERATION:
        return "Invalid operation";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "Invalid framebuffer operation";
    case GL_STACK_OVERFLOW:
        return "Stack overflow";
    case GL_STACK_UNDERFLOW:
        return "Stack underflow";
    case GL_OUT_OF_MEMORY:
        return "Out of memory";
    default:
        return "Unknown error";
    }
}

void Utils::checkOpenGLError(const std::string& error) {
    if (Utils::isOpenGLError()) {
        throwError(error);
    }
}

bool Utils::isOpenGLError() {
    bool isError = false;
    GLenum errCode;
    const char* errString;
    while ((errCode = glGetError()) != GL_NO_ERROR) {
        isError = true;
        errString = getGLErrorString(errCode);
        std::cerr << "OpenGL ERROR:" << errString << std::endl;
    }
    return isError;
}

void Utils::throwError(const std::string& error) {
	std::cerr << error << std::endl;
	//std::cin.get();
	exit(EXIT_FAILURE);
}