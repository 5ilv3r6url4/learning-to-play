#pragma once

// internal
#include "common.hpp"
#include "grid_state.hpp"
#include "hero.hpp"
#include "enemy.hpp"

// stdlib
#include <string.h>
#include <stdlib.h>
#include <cassert>
#include <sstream>
#include <fstream>
#include <iostream>
#include <vector>
#include <random>
#include <iterator>
#include <time.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_mixer.h>

// Container for all our entities and game logic. Individual rendering / update is 
// deferred to the relative update() methods
class Grid_World
{
	static Texture WORLD_TEXTURE;

public:
	Grid_World();
	~Grid_World();

	// Creates a window, sets up events and begins the game
	bool init(std::string filename_level, std::string algo, int enemy_type, std::vector<int> hero_pos, std::vector<int> enemy_pos, bool flag = false);

	// Loads policy
	bool load_policy(std::string filepath_policy);

	// Loads level
	bool load_level(std::string filename_level, std::vector<int> hero_pos, std::vector<int> enemy_pos, bool flag);

	// Releases all associated resources
	void destroy();

	// Steps the game enemy and world
	bool update();
	bool update(int64_t action);

	// Renders our scene
	void draw();

	// Should the game be over ?
	bool is_over() const;

	void reset();

	std::vector<int64_t> extract_state();

	int m_enemy_type;

	int m_points;

	int m_rows;
	int m_cols;

	std::string m_level_name;

private:
	void on_key(GLFWwindow*, int key, int, int action, int mod);

	int count_rows(std::string filename_level);
	int count_cols(std::string filename_level);

private:
	GLFWwindow* m_window;
	float m_screen_scale; 

	Grid_State**	m_grid_states;
	Hero* 			m_hero;
	Enemy* 			m_enemy;
	bool 			m_is_over;

	std::vector<int> m_hero_init_pos;
	std::vector<int> m_enemy_init_pos;
	std::vector<std::vector<std::vector<std::vector<std::vector<int>>>>> m_policy;
	
	Mix_Music* 		m_background_music;
	Mix_Chunk* 		m_lose_game;
	Mix_Chunk* 		m_win_game;
	Mix_Chunk* 		m_lose_points;
	Mix_Chunk* 		m_win_points;
	Texture 		m_world_texture;
};
