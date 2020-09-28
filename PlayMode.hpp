#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const&, glm::uvec2 const& window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const& drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	bool restart = true;

	Scene::Transform* board = nullptr;
	Scene::Transform* ball = nullptr;
	glm::vec3 board_rotation = glm::vec3(0.0f, 0.0f, 0.0f);

	bool on_board = true;
	float ball_mass = 1.0f;
	glm::vec3 ball_acc = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 ball_vel = glm::vec3(0.0f, 0.0f, 0.0f);

	glm::vec2 wind = glm::vec2(0.0f, 0.0f);
	bool no_wind = false;

	//camera:
	Scene::Camera* camera = nullptr;

};
