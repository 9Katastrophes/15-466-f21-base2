#include "Mode.hpp"

#include "Scene.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;
	bool collide(Scene::Transform *obj);

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, rot_cw, rot_ccw;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//catman game objects
	Scene::Transform *cat = nullptr;
	std::vector<Scene::Transform *> dogs;
	std::vector<Scene::Transform *> foods;

	glm::quat cat_base_rotation;
	std::vector<glm::quat> dogs_base_rotation;

	//game values
	//we need to know the radius (defined as half the length of the cube edge) in order to move properly
	float cat_radius = 0.3f;
	float dog_radius = 0.4f;
	float food_radius = 0.3f;

	//this value should be a poisitive integer, but we set to -1 as an invalid default value
	int food_left = -1;

	bool alive = true;
	
	//camera:
	Scene::Camera *camera = nullptr;
};
