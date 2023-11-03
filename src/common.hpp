#pragma once

// stlib
#include <fstream> // stdout, stderr..

// glfw
#define NOMINMAX
#include <gl3w.h>
#include <GLFW/glfw3.h>

// Simple utility macros to avoid mistyping directory name, name has to be a string literal
// audio_path("audio.ogg") -> data/audio/audio.ogg
// Get defintion of PROJECT_SOURCE_DIR from:
#include "project_path.hpp"

#define shader_path(name) PROJECT_SOURCE_DIR "./shaders/" name

#define data_path PROJECT_SOURCE_DIR "./data"
#define policies_path(name) 	data_path "/policies/" + name
#define levels_path(name) 		data_path "/levels/" + name
#define textures_path(name)  	data_path "/textures/" name
#define audio_path(name) 		data_path  "/audio/" name

// Not much math is needed and there are already way too many libraries linked (:
// If you want to do some overloads..
struct vec2 { float x, y; };
struct vec3 { float x, y, z; };
struct mat3 { vec3 c0, c1, c2; };

// Utility functions
vec2 add(vec2 a, vec2 b);
vec2 sub(vec2 a, vec2 b);
bool operator==(const vec2& a, const vec2& b);

// OpenGL utilities
// cleans error buffer
void gl_flush_errors();
bool gl_has_errors();

// Single Vertex Buffer element for textured sprites (textured.vs.glsl)
struct TexturedVertex
{
	vec2 position;
	vec2 texcoord;
};

// Texture wrapper
struct Texture
{
	Texture();
	~Texture();

	GLuint id;
	int width;
	int height;
	
	// Loads texture from file specified by path
	bool load_from_file(const char* path);
	bool is_valid() const; // True if texture is valid
};

struct Mesh {
	GLuint buffer_vao;
	GLuint buffer_vbo;
	GLuint buffer_ibo;
};

struct Shaders {
	GLuint vertex_shader;
	GLuint fragment_shader;
	GLuint program;

	bool load_from_file(const char* vs_path, const char* fs_path); // load shaders from files and link into program
	void release(); // release shaders and program
};
