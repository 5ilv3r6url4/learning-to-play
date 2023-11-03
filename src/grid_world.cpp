// Header
#include "grid_world.hpp"

namespace
{
	void glfw_err_cb(int error, const char* desc)
	{
		fprintf(stderr, "%d: %s", error, desc);
	}
}

int PENALTY_ENEMY_COLLISION = -100;
int PENALTY_ENEMY_ATTACK 	= -200;
int PENALTY_ENEMY_GUARD 	= -100;
int REWARD_HERO_ATTACK 		= 100;
int REWARD_HERO_GUARD 		= 50;

Texture Grid_World::WORLD_TEXTURE;

Grid_World::Grid_World() : m_points(0) { }
Grid_World::~Grid_World() { }

// World initialization
bool Grid_World::init(std::string filename_level, std::string algo, int enemy_type, std::vector<int> hero_pos, std::vector<int> enemy_pos, bool flag)
{
	srand(time(NULL));
	m_is_over = false;

	m_level_name = filename_level.substr(0, filename_level.size()-4);
	m_enemy_type = enemy_type;
	// std::cout << enemy_type << "\n";

	m_rows = count_rows(filename_level);
	m_cols = count_cols(filename_level);

	vec2 screen = { 50.f * (float)m_cols, 50.f * (float)m_rows};

	m_hero = new Hero();
	m_enemy = new Enemy();
	m_points = 0;

	m_hero_init_pos = hero_pos;
	m_enemy_init_pos = enemy_pos;

	if (enemy_type == 1) {
		std::string filepath_policy = std::string("bat-") + m_level_name + std::string("-") + algo + std::string("_policy.txt");
		load_policy(policies_path(std::string(filepath_policy)));
	}
	else if (enemy_type == 2) {
		std::string filepath_policy = std::string("skeleton-") + m_level_name + std::string("-") + algo + std::string("_policy.txt");
		load_policy(policies_path(std::string(filepath_policy)));
	}

	if (flag) {
		//-------------------------------------------------------------------------
		// GLFW / OGL Initialization
		// Core Opengl 3.
		glfwSetErrorCallback(glfw_err_cb);
		if (!glfwInit())
		{
			fprintf(stderr, "Failed to initialize GLFW");
			return false;
		}

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1);
	#if __APPLE__
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	#endif
		glfwWindowHint(GLFW_RESIZABLE, 0);
		m_window = glfwCreateWindow((int)screen.x, (int)screen.y, "Game", nullptr, nullptr);
		if (m_window == nullptr)
			return false;

		glfwMakeContextCurrent(m_window);
		glfwSwapInterval(1); // vsync

		// Load OpenGL function pointers
		gl3w_init();

		// Setting callbacks to member functions (that's why the redirect is needed)
		// Input is handled using GLFW, for more info see
		// http://www.glfw.org/docs/latest/input_guide.html
		glfwSetWindowUserPointer(m_window, this);
		auto key_redirect = [](GLFWwindow* wnd, int _0, int _1, int _2, int _3) { ((Grid_World*)glfwGetWindowUserPointer(wnd))->on_key(wnd, _0, _1, _2, _3); };
		glfwSetKeyCallback(m_window, key_redirect);

		// For some high DPI displays (ex. Retina Display on Macbooks)
		// https://stackoverflow.com/questions/36672935/why-retina-screen-coordinate-value-is-twice-the-value-of-pixel-value
		int fb_width, fb_height;
		glfwGetFramebufferSize(m_window, &fb_width, &fb_height);
		m_screen_scale = static_cast<float>(fb_width) / screen.x;

		//-------------------------------------------------------------------------
		// Loading music and sounds
		if (SDL_Init(SDL_INIT_AUDIO) < 0) {
			fprintf(stderr, "Failed to initialize SDL Audio");
			return false;
		}

		if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == -1) {
			fprintf(stderr, "Failed to open audio device");
			return false;
		}

		m_background_music = Mix_LoadMUS(audio_path("music.wav"));
		m_lose_game = Mix_LoadWAV(audio_path("game_over.wav"));
		m_win_game = Mix_LoadWAV(audio_path("game_win.wav"));
		m_lose_points = Mix_LoadWAV(audio_path("hero_damaged.wav"));
		m_win_points = Mix_LoadWAV(audio_path("enemy_damaged.wav"));

		if (m_background_music == nullptr || m_lose_game == nullptr || m_win_game == nullptr || m_lose_points == nullptr || m_win_points == nullptr) {
			fprintf(stderr, "Failed to load sounds\n %s\n %s\n %s\n %s\n %s\n make sure the data directory is present",
				audio_path("music.wav"),
				audio_path("game_over.wav"),
				audio_path("game_win.wav"),
				audio_path("hero_damaged.wav"),
				audio_path("enemy_damaged.wav"));

			return false;
		}

		// Playing background music indefinitely
		Mix_PlayMusic(m_background_music, -1);

		// Load shared texture
		if (!WORLD_TEXTURE.load_from_file(textures_path("tileset_1bit.png"))) {
			fprintf(stderr, "Failed to load world texture!");
			return false;
		}

		if (!m_hero->init(hero_pos[1], hero_pos[0], 5, 3)) {
			fprintf(stderr, "Failed to initialize hero!");
			return false;
		}

		if (enemy_type == 0) {
			if (!m_enemy->init(enemy_pos[1], enemy_pos[0], 3, 3)) {
				fprintf(stderr, "Failed to initialize enemy!");
				return false;
			}
		}
		else if (enemy_type == 1) {
			if (!m_enemy->init(enemy_pos[1], enemy_pos[0], 4, 5)) {
				fprintf(stderr, "Failed to initialize enemy!");
				return false;
			}
		}
		else if (enemy_type == 2) {
			if (!m_enemy->init(enemy_pos[1], enemy_pos[0], 12, 5)) {
				fprintf(stderr, "Failed to initialize enemy!");
				return false;
			}
		}
	}

	else {
		m_hero->m_action = -1;
		m_enemy->m_action = 2;
		m_hero->m_grid_position = {(float)hero_pos[1], (float)hero_pos[0]};
		m_enemy->m_grid_position = {(float)enemy_pos[1], (float)enemy_pos[0]};
	}

	if (!load_level(filename_level, hero_pos, enemy_pos, flag)) {
		fprintf(stderr, "Failed to load level!");
		return false;
	}
	
	return true;
}

bool Grid_World::load_policy(std::string filepath_policy) 
{
    std::vector<std::vector<std::vector<std::vector<std::vector<int>>>>> temp_vec(m_rows, std::vector<std::vector<std::vector<std::vector<int>>>>
                                           										 (m_cols, std::vector<std::vector<std::vector<int>>>
                                           										 (m_rows, std::vector<std::vector<int>>
                                           										 (m_cols, std::vector<int>
                                           										 (13)))));
	m_policy = temp_vec;

	std::ifstream file_policy(filepath_policy);
	std::string policy_line; 
	while (std::getline(file_policy, policy_line)) {
		std::vector<std::string> parsed(6);
		std::stringstream policy(policy_line);
		std::string hero_row;
		std::string hero_col;
		std::string enemy_row;
		std::string enemy_col;
		// flip the semantics as we are applying the hero policy to the enemy
		std::string hero_act;
		std::string enemy_act;
		// --
		getline(policy, hero_row, ','); 
		parsed[0] = hero_row;
		// --
		getline(policy, hero_col, ','); 
		parsed[1] = hero_col;
		// --
		getline(policy, enemy_row, ','); 
		parsed[2] = enemy_row;
		// --
		getline(policy, enemy_col, ','); 
		parsed[3] = enemy_col;
		// --
		std::string action_elems;
		getline(policy, action_elems, ','); 
		std::stringstream action(action_elems);

		getline(action, hero_act, '='); 
		parsed[4] = hero_act;
		// --
		getline(action, enemy_act, '='); 
		parsed[5] = enemy_act;
		// --

		m_policy[std::stoi(parsed[0])][std::stoi(parsed[1])][std::stoi(parsed[2])][std::stoi(parsed[3])][std::stoi(parsed[4])] = std::stoi(parsed[5]);
	}
	return true;
}

bool Grid_World::load_level(std::string filename_level, std::vector<int> hero_pos, std::vector<int> enemy_pos, bool flag)
{
	m_grid_states = new Grid_State*[m_rows];
	for (int i = 0; i < m_rows; ++i) {
		m_grid_states[i] = new Grid_State[m_cols];
	}

	std::string row_string;
	std::string block_string;
	std::fstream infile;
	infile.open(levels_path(filename_level));
	int row = 0;
		while (!infile.eof()) {
			int col = 0;
			getline(infile, row_string); 
			std::istringstream iss(row_string);
			while(iss >> block_string) {
				if (flag) {
					int col_tex = std::stoi(block_string.substr(0, 2));
					int row_tex = std::stoi(block_string.substr(3, 2));
					m_grid_states[row][col].init(row, col, row_tex, col_tex, block_string.substr(2,1) == "/");
				}
				else {
					m_grid_states[row][col].m_grid_position = {(float)row, (float)col};
					m_grid_states[row][col].m_obstacle = (block_string.substr(2,1) == "/");
				}
				col++;
			}
			row++;
		}

	infile.close();
	m_grid_states[hero_pos[1]][hero_pos[0]].m_hero = true;
	m_grid_states[enemy_pos[1]][enemy_pos[0]].m_enemy = true;
	return true;
}

int Grid_World::count_rows(std::string filename_level) {
	std::string row_string;
	std::fstream infile;
	infile.open(levels_path(filename_level));
	infile.unsetf(std::ios_base::skipws);
	int rows = std::count(std::istream_iterator<char>(infile), std::istream_iterator<char>(), '\n');
	return rows + 1;
}

int Grid_World::count_cols(std::string filename_level) {
	std::string row_string;
	std::fstream infile;
	infile.open(levels_path(filename_level));
	getline(infile, row_string); 
	int cols = std::count(row_string.begin(), row_string.end(), ' ');
	return cols + 1;
}

// Releases all the associated resources
void Grid_World::destroy()
{
	if (m_background_music != nullptr) {
		Mix_FreeMusic(m_background_music);
	}
	if (m_win_game != nullptr) {
		Mix_FreeChunk(m_win_game);
	}
	if (m_lose_game != nullptr) {
		Mix_FreeChunk(m_lose_game);
	}
	if (m_lose_points != nullptr) {
		Mix_FreeChunk(m_lose_points);
	}
	if (m_win_points != nullptr) {
		Mix_FreeChunk(m_win_points);
	}
	
	Mix_CloseAudio();

	delete m_hero;
	delete m_enemy;
	delete[] m_grid_states;

	glfwDestroyWindow(m_window);
}

// Update our game world
bool Grid_World::update()
{
	//////////////////
	// ACTION SPACE //
	//////////////////

	// 0 - DO NOTHING
	// 1 - MOVE UP
	// 2 - MOVE LEFT
	// 3 - MOVE DOWN
	// 4 - MOVE RIGHT
	// 5 - ATTACK UP
	// 6 - ATTACK LEFT
	// 7 - ATTACK DOWN
	// 8 - ATTACK RIGHT
	// 9 - GUARD UP
	// 10 - GUARD LEFT
	// 11 - GUARD DOWN
	// 12 - GUARD RIGHT
	
	vec2 cur_grid_position_hero  = { m_hero->m_grid_position.x, m_hero->m_grid_position.y };
	vec2 cur_grid_position_enemy = { m_enemy->m_grid_position.x, m_enemy->m_grid_position.y };

	vec2 new_grid_position_hero  = { -1.f, -1.f };
	vec2 new_grid_position_enemy = { -1.f, -1.f };

	switch (m_enemy_type) {

		case 0: { // bat enemy

			vec2 enemy_direction = { -1.f, -1.f };

			switch (m_hero->m_action) {
				case 0: { // hero do nothing
					vec2 hero_direction = { 0.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 1: { // enemy moves up
							// std::cout << "hero do nothing - enemy move up" << std::endl;
							enemy_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero do nothing - enemy move left" << std::endl;
							enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero do nothing - enemy move down" << std::endl;
							enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero do nothing - enemy move right" << std::endl;
						    enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						default: { // bat enemy attacks and guards
							// std::cout << "hero do nothing - bat default" << std::endl;
							break;
						}
					}
					break;
				}
				case 1: { // hero move up
					vec2 hero_direction = { -1.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
						new_grid_position_hero = cur_grid_position_hero;
					}
					switch (m_enemy->m_action) {
						case 1: { // enemy moves up
							// std::cout << "hero move up - enemy move up" << std::endl;
							enemy_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero move up - enemy move left" << std::endl;
							enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero move up - enemy move down" << std::endl;
							enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero move up - enemy move right" << std::endl;
							enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						default: { // bat enemy attacks and guards
							// std::cout << "hero move up - bat default" << std::endl;
							break;
						}
					}
					break;
				}
				case 2: { // hero move left
					vec2 hero_direction = { 0.f, -1.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
						new_grid_position_hero = cur_grid_position_hero;
					}
					switch (m_enemy->m_action) {
						case 1: { // enemy moves up
							// std::cout << "hero move left - enemy move up" << std::endl;
							enemy_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero move left - enemy move left" << std::endl;
							enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero move left - enemy move down" << std::endl;
							enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero move left - enemy move right" << std::endl;
							enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						default: { // bat enemy attacks and guards
							// std::cout << "hero move left - bat default" << std::endl;
							break;
						}
					}
					break;
				}
				case 3: { // hero move down
					vec2 hero_direction = { 1.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
						new_grid_position_hero = cur_grid_position_hero;
					}
					switch (m_enemy->m_action) {
						case 1: { // enemy moves up
							// std::cout << "hero move down - enemy move up" << std::endl;
							enemy_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero move down - enemy move left" << std::endl;
							enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero move down - enemy move down" << std::endl;
							enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero move down - enemy move right" << std::endl;
							enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						default: { // bat enemy attacks and guards
							// std::cout << "hero move down - bat default" << std::endl;
							break;
						}
					}
					break;
				}
				case 4: { // hero move right
					vec2 hero_direction = { 0.f, 1.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
						new_grid_position_hero = cur_grid_position_hero;
					}
					switch (m_enemy->m_action) {
						case 1: { // enemy moves up
							// std::cout << "hero move right - enemy move up" << std::endl;
							enemy_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero move right - enemy move left" << std::endl;
							enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero move right - enemy move down" << std::endl;
							enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero move right - enemy move right" << std::endl;
							enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						default: { // bat enemy attacks and guards
							// std::cout << "hero move right - bat default" << std::endl;
							break;
						}
					}
					break;
				}
				case 5: { // hero attack up
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_attack_direction = { -1.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 1: { // enemy moves up
							// std::cout << "hero attack up - enemy move up" << std::endl;
							enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

									int choose = rand() % 2; 
									if (choose == 0) {
										enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
									}
									else {
										enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
									}
									new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero attack up - enemy move left" << std::endl;
							enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

									int choose = rand() % 2; 
									if (choose == 0) {
										enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
									}
									else {
										enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
									}
									new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero attack up - enemy move down" << std::endl;
							enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

									int choose = rand() % 2; 
									if (choose == 0) {
										enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
									}
									else {
										enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
									}
									new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero attack up - enemy move right" << std::endl;
							enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

									int choose = rand() % 2; 
									if (choose == 0) {
										enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
									}
									else {
										enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
									}
									new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						default: { // bat enemy attacks and guards
							// std::cout << "hero attack up - bat default" << std::endl;
							break;
						}
					}
					break;
				}
				case 6: { // hero attack left
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_attack_direction = { 0.f, -1.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 1: { // enemy moves up
							// std::cout << "hero attack left - enemy move up" << std::endl;
							enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

									int choose = rand() % 2; 
									if (choose == 0) {
										enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
									}
									else {
										enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
									}
									new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero attack left - enemy move left" << std::endl;
							enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

									int choose = rand() % 2; 
									if (choose == 0) {
										enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
									}
									else {
										enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
									}
									new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero attack left - enemy move down" << std::endl;
							enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

									int choose = rand() % 2; 
									if (choose == 0) {
										enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
									}
									else {
										enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
									}
									new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero attack left - enemy move right" << std::endl;
							enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

									int choose = rand() % 2; 
									if (choose == 0) {
										enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
									}
									else {
										enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
									}
									new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						default: { // bat enemy attacks and guards
							// std::cout << "hero attack left - bat default" << std::endl;
							break;
						}
					}
					break;
				}
				case 7: { // hero attack down
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_attack_direction = { 1.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 1: { // enemy moves up
							// std::cout << "hero attack down - enemy move up" << std::endl;
							enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

									int choose = rand() % 2; 
									if (choose == 0) {
										enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
									}
									else {
										enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
									}
									new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero attack down - enemy move left" << std::endl;
							enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

									int choose = rand() % 2; 
									if (choose == 0) {
										enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
									}
									else {
										enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
									}
									new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero attack down - enemy move down" << std::endl;
							enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

									int choose = rand() % 2; 
									if (choose == 0) {
										enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
									}
									else {
										enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
									}
									new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero attack down - enemy move right" << std::endl;
							enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

									int choose = rand() % 2; 
									if (choose == 0) {
										enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
									}
									else {
										enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
									}
									new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						default: { // bat enemy attacks and guards
							// std::cout << "hero attack down - bat default" << std::endl;
							break;
						}
					}
					break;
				}
				case 8: { // hero attack right
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_attack_direction = { 0.f, 1.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 1: { // enemy moves up
							// std::cout << "hero attack right - enemy move up" << std::endl;
							enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

									int choose = rand() % 2; 
									if (choose == 0) {
										enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
									}
									else {
										enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
									}
									new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero attack right - enemy move left" << std::endl;
							enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

									int choose = rand() % 2; 
									if (choose == 0) {
										enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
									}
									else {
										enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
									}
									new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero attack right - enemy move down" << std::endl;
							enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

									int choose = rand() % 2; 
									if (choose == 0) {
										enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
									}
									else {
										enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
									}
									new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero attack right - enemy move right" << std::endl;
							enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

									int choose = rand() % 2; 
									if (choose == 0) {
										enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
									}
									else {
										enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
									}
									new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						default: { // bat enemy attacks and guards
							// std::cout << "hero attack right - bat default" << std::endl;
							break;
						}
					}
					break;
				}
				case 9: { // hero guard up
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_guard_direction = { -1.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 1: { // enemy moves up
							// std::cout << "hero guard up - enemy move up" << std::endl;
							enemy_direction = { -1.f, 0.f };	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}

							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero guard up - enemy move left" << std::endl;
							enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}
							
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero guard up - enemy move down" << std::endl;
							enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}
							
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero guard up - enemy move right" << std::endl;
							enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}
							
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						default: { // bat enemy attacks and guards
							// std::cout << "hero guard up - bat default" << std::endl;
							break;
						}
					}
					break;
				}
				case 10: { // hero guard left
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_guard_direction = { 0.f, -1.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 1: { // enemy moves up
							// std::cout << "hero guard left - enemy move up" << std::endl;
							enemy_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}
							
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero guard left - enemy move left" << std::endl;
							enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}
							
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero guard left - enemy move down" << std::endl;
							enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}
							
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero guard left - enemy move right" << std::endl;
							enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}
							
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						default: { // bat enemy attacks and guards
							// std::cout << "hero guard left - bat default" << std::endl;
							break;
						}
					}
					break;
				}
				case 11: { // hero guard down
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_guard_direction = { 1.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 1: { // enemy moves up
							// std::cout << "hero guard down - enemy move up" << std::endl;
							enemy_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}
							
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero guard down - enemy move left" << std::endl;
							enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}
							
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero guard down - enemy move down" << std::endl;
							enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}
							
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero guard down - enemy move right" << std::endl;
							enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}
							
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						default: { // bat enemy attacks and guards
							// std::cout << "hero guard down - bat default" << std::endl;
							break;
						}
					}
					break;
				}
				case 12: { // hero guard right
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_guard_direction = { 0.f, 1.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 1: { // enemy moves up
							// std::cout << "hero guard down - enemy move up" << std::endl;
							enemy_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}
							
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero guard down - enemy move left" << std::endl;
							enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}
							
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero guard down - enemy move down" << std::endl;
							enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}
							
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero guard down - enemy move right" << std::endl;
							enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							while (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								vec2 enemy_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 enemy_direction_cw = { enemy_direction.y, -enemy_direction.x };

								int choose = rand() % 2; 
								if (choose == 0) {
									enemy_direction = { enemy_direction_ccw.x, enemy_direction_ccw.y };
								}
								else {
									enemy_direction = { enemy_direction_cw.x, enemy_direction_cw.y };
								}
								new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							}
							
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						default: { // bat enemy attacks and guards
							// std::cout << "hero guard right - bat default" << std::endl;
							break;
						}
					}
					break;
				}
				default: break;
			}
			if (enemy_direction.x == 0.f) {
				if (enemy_direction.y == -1.f) m_enemy->m_action = 2;
				if (enemy_direction.y == 1.f)  m_enemy->m_action = 4;
			}
			else if (enemy_direction.y == 0.f) {
				if (enemy_direction.x == -1.f) m_enemy->m_action = 1;
				if (enemy_direction.x == 1.f)  m_enemy->m_action = 3;
			}
			break;
		}

		case 1: { // skeleton enemy

			switch (m_hero->m_action) {
				case 0: { // hero do nothing
					vec2 hero_direction = { 0.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero do nothing - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero do nothing - enemy move up" << std::endl;
							vec2 enemy_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero do nothing - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero do nothing - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero do nothing - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero do nothing - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero do nothing - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero do nothing - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero do nothing - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero do nothing - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero do nothing - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero do nothing - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero do nothing - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
					}
					break;
				}
				case 1: { // hero move up
					vec2 hero_direction = { -1.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
						new_grid_position_hero = cur_grid_position_hero;
					}
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero move up - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero move up - enemy move up" << std::endl;
							vec2 enemy_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero move up - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero move up - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero move up - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero move up - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero move up - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero move up - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero move up - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };				
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero move up - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { -1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero move up - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, -1.f };
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero move up - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 1.f, 0.f };	
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero move up - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, 1.f };		
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
					}
					break;
				}
				case 2: { // hero move left
					vec2 hero_direction = { 0.f, -1.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
						new_grid_position_hero = cur_grid_position_hero;
					}
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero move left - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero move left - enemy move up" << std::endl;
							vec2 enemy_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero move left - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero move left - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero move left - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero move left - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero move left - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero move left - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero move left - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };				
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero move left - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { -1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero move left - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, -1.f };
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero move left - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 1.f, 0.f };		
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero move left - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, 1.f };		
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
					}
					break;
				}
				case 3: { // hero move down
					vec2 hero_direction = { 1.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
						new_grid_position_hero = cur_grid_position_hero;
					}
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero move down - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero move down - enemy move up" << std::endl;
							vec2 enemy_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero move down - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero move down - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero move down - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero move down - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero move down - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero move down - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero move down - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };				
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero move down - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { -1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero move down - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, -1.f };
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero move down - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 1.f, 0.f };		
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero move down - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, 1.f };		
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
					}
					break;
				}
				case 4: { // hero move right
					vec2 hero_direction = { 0.f, 1.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
						new_grid_position_hero = cur_grid_position_hero;
					}
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero move right - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero move right - enemy move up" << std::endl;
							vec2 enemy_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero move right - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero move right - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero move right - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero move right - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero move right - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero move right - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero move right - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };				
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero move right - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { -1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero move right - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, -1.f };
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero move right - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 1.f, 0.f };		
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero move right - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, 1.f };		
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
					}
					break;
				}
				case 5: { // hero attack up
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_attack_direction = { -1.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero attack up - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero attack up - enemy move up" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero attack up - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero attack up - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero attack up - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero attack up - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero attack up - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero attack up - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero attack up - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero attack up - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero attack up - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero attack up - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero attack up - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
					}
					break;
				}
				case 6: { // hero attack left
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_attack_direction = { 0.f, -1.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero attack left - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero attack left - enemy move up" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero attack left - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero attack left - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero attack left - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero attack left - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero attack left - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero attack left - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero attack left - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero attack left - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero attack left - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero attack left - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero attack left - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
					}
					break;
				}
				case 7: { // hero attack down
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_attack_direction = { 1.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero attack down - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero attack down - enemy move up" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero attack down - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero attack down - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero attack down - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero attack down - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero attack down - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero attack down - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero attack down - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero attack down - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero attack down - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero attack down - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero attack down - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
					}
					break;
				}
				case 8: { // hero attack right
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_attack_direction = { 0.f, 1.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero attack right - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero attack right - enemy move up" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero attack right - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero attack right - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero attack right - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero attack right - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero attack right - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero attack right - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero attack right - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero attack right - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero attack right - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero attack right - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero attack right - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
					}
					break;
				}
				case 9: { // hero guard up
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_guard_direction = { -1.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero guard up - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero guard up - enemy move up" << std::endl;
							vec2 enemy_direction = { -1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero guard up - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero guard up - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero guard up - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero guard up - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero guard up - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero guard up - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero guard up - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero guard up - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero guard up - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero guard up - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero guard up - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
					}
					break;
				}
				case 10: { // hero guard left
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_guard_direction = { 0.f, -1.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero guard left - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero guard left - enemy move up" << std::endl;
							vec2 enemy_direction = { -1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero guard left - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero guard left - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero guard left - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero guard left - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero guard left - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero guard left - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero guard left - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero guard left - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero guard left - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero guard left - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero guard left - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
					}
					break;
				}
				case 11: { // hero guard down
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_guard_direction = { 1.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero guard down - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero guard down - enemy move up" << std::endl;
							vec2 enemy_direction = { -1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero guard down - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero guard down - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero guard down - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero guard down - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero guard down - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero guard down - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero guard down - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero guard down - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero guard down - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero guard down - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero guard down - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
					}
					break;
				}
				case 12: { // hero guard right
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_guard_direction = { 0.f, 1.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero guard down - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero guard down - enemy move up" << std::endl;
							vec2 enemy_direction = { -1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero guard down - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero guard down - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero guard down - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero guard down - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero guard down - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero guard down - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero guard down - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero guard down - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero guard down - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero guard down - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero guard down - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
					}
					break;
				}
				default: break;
			}
			m_enemy->m_action = m_policy[(int)m_hero->m_grid_position.y][(int)m_hero->m_grid_position.x][(int)m_enemy->m_grid_position.y][(int)m_enemy->m_grid_position.x][m_hero->m_action];
			break;
		}

		case 2: { // knight enemy

			switch (m_hero->m_action) {
				case 0: { // hero do nothing
					vec2 hero_direction = { 0.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero do nothing - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero do nothing - enemy move up" << std::endl;
							vec2 enemy_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero do nothing - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero do nothing - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero do nothing - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero do nothing - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero do nothing - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero do nothing - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero do nothing - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero do nothing - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero do nothing - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero do nothing - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero do nothing - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
					}
					break;
				}
				case 1: { // hero move up
					vec2 hero_direction = { -1.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
						new_grid_position_hero = cur_grid_position_hero;
					}
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero move up - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero move up - enemy move up" << std::endl;
							vec2 enemy_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero move up - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero move up - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero move up - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero move up - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero move up - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero move up - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero move up - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };				
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero move up - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { -1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero move up - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, -1.f };
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero move up - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 1.f, 0.f };	
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero move up - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, 1.f };		
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
					}
					break;
				}
				case 2: { // hero move left
					vec2 hero_direction = { 0.f, -1.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
						new_grid_position_hero = cur_grid_position_hero;
					}
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero move left - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero move left - enemy move up" << std::endl;
							vec2 enemy_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero move left - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero move left - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero move left - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero move left - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero move left - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero move left - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero move left - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };				
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero move left - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { -1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero move left - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, -1.f };
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero move left - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 1.f, 0.f };		
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero move left - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, 1.f };		
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
					}
					break;
				}
				case 3: { // hero move down
					vec2 hero_direction = { 1.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
						new_grid_position_hero = cur_grid_position_hero;
					}
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero move down - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero move down - enemy move up" << std::endl;
							vec2 enemy_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero move down - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero move down - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero move down - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero move down - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero move down - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero move down - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero move down - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };				
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero move down - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { -1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero move down - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, -1.f };
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero move down - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 1.f, 0.f };		
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero move down - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, 1.f };		
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
					}
					break;
				}
				case 4: { // hero move right
					vec2 hero_direction = { 0.f, 1.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
						new_grid_position_hero = cur_grid_position_hero;
					}
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero move right - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero move right - enemy move up" << std::endl;
							vec2 enemy_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero move right - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero move right - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero move right - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (new_grid_position_enemy == cur_grid_position_hero &&
								cur_grid_position_enemy == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
								vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
								vec2 new_grid_position_hero_pushed_ccw = add(cur_grid_position_hero, push_hero_direction_ccw);
								vec2 new_grid_position_hero_pushed_cw = add(cur_grid_position_hero, push_hero_direction_cw);
								if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
									new_grid_position_hero = new_grid_position_hero_pushed_ccw;
								}
								else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
									if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_cw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
										new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero move right - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero move right - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero move right - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero move right - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };				
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							else {
								if (new_grid_position_hero == new_grid_position_enemy) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero move right - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { -1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero move right - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, -1.f };
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero move right - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 1.f, 0.f };		
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero move right - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, 1.f };		
							vec2 hero_direction_enemy_guard = add(hero_direction, enemy_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += PENALTY_ENEMY_GUARD;
									if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
									new_grid_position_hero = cur_grid_position_hero;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									new_grid_position_hero = cur_grid_position_hero;
								}
							}
							break;
						}
					}
					break;
				}
				case 5: { // hero attack up
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_attack_direction = { -1.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero attack up - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero attack up - enemy move up" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero attack up - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero attack up - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero attack up - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero attack up - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero attack up - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero attack up - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero attack up - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero attack up - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero attack up - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero attack up - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero attack up - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
					}
					break;
				}
				case 6: { // hero attack left
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_attack_direction = { 0.f, -1.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero attack left - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero attack left - enemy move up" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero attack left - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero attack left - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero attack left - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero attack left - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero attack left - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero attack left - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero attack left - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero attack left - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero attack left - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero attack left - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero attack left - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
					}
					break;
				}
				case 7: { // hero attack down
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_attack_direction = { 1.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero attack down - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero attack down - enemy move up" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero attack down - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero attack down - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero attack down - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero attack down - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero attack down - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero attack down - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero attack down - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero attack down - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero attack down - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero attack down - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero attack down - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
					}
					break;
				}
				case 8: { // hero attack right
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_attack_direction = { 0.f, 1.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero attack right - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero attack right - enemy move up" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero attack right - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero attack right - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero attack right - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == cur_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							else {
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								if (new_grid_position_enemy == new_grid_position_hero) {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero attack right - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero attack right - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero attack right - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero attack right - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							if (add(enemy_attack_direction, cur_grid_position_enemy) == cur_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero attack right - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero attack right - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero attack right - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero attack right - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_guard_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_guard_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_GUARD;
								if (m_win_game != nullptr) {
										Mix_PlayChannel(-1, m_lose_points, 0);
										if (m_points < -1500) {
											Mix_PlayChannel(-1, m_lose_game, 0);
											reset();
										}
									}
								new_grid_position_hero = cur_grid_position_hero;
							}
							else if (add(hero_attack_direction, cur_grid_position_hero) == new_grid_position_enemy) {
								m_points += REWARD_HERO_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_win_points, 0);
								}
								new_grid_position_enemy = add(hero_attack_direction, cur_grid_position_enemy);
								if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
									vec2 hero_attack_direction_ccw = { -hero_attack_direction.y, hero_attack_direction.x };
									vec2 hero_attack_direction_cw = { hero_attack_direction.y, -hero_attack_direction.x };
									vec2 new_grid_position_enemy_ccw = add(hero_attack_direction_ccw, cur_grid_position_enemy);
									vec2 new_grid_position_enemy_cw = add(hero_attack_direction_cw, cur_grid_position_enemy);
									if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == false) {
										new_grid_position_enemy = new_grid_position_enemy_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_enemy_ccw.x][(int)new_grid_position_enemy_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == false) {
											new_grid_position_enemy = new_grid_position_enemy_cw;
										}
										else if (m_grid_states[(int)new_grid_position_enemy_cw.x][(int)new_grid_position_enemy_cw.y].m_obstacle == true) {
											new_grid_position_enemy = cur_grid_position_enemy;
										}
									}
								}
							}
							break;
						}
					}
					break;
				}
				case 9: { // hero guard up
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_guard_direction = { -1.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero guard up - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero guard up - enemy move up" << std::endl;
							vec2 enemy_direction = { -1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero guard up - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero guard up - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero guard up - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero guard up - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero guard up - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero guard up - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero guard up - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero guard up - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero guard up - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero guard up - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero guard up - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
					}
					break;
				}
				case 10: { // hero guard left
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_guard_direction = { 0.f, -1.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero guard left - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero guard left - enemy move up" << std::endl;
							vec2 enemy_direction = { -1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero guard left - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero guard left - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero guard left - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero guard left - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero guard left - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero guard left - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero guard left - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero guard left - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero guard left - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero guard left - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero guard left - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
					}
					break;
				}
				case 11: { // hero guard down
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_guard_direction = { 1.f, 0.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero guard down - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero guard down - enemy move up" << std::endl;
							vec2 enemy_direction = { -1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero guard down - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero guard down - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero guard down - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero guard down - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero guard down - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero guard down - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero guard down - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero guard down - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero guard down - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero guard down - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero guard down - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
					}
					break;
				}
				case 12: { // hero guard right
					vec2 hero_direction = { 0.f, 0.f };
					vec2 hero_guard_direction = { 0.f, 1.f };
					new_grid_position_hero = add(hero_direction, cur_grid_position_hero);
					switch (m_enemy->m_action) {
						case 0: { // enemy do nothing
							// std::cout << "hero guard down - enemy do nothing" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 1: { // enemy moves up
							// std::cout << "hero guard down - enemy move up" << std::endl;
							vec2 enemy_direction = { -1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 2: { // enemy moves left
							// std::cout << "hero guard down - enemy move left" << std::endl;
							vec2 enemy_direction = { 0.f, -1.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 3: { // enemy moves down
							// std::cout << "hero guard down - enemy move down" << std::endl;
							vec2 enemy_direction = { 1.f, 0.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 4: { // enemy moves right
							// std::cout << "hero guard down - enemy move right" << std::endl;
							vec2 enemy_direction = { 0.f, 1.f };
							vec2 hero_direction_enemy_guard = add(enemy_direction, hero_guard_direction);	
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_obstacle == true) {
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							if (new_grid_position_hero == new_grid_position_enemy) {
								if (hero_direction_enemy_guard.x == 0.f &&
									hero_direction_enemy_guard.y == 0.f) {
									m_points += REWARD_HERO_GUARD;
									new_grid_position_enemy = cur_grid_position_enemy;
								}
								else {
									m_points += PENALTY_ENEMY_COLLISION;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
									vec2 push_hero_direction_ccw = { -enemy_direction.y, enemy_direction.x };
									vec2 push_hero_direction_cw = { enemy_direction.y, -enemy_direction.x };
									vec2 new_grid_position_hero_pushed_ccw = add(new_grid_position_hero, push_hero_direction_ccw);
									vec2 new_grid_position_hero_pushed_cw = add(new_grid_position_hero, push_hero_direction_cw);
									if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_pushed_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_pushed_ccw.x][(int)new_grid_position_hero_pushed_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_pushed_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_pushed_cw.x][(int)new_grid_position_hero_pushed_cw.y].m_obstacle == true) {
											new_grid_position_hero = add(new_grid_position_enemy,enemy_direction);
										}
									}
								}
							}
							break;
						}
						case 5: { // enemy attack up 
							// std::cout << "hero guard down - enemy attack up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { -1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 6: { // enemy attack left 
							// std::cout << "hero guard down - enemy attack left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, -1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 7: { // enemy attack down
							// std::cout << "hero guard down - enemy attack down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 1.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 8: { // enemy attack right
							// std::cout << "hero guard down - enemy attack right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							vec2 enemy_attack_direction = { 0.f, 1.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							if (add(hero_guard_direction, cur_grid_position_hero) == new_grid_position_enemy &&
								add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += REWARD_HERO_GUARD;
								new_grid_position_enemy = cur_grid_position_enemy;
							}
							else if (add(enemy_attack_direction, cur_grid_position_enemy) == new_grid_position_hero) {
								m_points += PENALTY_ENEMY_ATTACK;
								if (m_win_game != nullptr) {
									Mix_PlayChannel(-1, m_lose_points, 0);
								}
								new_grid_position_hero = add(enemy_attack_direction, cur_grid_position_hero);
								if (m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_obstacle == true) {
									vec2 enemy_attack_direction_ccw = { -enemy_attack_direction.y, enemy_attack_direction.x };
									vec2 enemy_attack_direction_cw = { enemy_attack_direction.y, -enemy_attack_direction.x };
									vec2 new_grid_position_hero_ccw = add(enemy_attack_direction_ccw, cur_grid_position_hero);
									vec2 new_grid_position_hero_cw = add(enemy_attack_direction_cw, cur_grid_position_hero);
									if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == false) {
										new_grid_position_hero = new_grid_position_hero_ccw;
									}
									else if (m_grid_states[(int)new_grid_position_hero_ccw.x][(int)new_grid_position_hero_ccw.y].m_obstacle == true) {
										if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == false) {
											new_grid_position_hero = new_grid_position_hero_cw;
										}
										else if (m_grid_states[(int)new_grid_position_hero_cw.x][(int)new_grid_position_hero_cw.y].m_obstacle == true) {
											new_grid_position_hero = cur_grid_position_hero;
										}
									}
								}
							}
							break;
						}
						case 9: { // enemy guard up 
							// std::cout << "hero guard down - enemy guard up" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 10: { // enemy guard left 
							// std::cout << "hero guard down - enemy guard left" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 11: { // enemy guard down 
							// std::cout << "hero guard down - enemy guard down" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
						case 12: { // enemy guard right 
							// std::cout << "hero guard down - enemy guard right" << std::endl;
							vec2 enemy_direction = { 0.f, 0.f };
							new_grid_position_enemy = add(enemy_direction, cur_grid_position_enemy);
							break;
						}
					}
					break;
				}
				default: break;
			}
			m_enemy->m_action = m_policy[(int)m_hero->m_grid_position.y][(int)m_hero->m_grid_position.x][(int)m_enemy->m_grid_position.y][(int)m_enemy->m_grid_position.x][m_hero->m_action];
			break;
		}
	}

	m_grid_states[(int)cur_grid_position_enemy.x][(int)cur_grid_position_enemy.y].m_enemy = false;
	m_grid_states[(int)new_grid_position_enemy.x][(int)new_grid_position_enemy.y].m_enemy = true;

	m_grid_states[(int)cur_grid_position_hero.x][(int)cur_grid_position_hero.y].m_hero = false;
	m_grid_states[(int)new_grid_position_hero.x][(int)new_grid_position_hero.y].m_hero = true;

	m_hero->m_grid_position  = new_grid_position_hero;
	m_enemy->m_grid_position = new_grid_position_enemy;

	if (m_win_game != nullptr) {
		if (m_points < -1500) {
			Mix_PlayChannel(-1, m_lose_game, 0);
			reset();
		}
		else if (m_points > 1500) {
			Mix_PlayChannel(-1, m_win_game, 0);
			reset();			
		}
	}

	return true;
}

bool Grid_World::update(int64_t action) {
	m_hero->m_action = action;
	return update();
}

void Grid_World::draw()
{
	gl_flush_errors();

	int w, h;
	glfwGetFramebufferSize(m_window, &w, &h);

	std::stringstream title_ss;
	title_ss << "Points: " << m_points;
	glfwSetWindowTitle(m_window, title_ss.str().c_str());

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glViewport(0, 0, w, h);
	glDepthRange(0.00001, 10);
	const float clear_color[3] = { 0.3f, 0.3f, 0.8f };
	glClearColor(clear_color[0], clear_color[1], clear_color[2], 1.0);
	glClearDepth(1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	float left = 0.f;// *-0.5;
	float top = 0.f;// (float)h * -0.5;
	float right = (float)w / m_screen_scale;// *0.5;
	float bottom = (float)h / m_screen_scale;// *0.5;

	float sx = 2.f / (right - left);
	float sy = 2.f / (top - bottom);
	float tx = -(right + left) / (right - left);
	float ty = -(top + bottom) / (top - bottom);
	mat3 projection_2D{ { sx, 0.f, 0.f },{ 0.f, sy, 0.f },{ tx, ty, 1.f } };


	for (int i = 0; i < m_rows; ++i) {
		for (int j = 0; j < m_cols; ++j) {
			m_grid_states[i][j].draw(projection_2D, WORLD_TEXTURE.id);
		}
	}

	m_enemy->draw(projection_2D, WORLD_TEXTURE.id);
	m_hero->draw(projection_2D, WORLD_TEXTURE.id);
		
	glfwSwapBuffers(m_window);
}

bool Grid_World::is_over() const
{
	glfwWindowShouldClose(m_window);
	return m_is_over;
}

void Grid_World::on_key(GLFWwindow*, int key, int, int action, int mod)
{
	m_hero->m_action = -1;

	if (action == GLFW_PRESS || action == GLFW_REPEAT) {

		if (mod != GLFW_MOD_SHIFT && mod != GLFW_MOD_ALT) {
			if (key == GLFW_KEY_SPACE) {
				m_hero->m_action = 0;
			}
			else if (key == GLFW_KEY_UP) {
				m_hero->m_action = 1;
			}
			else if (key == GLFW_KEY_LEFT) {
				m_hero->m_action = 2;
			}
			else if (key == GLFW_KEY_DOWN) {
				m_hero->m_action = 3;
			}
			else if (key == GLFW_KEY_RIGHT) {
				m_hero->m_action = 4;
			}
		}

		else if (mod == GLFW_MOD_SHIFT && mod != GLFW_MOD_ALT) {
			
			if (key == GLFW_KEY_UP) {
				m_hero->m_action = 5;
			}
			else if (key == GLFW_KEY_LEFT) {
				m_hero->m_action = 6;
			}
			else if (key == GLFW_KEY_DOWN) {
				m_hero->m_action = 7;
			}
			else if (key == GLFW_KEY_RIGHT) {
				m_hero->m_action = 8;
			}
		}

		else if (mod != GLFW_MOD_SHIFT && mod == GLFW_MOD_ALT) {
			
			if (key == GLFW_KEY_UP) {
				m_hero->m_action = 9;
			}
			else if (key == GLFW_KEY_LEFT) {
				m_hero->m_action = 10;
			}
			else if (key == GLFW_KEY_DOWN) {
				m_hero->m_action = 11;
			}
			else if (key == GLFW_KEY_RIGHT) {
				m_hero->m_action = 12;
			}
		}
	}

	if (m_hero->m_action > -1) {
		update();
	}

	if (action == GLFW_RELEASE && key == GLFW_KEY_R) {
		m_points = 0;

		m_grid_states[(int)m_hero->m_grid_position.x][(int)m_hero->m_grid_position.y].m_hero = false;
		m_grid_states[(int)m_enemy->m_grid_position.x][(int)m_enemy->m_grid_position.y].m_enemy = false;

		m_hero->m_grid_position = { (float)m_hero_init_pos[1], (float)m_hero_init_pos[0] };
		m_hero->m_action = -1;

		int a = rand() % 4 + 1;

		m_enemy->m_grid_position = { (float)m_enemy_init_pos[1], (float)m_enemy_init_pos[0] };
		m_enemy->m_action = a;

		m_grid_states[(int)m_hero->m_grid_position.x][(int)m_hero->m_grid_position.y].m_hero = true;
		m_grid_states[(int)m_enemy->m_grid_position.x][(int)m_enemy->m_grid_position.y].m_enemy = true;
	}

	if (action == GLFW_RELEASE && key == GLFW_KEY_Q) {
		m_is_over = true;
	}
}

void Grid_World::reset() {
	m_points = 0;

	m_grid_states[(int)m_hero->m_grid_position.x][(int)m_hero->m_grid_position.y].m_hero = false;
	m_grid_states[(int)m_enemy->m_grid_position.x][(int)m_enemy->m_grid_position.y].m_enemy = false;

	m_hero->m_grid_position = { (float)m_hero_init_pos[1], (float)m_hero_init_pos[0] };
	m_hero->m_action = -1;

	int a = rand() % 4 + 1;

	m_enemy->m_grid_position = { (float)m_enemy_init_pos[1], (float)m_enemy_init_pos[0] };
	m_enemy->m_action = a;

	m_grid_states[(int)m_hero->m_grid_position.x][(int)m_hero->m_grid_position.y].m_hero = true;
	m_grid_states[(int)m_enemy->m_grid_position.x][(int)m_enemy->m_grid_position.y].m_enemy = true;
}

std::vector<int64_t> Grid_World::extract_state() {
	std::vector<int64_t> state;
	state.push_back(m_hero->m_grid_position.x);
	state.push_back(m_hero->m_grid_position.y);
	state.push_back(m_enemy->m_grid_position.x);
	state.push_back(m_enemy->m_grid_position.y);
	state.push_back(m_enemy->m_action);
	return state;
}