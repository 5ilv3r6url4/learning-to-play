#include "tabq.hpp"
#include <vector>

using namespace torch;

int arg_max(torch::TensorAccessor<float, 1> tensor) {
	int idx = 0;
	float max_score = 0;
	for (int i = 0; i < tensor.size(0); i++) {
		if (tensor[i] > max_score) {
			max_score = tensor[i];
			idx = i;
		}
	}
	return idx;
}

TabQ::TabQ(Grid_World* world) {
	m_world = world;

	m_action_dim = 13;
	std::vector<int64_t> state_dims;
	state_dims.push_back(m_world->m_rows);
	state_dims.push_back(m_world->m_cols);
	state_dims.push_back(m_world->m_rows);
	state_dims.push_back(m_world->m_cols);
	state_dims.push_back(13);
	state_dims.push_back(13);

	Q = torch::rand(state_dims);
}

void TabQ::train() {
	for (int epi_idx = 0; epi_idx < MAX_EPISODE; epi_idx++) {
		int64_t action;
		m_world->reset();
		std::vector<int64_t> state = m_world->extract_state();
		std::vector<int64_t> new_state = m_world->extract_state();
		int reward = m_world->m_points;
		int new_reward = m_world->m_points;
		for (int t = 0; t < MAX_TIME; t++) {
			double r = ((double)rand() / (RAND_MAX));
			state = new_state;
			reward = new_reward;
			auto Q_acc = Q.accessor<float, 6>();
			if (r < 0.05) {
				// choose action randomly random
				action = rand() % m_action_dim;
			}
			else {
				auto score = Q_acc[state.at(0)][state.at(1)][state.at(2)][state.at(3)][state.at(4)];
				action = arg_max(score);
			}
			m_world->update(action);

			new_state = m_world->extract_state();
			new_reward = m_world->m_points;
			int reward_diff = new_reward - reward;

			auto score = Q_acc[new_state.at(0)][new_state.at(1)][new_state.at(2)][new_state.at(3)][state.at(4)];
			int max_action = arg_max(score);
			float best_Q = score[max_action];
			Q_acc[state.at(0)][state.at(1)][state.at(2)][state.at(3)][state.at(4)][action] += ALPHA * (reward_diff + GAMMA * best_Q - Q_acc[state.at(0)][state.at(1)][state.at(2)][state.at(3)][state.at(4)][action]);
		}
		// std::cout << ">> [ EPISODE ] " << epi_idx << std::endl;
		// std::cout << ">> [ SCORE =  " << m_world->m_points << std::endl;
	}

	// save policy 
	std::string filename_policy;
	if (m_world->m_enemy_type == 0) {
		filename_policy = std::string("bat-") + m_world->m_level_name + std::string("-tabq_policy.txt");
	}
	else if (m_world->m_enemy_type  == 1) {
		filename_policy = std::string("skeleton-") + m_world->m_level_name + std::string("-tabq_policy.txt");
	}
	else if (m_world->m_enemy_type  == 2) {
		filename_policy = std::string("knight-") + m_world->m_level_name + std::string("-tabq_policy.txt");
	}

	std::ofstream file_policy;
	file_policy.open(policies_path(filename_policy), std::ios_base::trunc);
	for (int hero_row = 0; hero_row < m_world->m_rows; ++hero_row) {
		for (int hero_col = 0; hero_col < m_world->m_cols; ++hero_col) {
			for (int enemy_row = 0; enemy_row < m_world->m_rows; ++enemy_row) {
				for (int enemy_col = 0; enemy_col < m_world->m_cols; ++enemy_col) {
					for (int enemy_act = 0; enemy_act < m_action_dim; ++enemy_act) {	
						auto q_access = Q.accessor<float, 6>();				
						auto q_score = q_access[hero_row][hero_col][enemy_row][enemy_col][enemy_act];
						int action = arg_max(q_score);
						file_policy << std::to_string(hero_row) << "," 
								    << std::to_string(hero_col) << "," 
								    << std::to_string(enemy_row) << "," 
								    << std::to_string(enemy_col) << "," 
								    << std::to_string(enemy_act) << "=" 
								    << std::to_string(action) << std::endl;
					}
				}
			}
		}
	}
	file_policy.close();
	m_world->destroy();
}