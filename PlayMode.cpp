#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>
#include <math.h>

GLuint catman_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > catman_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("catman.pnct"));
	catman_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > catman_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("catman.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = catman_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = catman_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

PlayMode::PlayMode() : scene(*catman_scene) {
	//get pointers to leg for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Cat") cat = &transform;
		else if (transform.name.substr(0, 3) == "Dog") dogs.push_back(&transform);
		else if (transform.name.substr(0, 4) == "Food") foods.push_back(&transform);
	}
	if (cat == nullptr) throw std::runtime_error("Cat not found.");
	if (dogs.size() != 3) throw std::runtime_error("Some dogs not found.");
	if (foods.size() != 8) throw std::runtime_error("Some foods not found.");

	cat_base_rotation = cat->rotation;
	for (size_t i=0;i<dogs.size();i++){
		dogs_base_position.push_back(dogs[i]->position);
		dogs_circle_radius.push_back(i+1.0f);
		if (i % 2 == 0)
			dogs_circle_direction.push_back(1.0f);
		else
			dogs_circle_direction.push_back(-1.0f);
	}
	food_left = foods.size();

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN && alive) {
		if (evt.key.keysym.sym == SDLK_j){
			rot_ccw.downs += 1;
			rot_ccw.pressed = true;
		} else if (evt.key.keysym.sym == SDLK_k) {
			rot_cw.downs += 1;
			rot_cw.pressed = true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP && alive) {
		if (evt.key.keysym.sym == SDLK_j) {
			rot_ccw.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_k){
			rot_cw.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		}
	}

	return false;
}

bool PlayMode::collide(Scene::Transform *obj) {
	std::string name = obj->name;
	float dist = glm::distance(cat->position, obj->position);

	if (name.substr(0, 3) == "Dog"){
		if (dist <= cat_radius + dog_radius) return true;
	}
	else if (name.substr(0, 4) == "Food"){
		if (dist <= cat_radius + food_radius) return true;
	}
	return false;
}

void PlayMode::update(float elapsed) {

	//rotate camera and player:
	{
		float rot = 0.0f;
		if (rot_cw.pressed && !rot_ccw.pressed) rot = glm::radians(1.0f);
		if (!rot_cw.pressed && rot_ccw.pressed) rot = glm::radians(-1.0f);

		glm::vec3 angles(0.0f, 0.0f, rot);
		glm::quat delta_rotation(angles);
		camera->transform->rotation = glm::normalize(camera->transform->rotation * delta_rotation);
		cat->rotation = glm::normalize(cat->rotation * delta_rotation);
	}

	//move the player:
	{
		constexpr float PlayerSpeed = 0.05f;
		glm::vec2 move = glm::vec2(0.0f);

		if (up.pressed && alive) move.y += -PlayerSpeed;
		if (down.pressed && alive) move.y += PlayerSpeed;

		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		//glm::vec3 right = frame[0];
		glm::vec3 up = -frame[1];
		//glm::vec3 forward = -frame[2];

		glm::vec3 future_pos = cat->position + move.y * up;
		if (x_bounds.x < future_pos.x && future_pos.x < x_bounds.y && y_bounds.x < future_pos.y && future_pos.y < y_bounds.y){
			cat->position = future_pos;
		}
	}

	//move the dogs; they like to run in circles
	{
		constexpr float DogCircleSpeed = 0.02f;
		for (size_t i=0;i<dogs.size();i++) {
			float rot = -0.1f;

			glm::vec3 angles(0.0f, 0.0f, rot);
			glm::quat delta_rotation(angles);
			dogs[i]->rotation = glm::normalize(dogs[i]->rotation * delta_rotation);

			dog_circle_degrees += DogCircleSpeed;
			dogs[i]->position.x = dogs_circle_direction[i] * dogs_circle_radius[i] * cos(dog_circle_degrees) + dogs_base_position[i].x;
			dogs[i]->position.y = dogs_circle_direction[i] * dogs_circle_radius[i] * sin(dog_circle_degrees) + dogs_base_position[i].y;
		}

	}

	//check collisions
	{
		for (size_t i=0;i<dogs.size();i++){
			if (collide(dogs[i])) {
				alive = false;
			}
		}
		for (size_t i=0;i<foods.size();i++){
			if (collide(foods[i])){
				foods[i]->position.z -= 100.0f; //essentially disappearing
				foods.erase(foods.begin()+i);
				food_left--;
			}
		}
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

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

		constexpr float H = 0.09f;
		float ofs = 2.0f / drawable_size.y;
		if (!alive){
			lines.draw_text("OW!",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
		else if (food_left > 0) {
			lines.draw_text("J and K rotate the camera. W and S move you forward and backward.",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
		else if (food_left == 0) {
			lines.draw_text("Nom nom. I'm full now! Purrrr.",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
	}
}
