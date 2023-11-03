#pragma once

#include "common.hpp"

class Enemy
{
public:
	Enemy();
	~Enemy();

	bool init(int row, int col, int tex_row, int tex_col);
	void draw(const mat3& projection, GLuint texture_id);
	void destroy();

	Mesh m_mesh;
	Shaders m_shaders;
	vec2 m_grid_position;

	int m_action;

};