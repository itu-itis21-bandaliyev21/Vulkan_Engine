#include "Core/Application.hpp"
#include "Core/Renderer.hpp"
#include "Core/GLTFRenderer.hpp"
#include "Core/FrameInfo.hpp"
#include "Events/MouseEvents.hpp"
#include "Scene/Components.hpp"

namespace Nyxis
{
    Application::Application()
	{
        Renderer::Init(&m_Window, &m_Device);
        m_FrameInfo = std::make_shared<FrameInfo>();
        m_EditorLayer.OnAttach();
    	auto commandBuffer = m_Device.beginSingleTimeCommands();
        m_EditorLayer.Init(Renderer::GetUIRenderPass(), commandBuffer);
        m_Device.endSingleTimeCommands(commandBuffer);
    	m_Scene = std::make_shared<Scene>();
        m_EditorLayer.SetScene(m_Scene);
    	s_Instance = this;
        m_Window.SetEventCallback(std::bind(&Application::OnEvent, this, std::placeholders::_1));
	}

    Application::~Application()
    {
        m_EditorLayer.OnDetach();
        Renderer::Shutdown();
        Log::Shutdown();
    }

    void Application::OnEvent(Event& e)
	{
#if 0
        LOG_INFO("{}", e.toString());
#endif
	}

    void Application::Run()
	{
        // create rendering systems
        GLTFRenderer gltfRenderer{ Renderer::GetSwapChainRenderPass() };

        // add functions to editor layer
        {
 			EditorLayer::AddFunction([&]() {
                ImGui::Begin("Statistics");
                ImGui::Text("Entity Count: %d", m_Scene->GetEntityCount());
                ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
                ImGui::End();
                });

            EditorLayer::AddFunction([&]() {
                ImGui::Begin("Physics");
                ImGui::Checkbox("Enable Physics", &m_PhysicsEngine.enable);
                ImGui::DragFloat2("BoxEdges", &m_PhysicsEngine.edges.x);
                ImGui::DragFloat("Gravity", &m_PhysicsEngine.gravity, 0.1, -1.0f, 1.0f);
                ImGui::End();
                });

            EditorLayer::AddFunction([&]() {
                    ImGui::Begin("Scene Settings");
                    ImGui::Text("SkyMap");
                    ImGui::DragFloat("Exposure", &gltfRenderer.sceneInfo.shaderValuesParams.exposure, 0.1f, 0.0f, 10.0f);
                    ImGui::DragFloat("Gamma", &gltfRenderer.sceneInfo.shaderValuesParams.gamma, 0.1f, 0.0f, 10.0f);
                    ImGui::DragFloat3("Light Direction", &gltfRenderer.sceneInfo.shaderValuesParams.lightDir.x);
                    if(ImGui::BeginCombo("Environment", gltfRenderer.m_EnvMapFile.c_str()))
                    {
                        static const std::string path = "../assets/environments/";
                        static const std::string ext = ".ktx";

						std::vector maps = { gltfRenderer.m_EnvMapFile };
						for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
						{
							if (entry.path().extension() == ext)
							{
								maps.push_back(entry.path().filename().string());
							}
						}

						static int current_item = 0;
						for (int n = 0; n < maps.size(); n++)
						{
							const bool is_selected = (current_item == n);
							if (ImGui::Selectable(maps[n].data(), is_selected))
								current_item = n;
							if (is_selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();

                    	if(current_item != 0)
                    	{
                    		gltfRenderer.m_EnvMapFile = path + maps[current_item];
							gltfRenderer.UpdateScene();
                            current_item = 0;
                    	}
                    }

                    static std::vector<const char*> debugViews = { "None", "Base color", "Normal", "Occlusion", "Emissive", "Metallic", "Roughness" };
                    static int debugViewIndex = 0;
                    if (ImGui::Combo("Debug View", &debugViewIndex, &debugViews[0], debugViews.size(), debugViews.size()))
                        gltfRenderer.sceneInfo.shaderValuesParams.debugViewInputs = static_cast<float>(debugViewIndex);

                    static std::vector<const char*> pbrEquations = { "None", "Diff (l,n)", "F (l,h)", "G (l,v,h)", "D (h)", "Specular" };
                    static int pbrIndex = 0;
                    if (ImGui::Combo("PBR Equation", &pbrIndex, &pbrEquations[0], pbrEquations.size(), pbrEquations.size()))
                        gltfRenderer.sceneInfo.shaderValuesParams.debugViewEquation = static_cast<float>(pbrIndex);

                    ImGui::End();
                });
        }

    	auto currentTime = std::chrono::high_resolution_clock::now();

#if 1
        for(int i = 0; i < 1; i++)
        {
			auto model = m_Scene->CreateEntity("Model");
            m_Scene->AddComponent<RigidBody>(model);
            m_Scene->AddComponent<Model>(model);
        }
#endif

    	while (!m_Window.ShouldClose()) {
            glfwPollEvents();
            auto newTime = std::chrono::high_resolution_clock::now();
            auto frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;

        	auto worldExtent = Renderer::GetAspectRatio();
			float aspect = static_cast<float>(worldExtent.width) / static_cast<float>(worldExtent.height);
        	auto worldCommandBuffer = Renderer::BeginWorldFrame();

			m_FrameInfo->frameTime = frameTime;
			m_FrameInfo->frameIndex = Renderer::GetFrameIndex();
    		m_FrameInfo->commandBuffer = worldCommandBuffer;

            Renderer::BeginMainRenderPass(m_FrameInfo->commandBuffer);
            gltfRenderer.Render();

            m_PhysicsEngine.OnUpdate(m_FrameInfo->frameTime);
            Renderer::EndMainRenderPass(worldCommandBuffer);
            
        	auto commandBuffer = Renderer::BeginUIFrame();
			m_FrameInfo->commandBuffer = commandBuffer;

            m_EditorLayer.Begin();
    		m_EditorLayer.OnUpdate();
            m_EditorLayer.End();
    		Renderer::EndUIRenderPass(commandBuffer);
			Renderer::SetWorldImageSize(m_EditorLayer.GetViewportExtent());

            m_Scene->OnUpdate(m_FrameInfo->frameTime, aspect);

    		gltfRenderer.OnUpdate();
    	}

    	vkDeviceWaitIdle(m_Device.device());
    }
} // namespace Nyxis