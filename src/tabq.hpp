#pragma once

#include "grid_world.hpp"
#include "common.hpp"

// stdlib
#include <iostream>
#include <string>
#include <fstream>

// torch
#include <torch/torch.h>

class TabQ
{
public:
	TabQ(Grid_World* grid_world);
	void train();

private:
	torch::Tensor Q;
	const int MAX_EPISODE = 10000;
	const int MAX_TIME = 500;
	const float ALPHA = 0.1;
	const float GAMMA = 0.99;

	std::vector<int64_t> m_state_dims;
	int64_t m_action_dim;
	Grid_World* m_world;
};