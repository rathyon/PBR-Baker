#ifndef __XGP_UTILS_H__
#define __XGP_UTILS_H__

#include <GL/glew.h>
#include <string>

namespace Utils {
	const char* getGLErrorString(GLenum err);
	void checkOpenGLError(const std::string& error);
	bool isOpenGLError();
	void throwError (const std::string& error);
}

#endif // !__UTILS_H__