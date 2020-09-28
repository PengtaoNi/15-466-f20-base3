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
		wind = glm::vec2(0.0f, 0.0f);
		on_board = true;
		restart = false;

		Sound::play(*chime_sample, 1.0f, 0.0f);;
	}
	else {
		if (no_wind) wind = glm::vec2(0.0f, 0.0f);
		else {
			wind.x += (rand() / (float)RAND_MAX - 0.5f) * 10.0f * elapsed;
			wind.y += (rand() / (float)RAND_MAX - 0.5f) * 10.0f * elapsed;
			wind.x = std::max(-6.0f, std::min(6.0f, wind.x));
			wind.y = std::max(-6.0f, std::min(6.0f, wind.y));
		}
	}

	// rotate board
	if (left.pressed && !right.pressed) board_rotation.y -= elapsed;
	if (!left.pressed && right.pressed) board_rotation.y += elapsed;
	if (!down.pressed && up.pressed) board_rotation.x -= elapsed;
	if (down.pressed && !up.pressed) board_rotation.x += elapsed;
	board_rotation.x = std::max(-float(M_PI) / 4.0f, std::min(float(M_PI) / 4.0f, board_rotation.x));
	board_rotation.y = std::max(-float(M_PI) / 4.0f, std::min(float(M_PI) / 4.0f, board_rotation.y));
	board->rotation = board_rotation;

	// check if ball is touching board
	bool touching_board = true;
	glm::vec3 old_pos = ball->position;
	glm::vec3 norm = glm::vec3(0.0f, 0.0f, 0.0f);
	norm.x = std::sin(board_rotation.y) * std::cos(board_rotation.x);
	norm.y = -std::sin(board_rotation.x) * std::cos(board_rotation.y);
	norm.z = std::cos(board_rotation.x) * std::cos(board_rotation.y);
	norm = glm::normalize(norm);
	float dist = old_pos.x * norm.x + old_pos.y * norm.y + old_pos.z * norm.z;
	if (std::abs(dist - 2.0f) > 0.01f) touching_board = false;

	// roll ball
	ball_acc = glm::vec3(wind.x, wind.y, -9.8f);
	if (on_board && touching_board) {
		ball_acc += norm * 9.8f * std::cos(board_rotation.x) * std::cos(board_rotation.y);
	}
	ball_vel += ball_acc * elapsed;
	ball->position += ball_vel * elapsed;

	// check if ball is out of board
	glm::vec3 ball_proj = ball->position - dist * norm;
	if (std::abs(ball_proj.x) > 10.0f * std::cos(board_rotation.y) ||
		std::abs(ball_proj.y) > 10.0f * std::cos(board_rotation.x)) {
		if (dist <= 2.0f) on_board = false;
	}

	// make sure ball doesn't go through board
	dist = ball->position.x * norm.x + ball->position.y * norm.y + ball->position.z * norm.z;
	if (on_board) {
		if (dist < 2.0f) ball->position += norm * (2.0f - dist);
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
	
	// update velocity based on final position of the ball
	if (elapsed > 0) ball_vel = (ball->position - old_pos) / elapsed;

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
