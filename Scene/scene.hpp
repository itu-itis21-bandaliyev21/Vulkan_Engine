#pragma once
#include "ve.hpp"
#include "components.hpp"
#include "Camera.hpp"

#include <entt/entt.hpp>

#include <atomic>
#include <thread>
#include <mutex>

#define LOAD_STATE_NONE 0
#define LOAD_STATE_STARTED 1
#define LOAD_STATE_FINISHED 2

namespace ve
{
    using Entity = entt::entity;
    using Registry = entt::registry;

    class Scene
    {
    public:
        Scene()
		{
			m_CameraEntity = createEntity("Camera");
			auto& transform = addComponent<TransformComponent>(m_CameraEntity, glm::vec3{ 0, 0, -14 });
			m_Camera = new Camera(transform);
		}
        ~Scene();

        Entity createEntity(const std::string &name);
        std::pair<std::string, Entity> addEntity(const std::string &filename);
        void destroyEntity(Entity entity);

        inline uint32_t getEntityCount() { return m_EntityCount; }

        // add component to entity
        template <typename T, typename... Args>
        T &addComponent(Entity entity, Args &&...args)
        {
            return m_Registry.emplace<T>(entity, std::forward<Args>(args)...);
        }

        template <typename... Ts>
        void addComponents(Entity entity, Ts &&...args)
        {
            (addComponent<Ts>(entity, std::forward<Ts>(args)...), ...);
        }

        // get entites with specific component
        template <typename T>
        auto getEntitiesWithComponent()
        {
            return m_Registry.view<T>();
        }

        TagComponent getEntityName(Entity entity)
        {
            return m_Registry.get<TagComponent>(entity);
        }

        // get component from entity
        template <typename T>
        auto &getComponent(Entity entity)
        {
            return m_Registry.get<T>(entity);
        }

        template <typename... Comps>
        auto getComponentView()
        {
			return m_Registry.view<Comps...>();
        }

		void OnUpdate(float dt, float aspect, bool projection);
		void LoadModels();
		Camera *GetCamera() { return m_Camera; }

        float m_SkyColor[3] = {0.0f, 0.0f, 0.0f};
        Registry m_Registry;

    private:
		Camera* m_Camera = nullptr;
		Entity m_CameraEntity = entt::null;
        uint32_t m_EntityCount = 0;
        std::atomic_int m_loadingEntity = 0;
        veDevice &device = veDevice::get();
    };
} // namespace ve