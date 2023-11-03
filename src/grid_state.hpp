#pragma once

#include "common.hpp"

class Hero;
class Enemy; 

class Grid_State
{

public:
	Grid_State();
	~Grid_State();

	bool init(int row, int col, int tex_row, int tex_col, bool obstacle);
	void draw(const mat3& projection, GLuint texture_id);
	void destroy();

	bool m_hero;
	bool m_enemy;
	bool m_obstacle;

	Mesh m_mesh;
	Shaders m_shaders;
	vec2 m_grid_position;
};