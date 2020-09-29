#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>
#include <time.h>

GLuint balance_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > balance_meshes(LoadTagDefault, []() -> MeshBuffer const* {
	MeshBuffer const* ret = new MeshBuffer(data_path("balance.pnct"));
	balance_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
	});

Load< Scene > balance_scene(LoadTagDefault, []() -> Scene const* {
	return new Scene(data_path("balance.scene"), [&](Scene& scene, Scene::Transform* transform, std::string const& mesh_name) {
		Mesh const& mesh = balance_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable& drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = balance_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

		});
	});

Load< Sound::Sample > chime_sample(LoadTagDefault, []() -> Sound::Sample const* {
	return new Sound::Sample(data_path("chime.wav"));
	});
Load< Sound::Sample > chime_sample_low(LoadTagDefault, []() -> Sound::Sample const* {
	return new Sound::Sample(data_path("chime_low.wav"));
	});
Load< Sound::Sample > chime_sample_high(LoadTagDefault, []() -> Sound::Sample const* {
	return new Sound::Sample(data_path("chime_high.wav"));
	});

PlayMode::PlayMode() : scene(*balance_scene) {
	for (auto& transform : scene.transforms) {
		if (transform.name == "board") board = &transform;
		else if (transform.name == "ball") ball = &transform;
	}

	if (board == nullptr) throw std::runtime_error("board not found.");
	if (ball == nullptr) throw std::runtime_error("ball not found.");

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const& evt, glm::uvec2 const& window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_LEFT) {
			left.downs += 1;
			left.pressed = true;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_RIGHT) {
			right.downs += 1;
			right.pressed = true;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_UP) {
			up.downs += 1;
			up.pressed = true;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_DOWN) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_r) {
			restart = true;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_w) {
			no_wind = !no_wind;
			return true;
		}
	}
	else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_LEFT) {
			left.pressed = false;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_RIGHT) {
			right.pressed = false;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_UP) {
			up.pressed = false;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_DOWN) {
			down.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	if (restart) {
		board_rotation = glm::vec3(0.0f, 0.0f, 0.0f);
		ball->position = glm::vec3(0.0f, 0.0f, 2.0f);
		ball_acc = glm::vec3(0.0f, 0.0f, 0.0f);
		ball_vel = glm::vec3(0.0f, 0.0f, 0.0f);
		wind = glm::vec3(0.0f, 0.0f, 0.0f);
		old_dist = 2.0f;
		old_norm = glm::vec3(0.0f, 0.0f, 0.0f);
		counter = 0.0f;
		Sound::stop_all_samples();

		srand((int)time(NULL));
		if (!no_wind) {
			wind.x = (float)(rand() % 3 - 1); // -1, 0, or 1
			wind.y = (float)(rand() % 3 - 1);
			wind.z = (rand() / (float)RAND_MAX) * 2.0f;

			if (wind.y == -1.0f) {
				Sound::play(*chime_sample_low, wind.z, wind.x);
			}
			else if (wind.y == 0.0f && wind.x != 0.0f) {
				Sound::play(*chime_sample, wind.z, wind.x);
			}
			else if (wind.y == 1.0f) {
				Sound::play(*chime_sample_high, wind.z, wind.x);
			}
		}

		restart = false;
	}

	if (no_wind) wind = glm::vec3(0.0f, 0.0f, 0.0f);

	if (counter > 15.0f || (wind.x == 0.0f && wind.y == 0.0f)) {
		srand((int)time(NULL));
		if (!no_wind) {
			wind.x = (float)(rand() % 3 - 1); // -1, 0, or 1
			wind.y = (float)(rand() % 3 - 1);
			wind.z = (rand() / (float)RAND_MAX) * 2.0f;

			if (wind.y == -1.0f) {
				Sound::play(*chime_sample_low, wind.z, wind.x);
			}
			else if (wind.y == 0.0f && wind.x != 0.0f) {
				Sound::play(*chime_sample, wind.z, wind.x);
			}
			else if (wind.y == 1.0f) {
				Sound::play(*chime_sample_high, wind.z, wind.x);
			}
		}

		counter = 0.0f;
	}

	// check if ball is touching board
	bool touching_board = false;
	glm::vec3 old_pos = ball->position;
	glm::vec3 ball_proj = old_pos - old_dist * old_norm;
	if (std::abs(old_dist - 2.0f) <= 0.01f &&
		std::abs(ball_proj.x) <= 10.0f * std::cos(board_rotation.y) &&
		std::abs(ball_proj.y) <= 10.0f * std::cos(board_rotation.x)) {
		touching_board = true;
	}

	// rotate board
	if (left.pressed && !right.pressed) board_rotation.y -= 0.7f * elapsed;
	if (!left.pressed && right.pressed) board_rotation.y += 0.7f * elapsed;
	if (!down.pressed && up.pressed) board_rotation.x -= 0.7f * elapsed;
	if (down.pressed && !up.pressed) board_rotation.x += 0.7f * elapsed;
	board_rotation.x = std::max(-float(M_PI) / 6.0f, std::min(float(M_PI) / 6.0f, board_rotation.x));
	board_rotation.y = std::max(-float(M_PI) / 6.0f, std::min(float(M_PI) / 6.0f, board_rotation.y));
	board->rotation = board_rotation;

	// update norm
	glm::vec3 norm = glm::vec3(0.0f, 0.0f, 0.0f);
	norm.x = std::sin(board_rotation.y) * std::cos(board_rotation.x);
	norm.y = -std::sin(board_rotation.x) * std::cos(board_rotation.y);
	norm.z = std::cos(board_rotation.x) * std::cos(board_rotation.y);
	norm = glm::normalize(norm);

	// roll ball
	ball_acc = glm::vec3(wind.x * wind.z, wind.y * wind.z, -9.8f);
	if (touching_board) {
		ball_acc += norm * 9.8f * std::cos(board_rotation.x) * std::cos(board_rotation.y);
	}
	ball_vel += ball_acc * elapsed;
	ball->position += ball_vel * elapsed;

	// check if ball is out of board
	/*glm::vec3 ball_proj = ball->position - dist * norm;
	if (std::abs(ball_proj.x) > 10.0f * std::cos(board_rotation.y) ||
		std::abs(ball_proj.y) > 10.0f * std::cos(board_rotation.x)) {
		if (dist <= 2.0f) on_board = false;
	}*/

	// make sure ball doesn't go through board
	float dist = ball->position.x * norm.x + ball->position.y * norm.y + ball->position.z * norm.z;
	ball_proj = ball->position - dist * norm;
	if (std::abs(ball_proj.x) <= 10.0f * std::cos(board_rotation.y) &&
		std::abs(ball_proj.y) <= 10.0f * std::cos(board_rotation.x)) {
		if (old_dist >= 2.0f && dist < 2.0f) {
			ball->position += norm * (2.0f - dist);
			dist = 2.0f;
		}
	}
	else {
		// on edge
		if (dist >= 0 &&
			std::abs(ball_proj.x) > 10.0f * std::cos(board_rotation.y) &&
			std::abs(ball_proj.x) < 11.0f * std::cos(board_rotation.y)) {
			float out_dist = std::abs(ball_proj.x) / std::cos(board_rotation.y) - 10.0f;
			float true_dist = 1.0f + std::sqrt(1.0f - out_dist * out_dist);
			if (dist < true_dist) ball->position += norm * (true_dist - dist);
		}
		else if (dist >= 0 &&
			     std::abs(ball_proj.y) > 10.0f * std::cos(board_rotation.x) &&
			     std::abs(ball_proj.y) < 11.0f * std::cos(board_rotation.x)) {
			float out_dist = std::abs(ball_proj.y) / std::cos(board_rotation.x) - 10.0f;
			float true_dist = 1.0f + std::sqrt(1.0f - out_dist * out_dist);
			if (dist < true_dist) ball->position += norm * (true_dist - dist);
		}
	}
	
	// update parameters
	if (elapsed > 0) ball_vel = (ball->position - old_pos) / elapsed;
	old_norm = norm;
	old_dist = dist;
	counter += elapsed;

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const& drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	GL_ERRORS();
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, -1.0f)));
	GL_ERRORS();
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	GL_ERRORS();
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		/*constexpr float H = 0.09f;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H, 1.0 - 1.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H + ofs, 1.0 - 1.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));*/

			// draw wind
		lines.draw(glm::vec3(-0.1f, 0.7f, 0.0f), glm::vec3(-0.1f, 0.9f, 0.0f), glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw(glm::vec3(-0.1f, 0.7f, 0.0f), glm::vec3(0.1f, 0.7f, 0.0f), glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw(glm::vec3(0.1f, 0.9f, 0.0f), glm::vec3(-0.1f, 0.9f, 0.0f), glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw(glm::vec3(0.1f, 0.7f, 0.0f), glm::vec3(0.1f, 0.9f, 0.0f), glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		glm::vec3 center = glm::vec3(0.0f, 0.8f, 0.0f);
		glm::vec3 wind_vec = glm::vec3(wind.x / 60.0f, wind.y / 60.0f, 0.0f);
		lines.draw(center, center + wind_vec, glm::u8vec4(0x00, 0x00, 0x00, 0x00));
	}
}
