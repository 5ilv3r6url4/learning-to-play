// internal
#include "common.hpp"
#include "grid_world.hpp"
#include "deepQ.hpp"
#include "tabq.hpp"

#define GL3W_IMPLEMENTATION
#include <gl3w.h>

// stlib
#include <iostream>

// libtorch
#include <torch/torch.h>

// Global 
Grid_World 	g_world;

// Entry point
int main(int argc, char* argv[])
{
	if (argc < 3) {
		std::cout << "[ ERROR ] incorrect args\n";
		std::cout << "[ EXAMPLE ./game play level_0.txt bat 1 2 3 4 \n";
		return EXIT_FAILURE;
	}

	std::string flag = std::string(argv[1]);
	std::string filename_level = argv[2];
	std::string enemy_flag = std::string(argv[3]);
	std::vector<int> hero_pos = { atoi(argv[4]), atoi(argv[5]) };
	std::vector<int> enemy_pos = { atoi(argv[6]), atoi(argv[7]) };

	int enemy_type = -1;
	if (enemy_flag.compare(std::string("bat")) == 0){
		enemy_type = 0;
	}
	else if (enemy_flag.compare(std::string("skeleton")) == 0){
		enemy_type = 1;
	}
	else if (enemy_flag.compare(std::string("knight")) == 0){
		enemy_type = 2;
	}
	if (enemy_type == -1) {
		std::cout << "[ ERROR ] incorrect enemy type\n";
		std::cout << "[ 'bat' for basic \n";
		std::cout << "[ 'skeleton' for intermediate \n";
		std::cout << "[ 'knight' for advanced \n";
		return EXIT_FAILURE;
	}

	if (flag == "play-tabq") {
		if (!g_world.init(filename_level, std::string("tabq"), enemy_type, hero_pos, enemy_pos, true))
		{
			std::cout << "Press any key to exit" << std::endl;
			std::cin.get();
			return EXIT_FAILURE;
		}
		while (!g_world.is_over())
		{
			glfwWaitEventsTimeout(0.5);
			g_world.draw();
		}
		g_world.destroy();
	}

	if (flag == "play-dqn") {
			if (!g_world.init(filename_level, std::string("dqn"), enemy_type, hero_pos, enemy_pos, true))
			{
				std::cout << "Press any key to exit" << std::endl;
				std::cin.get();
				return EXIT_FAILURE;
			}
			while (!g_world.is_over())
			{
				glfwWaitEventsTimeout(0.5);
				g_world.draw();
			}
			g_world.destroy();
		}

	else if (flag ==  "tabq") {
		if (g_world.init(filename_level, flag, enemy_type, hero_pos, enemy_pos)) {
			TabQ* q = new TabQ(&g_world);
			q->train();
		}
	}

	else if (flag == "dqn") {
		if (g_world.init(filename_level, flag, enemy_type, hero_pos, enemy_pos)) {
			deepQ* q = new deepQ(&g_world);
			q->train();
		}
	}

	else {
		std::cout << "[ ERROR ] incorrect flag\n";
		std::cout << "[ 'play' to render and play game\n";
		std::cout << "[ 'tabq' to NOT render and train with tabq\n";
		std::cout << "[ 'dqn' to NOT render and train with dqn\n";

		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}