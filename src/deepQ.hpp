#pragma once

#include <torch/torch.h>
#include "grid_world.hpp"

#include <algorithm>
#include <random>

class deepQ
{
public:
	deepQ(Grid_World* grid_world);

	void train();

	void load(std::string path);

	void save_as_txt(std::string path);

private:
	struct Net :torch::nn::Module {
		Net(int state_size, int action_size) {
			m_state_size = state_size;
			m_action_size = action_size;
			fc1 = register_module("fc1", torch::nn::Linear(m_state_size, 50));
			fc2 = register_module("fc2", torch::nn::Linear(50, m_action_size));
		}

		torch::Tensor forward(torch::Tensor x) {
			x = torch::sigmoid(fc1->forward(x));
			x = fc2->forward(x);
			return x;
		}

		int select_action(torch::Tensor state) {
			auto x = forward(state);
			return x.argmax(0).item().toInt();
		}

		int m_state_size, m_action_size;

		torch::nn::Linear fc1{ nullptr }, fc2{ nullptr };
	};

	struct Experience {
		std::vector<int64_t> state_prev;
		std::vector<int64_t> state_next;
		int64_t action; 
		int reward;

		Experience(std::vector<int64_t> state_prev, std::vector<int64_t> state_next, int64_t action, int reward) {
			this->state_prev = state_prev;
			this->state_next = state_next;
			this->action = action;
			this->reward = reward;
		}
	};

	struct ReplayBuffer {
		std::vector<Experience> experiences;
		int n_exp;
		const int size = 10000;
		ReplayBuffer() {
			experiences.reserve(size);
			n_exp = 0;
		}

		void add_experience(std::vector<int64_t> state_prev, std::vector<int64_t> state_next, int64_t action, int reward) {
			Experience experience = Experience(state_prev, state_next, action, reward);
			if (experiences.size() < size) {
				experiences.push_back(experience);
			}
			else {
				experiences[n_exp % size] = experience;
			}
			n_exp++;
		}

		int num_experiences() {
			return n_exp;
		}

	    std::vector<Experience> sample_experiences(int batch_size) {
	    	int off = n_exp > size ? size : n_exp;
    	    std::vector<Experience> batch;
			std::sample(experiences.begin(), experiences.begin() + off, std::back_inserter(batch),
            batch_size, std::mt19937{std::random_device{}()});
            return batch;
        }
	};

	torch::Tensor Q;
	int m_action_dim = -1;
	Grid_World* 	m_world;
	ReplayBuffer m_replay_buffer;
	std::shared_ptr<deepQ::Net> m_Net;
	std::shared_ptr<deepQ::Net> m_Target;
	std::string MODEL_PATH = "./deepQ/";
};