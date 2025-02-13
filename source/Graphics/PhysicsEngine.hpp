#pragma once
#include "Scene/Scene.hpp"

namespace Nyxis
{
	class PhysicsEngine
	{
	public:
		PhysicsEngine();
		~PhysicsEngine();

		void OnUpdate(float deltaTime);

		bool enable = false;
        glm::vec2 edges = glm::vec2(1.f, 0.8f);
        float gravity = 0.981f;
    private:
	};
}
