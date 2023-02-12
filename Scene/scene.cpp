#include "scene.hpp"
#include "app.hpp"
#include "ThreadPool.hpp"

namespace ve
{
    Scene::~Scene()
    {
        m_Registry.clear();
    }

    Entity Scene::createEntity(const std::string &name)
    {
        auto entity = m_Registry.create();
        m_Registry.emplace<TagComponent>(entity, name);
        m_EntityCount++;
        return entity;
    }

    void Scene::destroyEntity(Entity entity)
    {
        m_Registry.destroy(entity);
        m_EntityCount--;
    }
    /**
     * @note - It makes sense to use this function only for imgui interface which will have its own class in the future
     *
     * @param filename - path to the file with .obj extension
     * @return std::pair<std::string, Entity> - pair of the name of the object and the entity
     */
    std::pair<std::string, Entity> Scene::addEntity(const std::string &filename)
    {
        auto name = filename.substr(0, filename.find_last_of('.'));
        auto entity = createEntity(name);
        addComponent<TransformComponent>(entity, glm::vec3(0.f, 0.f, 0.f), glm::vec3(.0f, .0f, 0.0f), glm::vec3(1.f, 1.f, 1.f), 0.0f);
		addComponent<MeshComponent>(entity, model_path + filename);
		auto &model = getComponent<MeshComponent>(entity);
		model.model->loadModel();
		return {name, entity};
    }

	void Scene::LoadModels()
	{
		auto& models = veModel::GetModels();

		if(models.empty())
			return;

		ThreadPool pool;
		std::vector<std::string> loadingModels(m_EntityCount);
        
		for(auto& model : models)
		{
            pool.enqueue([&]() { model.second->loadModel(); });
		}
	}
	void Scene::OnUpdate(float dt, float aspect, bool projection)
	{
		if(projection)
			m_Camera->setPerspectiveProjection(glm::radians(60.0f), aspect, 0.1f, 1000.0f);
		else
			m_Camera->setOrthographicProjection(-aspect, aspect, -1.0f, 1.0f, 0.1f, 1000.0f);

		m_Camera->OnUpdate(dt);
		getComponentView<Player, TransformComponent>().each([&](auto entity, auto& player, auto& transform)
		{
			player.OnUpdate(dt, transform);
		});
	}
} // namespace ve