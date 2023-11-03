#include "common.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../ext/stb_image/stb_image.h"

// stlib
#include <vector>
#include <iostream>
#include <sstream>
#include <cmath>

void gl_flush_errors()
{
	while (glGetError() != GL_NO_ERROR);
}

bool gl_has_errors()
{
	GLenum error = glGetError();

	if (error == GL_NO_ERROR) return false;

	while (error != GL_NO_ERROR)
	{
		const char* error_str = "";
		switch (error)
		{
			case GL_INVALID_OPERATION:
			error_str = "INVALID_OPERATION";
			break;
			case GL_INVALID_ENUM:
			error_str = "INVALID_ENUM";
			break;
			case GL_INVALID_VALUE:
			error_str = "INVALID_VALUE";
			break;
			case GL_OUT_OF_MEMORY:
			error_str = "OUT_OF_MEMORY";
			break;
			case GL_INVALID_FRAMEBUFFER_OPERATION:
			error_str = "INVALID_FRAMEBUFFER_OPERATION";
			break;
		}

		fprintf(stderr, "OpenGL: %s", error_str);
		error = glGetError();
	}

	return true;
}

vec2 add(vec2 a, vec2 b) { return { a.x + b.x, a.y + b.y }; }
vec2 sub(vec2 a, vec2 b) { return { a.x - b.x, a.y - b.y }; }
bool operator==(const vec2& a, const vec2& b) { return a.x == b.x && a.y == b.y; }

Texture::Texture()
{

}

Texture::~Texture()
{
	if (id != 0) glDeleteTextures(1, &id);
}

bool Texture::load_from_file(const char* path)
{
	if (path == nullptr) 
		return false;
	
	stbi_uc* data = stbi_load(path, &width, &height, NULL, 4);
	if (data == NULL)
		return false;

	gl_flush_errors();
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	stbi_image_free(data);
	return !gl_has_errors();
}

bool Texture::is_valid() const
{
	return id != 0;
}

namespace
{
	bool gl_compile_shader(GLuint shader)
	{
		glCompileShader(shader);
		GLint success = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (success == GL_FALSE)
		{
			GLint log_len;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
			std::vector<char> log(log_len);
			glGetShaderInfoLog(shader, log_len, &log_len, log.data());
			glDeleteShader(shader);

			fprintf(stderr, "GLSL: %s", log.data());
			return false;
		}

		return true;
	}
}

bool Shaders::load_from_file(const char* vs_path, const char* fs_path) 
{
	gl_flush_errors();

	// Opening files
	std::ifstream vs_is(vs_path);
	std::ifstream fs_is(fs_path);

	if (!vs_is.good() || !fs_is.good())
	{
		fprintf(stderr, "Failed to load shader files %s, %s", vs_path, fs_path);
		return false;
	}

	// Reading sources
	std::stringstream vs_ss, fs_ss;
	vs_ss << vs_is.rdbuf();
	fs_ss << fs_is.rdbuf();
	std::string vs_str = vs_ss.str();
	std::string fs_str = fs_ss.str();
	const char* vs_src = vs_str.c_str();
	const char* fs_src = fs_str.c_str();
	GLsizei vs_len = (GLsizei)vs_str.size();
	GLsizei fs_len = (GLsizei)fs_str.size();

	vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &vs_src, &vs_len);
	fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &fs_src, &fs_len);

	// Compiling
	// Shaders already delete if compilation fails
	if (!gl_compile_shader(vertex_shader))
		return false;

	if (!gl_compile_shader(fragment_shader))
	{
		glDeleteShader(vertex_shader);
		return false;
	}

	// Linking
	program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);
	{
		GLint is_linked = 0;
		glGetProgramiv(program, GL_LINK_STATUS, &is_linked);
		if (is_linked == GL_FALSE)
		{
			GLint log_len;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
			std::vector<char> log(log_len);
			glGetProgramInfoLog(program, log_len, &log_len, log.data());

			release();
			fprintf(stderr, "Link error: %s", log.data());
			return false;
		}
	}

	if (gl_has_errors())
	{
		release();
		fprintf(stderr, "OpenGL errors occured while compiling Effect");
		return false;
	}

	return true;
}

void Shaders::release()
{
	glDeleteProgram(program);
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
}