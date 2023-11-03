#include "deepQ.hpp"
#include <vector>
// #include "Windows.h"
#include <iostream>
#include <fstream>

const int MAX_EPISODE = 5000;
const int MAX_TIME = 100;
const int TARGET_UPDATE = 100;
const int BATCH_SIZE = 64;
const float GAMMA = 0.99;


deepQ::deepQ(Grid_World* grid_world) {
	m_world = grid_world;
	m_action_dim = 9;
	m_Net = std::make_shared<Net>(5, m_action_dim);
	m_Target = std::make_shared<Net>(5, m_action_dim);
}

torch::Tensor convert_vector_to_tensor(std::vector<int64_t> s) {
	torch::Tensor x = torch::zeros({(long)s.size()});
	auto x_acc = x.accessor<float, 1>();
	for (int i = 0; i < s.size(); i++) {
		x_acc[i] = s.at(i);
	}
	return x;
}

void deepQ::load(std::string path) {
	torch::serialize::InputArchive input_archive;
	input_archive.load_from(path);
	m_Net->load(input_archive);
	std::cout << m_Net->fc2->named_parameters()["weight"] << "\n";
}

void deepQ::save_as_txt(std::string path) {
	std::ofstream myfile;
	myfile.open(path);
	
	for (int hero_row = 0; hero_row < m_world->m_rows; ++hero_row) {
		for (int hero_col = 0; hero_col < m_world->m_cols; ++hero_col) {
			for (int enemy_row = 0; enemy_row < m_world->m_rows; ++enemy_row) {
				for (int enemy_col = 0; enemy_col < m_world->m_cols; ++enemy_col) {
					for (int enemy_act = 0; enemy_act < m_action_dim; ++enemy_act) {
						int action = m_Net->select_action(convert_vector_to_tensor(std::vector<int64_t>{hero_row, hero_col, enemy_row, enemy_col, enemy_act}));
						myfile << std::to_string(hero_row) << ","
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
	myfile.close();
}

void deepQ::train() {
	int64_t action;
	torch::optim::SGD optimizer(m_Net->parameters(), /*lr=*/0.0001);
	int posCount = 0;
	int negCount = 0;
	int zeroCount = 0;
	int bestScore = 0;
	int scoreSum = 0;
	for (int epi_idx = 0; epi_idx < MAX_EPISODE; epi_idx++) {
		m_world->reset();
		auto state = m_world->extract_state();
		auto new_state = m_world->extract_state();
		int reward = m_world->m_points;
		int new_reward = m_world->m_points;
		for (int t = 0; t < MAX_TIME; t++) {
			state = new_state;
			reward = new_reward;
			double r = ((double)rand() / (RAND_MAX));
			auto test = ((MAX_EPISODE * 1.0 - epi_idx) / MAX_EPISODE);
			if (r < 0.05) {
				// randomize action
				action = rand() % m_action_dim;
			}
			else {
				action = m_Net->select_action(convert_vector_to_tensor(state));
			}
			m_world->update(action);
			new_state = m_world->extract_state();
			new_reward = m_world->m_points;
			int reward_diff = new_reward - reward;
			/*if (reward_diff == 0) {
				reward_diff = -1;
				reward_diff = -10 * std::max(abs(new_state.at(0) - new_state.at(2)), abs(new_state.at(1) - new_state.at(3)));
			}*/
			/*auto expected_reward = m_Target->forward(convert_vector_to_tensor(new_state)).max_values(0).detach() * GAMMA + reward_diff;
			torch::Tensor loss = torch::mse_loss(m_Net->forward(convert_vector_to_tensor(state))[action], expected_reward);
			optimizer.zero_grad();
			loss.backward();
			optimizer.step();*/
			
			//m_replay_buffer.add_experience(state, new_state, action, reward_diff);
			int actual_reward = reward_diff;
			if (action <= 4) {
				int current_dist = std::max(abs(new_state.at(0) - new_state.at(2)), abs(new_state.at(1) - new_state.at(3)));
				int prev_dist = std::max(abs(state.at(0) - state.at(2)), abs(state.at(1) - state.at(3)));

				actual_reward += 50 * (prev_dist - current_dist);
			}
			else if (action > 4 && action <= 8) {
				if (reward_diff == 0) {
					actual_reward -= 100;
				}
			}
			else {
				if (reward_diff == 0) {
					actual_reward -= 50;
				}
			}

			if (reward_diff > 0) {
				for (int i = 0; i < 20; i++) {
					m_replay_buffer.add_experience(state, new_state, action, actual_reward);
				}
				posCount++;
			}
			else if (reward_diff == 0) {
				m_replay_buffer.add_experience(state, new_state, action, actual_reward);
				zeroCount++;
			}
			else {
				for (int i = 0; i < 20; i++) {
					m_replay_buffer.add_experience(state, new_state, action, actual_reward);
				}
				negCount++;
			}
			if (m_replay_buffer.num_experiences() >= BATCH_SIZE) {
				std::vector<deepQ::Experience> batch = m_replay_buffer.sample_experiences(BATCH_SIZE);
				torch::Tensor states_prev = torch::zeros({ BATCH_SIZE, (long)batch[0].state_prev.size() });
				torch::Tensor states_next = torch::zeros({ BATCH_SIZE, (long)batch[0].state_next.size() });
				torch::Tensor actions = torch::zeros({ BATCH_SIZE });
				torch::Tensor rewards = torch::zeros({ BATCH_SIZE });

				auto access_states_prev = states_prev.accessor<float, 2>();
				auto access_states_next = states_next.accessor<float, 2>();
				auto access_actions = actions.accessor<float, 1>();
				auto access_rewards = rewards.accessor<float, 1>();

				for (int sample = 0; sample < BATCH_SIZE; ++sample) {
					for (int elem = 0; elem < batch[0].state_prev.size(); ++elem) {
						access_states_prev[sample][elem] = batch[sample].state_prev.at(elem);
						access_states_next[sample][elem] = batch[sample].state_next.at(elem);
					}
					access_actions[sample] = batch[sample].action;
					access_rewards[sample] = batch[sample].reward;
				}

				auto prediction = m_Net->forward(states_prev);
				auto idx = actions.to(torch::kLong);
				auto indices = idx.unsqueeze(1).expand_as(prediction);
				auto result = at::gather(prediction, 1, indices);
				auto results = result.slice(-1, 0, BATCH_SIZE, 13);

				auto expected_reward = m_Target->forward(states_next).max_values(1).detach() * GAMMA + rewards;
				torch::Tensor loss = torch::mse_loss(results, expected_reward);
				auto watch = torch::max(m_Net->fc1->named_parameters()["weight"]).item<float>();
				optimizer.zero_grad();
				loss.backward();
				optimizer.step();

				if (epi_idx % TARGET_UPDATE == 0) {
					// m_world->draw();
					// Sleep(5.0);
				}
			}
			
		}

		scoreSum += m_world->m_points;
		
		if (epi_idx % TARGET_UPDATE == 0) {
			std::cout << "Current episode: " << epi_idx << std::endl;
			std::cout << "Score: " << m_world->m_points << std::endl;
			torch::serialize::OutputArchive output_archive;
			m_Net->save(output_archive);
			output_archive.save_to(MODEL_PATH + "model.pt");
			// std::cout << m_Net->fc1->named_parameters()["weight"] << "\n";
			torch::serialize::InputArchive input_archive;
			input_archive.load_from(MODEL_PATH + "model.pt");
			m_Target->load(input_archive);
			// std::cout << m_Target->fc1->named_parameters()["weight"] << "\n";
			if (bestScore < m_world->m_points) {
				bestScore = m_world->m_points;
				torch::serialize::OutputArchive output_archive;
				m_Net->save(output_archive);
				output_archive.save_to(MODEL_PATH + "model_" + std::to_string(bestScore) + ".pt");
			}
			std::ofstream myfile;
			myfile.open(MODEL_PATH + "result.csv", std::ios_base::app);
			myfile << std::to_string(epi_idx) << "," << std::to_string(scoreSum*1.0 / TARGET_UPDATE) << std::endl;
			myfile.close();
			scoreSum = 0;
		}
	}
	std::cout << "pos: " << posCount << std::endl;
	std::cout << "zero: " << zeroCount << std::endl;
	std::cout << "neg: " << negCount << std::endl;
	std::string filename_policy;
	if (m_world->m_enemy_type == 0) {
		filename_policy = std::string("bat-") + m_world->m_level_name + std::string("-dqn_policy.txt");
	}
	else if (m_world->m_enemy_type  == 1) {
		filename_policy = std::string("skeleton-") + m_world->m_level_name + std::string("-dqn_policy.txt");
	}
	else if (m_world->m_enemy_type  == 2) {
		filename_policy = std::string("knight-") + m_world->m_level_name + std::string("-dqn_policy.txt");
	}
	save_as_txt(policies_path(filename_policy));
	m_world->destroy();
}