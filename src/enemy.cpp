// Header
#include "enemy.hpp"

#include <cmath>

Enemy::Enemy() { } 
Enemy::~Enemy() { }

bool Enemy::init(int row, int col, int tex_row, int tex_col)
{
	TexturedVertex vertices[4];
	vertices[0].position = { 0.f,  50.f };
	vertices[1].position = { 50.f, 50.f };
	vertices[2].position = { 50.f, 0.f };
	vertices[3].position = { 0.f,  0.f };
	vertices[0].texcoord = { 0.f 	  + tex_row * (1.f/16.f), 	1.f/8.f 	+ tex_col * (1.f/8.f)};
	vertices[1].texcoord = { 1.f/16.f + tex_row * (1.f/16.f), 	1.f/8.f 	+ tex_col * (1.f/8.f)};
	vertices[2].texcoord = { 1.f/16.f + tex_row * (1.f/16.f), 	0.f 		+ tex_col * (1.f/8.f)};
	vertices[3].texcoord = { 0.f 	  + tex_row * (1.f/16.f), 	0.f 	  	+ tex_col * (1.f/8.f)};

	// Counterclockwise as it's the default opengl front winding direction
	uint16_t indices[] = { 0, 3, 1, 1, 3, 2 };

	// Clearing errors
	gl_flush_errors();
	
	// Vertex Buffer creation
	glGenBuffers(1, &m_mesh.buffer_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, m_mesh.buffer_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(TexturedVertex) * 4, vertices, GL_STATIC_DRAW);

	// Index Buffer creation
	glGenBuffers(1, &m_mesh.buffer_ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_mesh.buffer_ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint16_t) * 6, indices, GL_STATIC_DRAW);

	// Vertex Array (Container for Vertex + Index buffer)
	glGenVertexArrays(1, &m_mesh.buffer_vao);
	if (gl_has_errors()) {
		return false;
	}

	if (!m_shaders.load_from_file(shader_path("textured.vs.glsl"), shader_path("textured.fs.glsl"))) {
		fprintf(stderr, "Failed to load textured shaders!");
		return false; 
	}

	m_grid_position = {(float)row, (float)col};
	m_action = 2;

	return true;
}

// Releases all graphics resources
void Enemy::destroy()
{
	glDeleteBuffers(1, &m_mesh.buffer_vbo);
	glDeleteBuffers(1, &m_mesh.buffer_ibo);
	glDeleteBuffers(1, &m_mesh.buffer_vao);

	glDeleteShader(m_shaders.vertex_shader);
	glDeleteShader(m_shaders.fragment_shader);
	glDeleteProgram(m_shaders.program);
}

void Enemy::draw(const mat3& projection, GLuint texture_id)
{
	// Setting shaders
	glUseProgram(m_shaders.program);

	// Enabling alpha channel for textures
	glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);

	// Getting uniform locations for glUniform* calls
	GLint projection_uloc = glGetUniformLocation(m_shaders.program, "projection");
	GLint grid_position_uloc = glGetUniformLocation(m_shaders.program, "grid_position");

	// Setting vertices and indices
	glBindVertexArray(m_mesh.buffer_vao);
	glBindBuffer(GL_ARRAY_BUFFER, m_mesh.buffer_vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_mesh.buffer_ibo);

	// Input data location as in the vertex buffer
	GLint in_position_loc = glGetAttribLocation(m_shaders.program, "in_position");
	GLint in_texcoord_loc = glGetAttribLocation(m_shaders.program, "in_texcoord");
	glEnableVertexAttribArray(in_position_loc);
	glEnableVertexAttribArray(in_texcoord_loc);
	glVertexAttribPointer(in_position_loc, 2, GL_FLOAT, GL_FALSE, sizeof(TexturedVertex), (void*)0);
	glVertexAttribPointer(in_texcoord_loc, 2, GL_FLOAT, GL_FALSE, sizeof(TexturedVertex), (void*)sizeof(vec2));

	// Enabling and binding texture to slot 0
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


	// Setting uniform values to the currently bound program
	glUniformMatrix3fv(projection_uloc, 1, GL_FALSE, (float*)&projection);
	glUniform2f(grid_position_uloc, m_grid_position.x, m_grid_position.y);

	// Drawing!
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
}