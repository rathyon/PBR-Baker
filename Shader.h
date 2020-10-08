#ifndef __XGP_SHADER_H__
#define __XGP_SHADER_H__

#include <GL/glew.h>
#include <string>
#include <vector>

class ShaderSource {
public:
	ShaderSource(GLenum shaderType, const std::string& filepath);
	~ShaderSource();

	GLuint id() const;
	GLenum type() const;

	const std::string& name() const;
	const std::string& source() const;

	void inject(const std::string& code);

	bool compile();

private:
	GLuint _id;
	GLenum _type;
	std::string _name;
	std::string _source;

};

class Shader {
public:
	Shader(const std::string& name);

	void addShader(const ShaderSource& shader);
	bool link();

	GLuint id() const;
	const std::string& name() const;
	const std::vector<GLuint>& shaders() const;

private:
	GLuint _id;
	std::string _name;
	std::vector<GLuint> _shaders;
};

#endif