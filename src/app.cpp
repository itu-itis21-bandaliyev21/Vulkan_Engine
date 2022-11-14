#include "app.hpp"
#include "descriptors.hpp"
#include "gameObject.hpp"
#include "model.hpp"
#include "renderer.hpp"
#include "simpleRenderSystem.hpp"
#include "swap_chain.hpp"
#include "camera.hpp"
#include "keyboardInput.hpp"
#include "frameInfo.hpp"
#include "pointLightSystem.hpp"

#include <iostream>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <array>
#include <string>
#include <typeinfo>

#include <vulkan/vulkan_core.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui_impl_glfw.h>

namespace ve
{
    struct GlobalUbo
    {
        glm::mat4 projection{1.f};
        glm::mat4 view{1.f};
        glm::vec4 ambientLightColor{1.0f, 0.4f, 0.2f, 0.02f};
        glm::vec3 lightPosition{-1.f};
        alignas(16) glm::vec4 lightColor{1.f}; // color of light above
    };

    void App::init_imgui(VkCommandBuffer commandBuffer)
    {
        // 1: create descriptor pool for IMGUI
        //  the size of the pool is very oversize, but it's copied from imgui demo itself.

        imguiPool = veDescriptorPool::Builder(pDevice)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, 1000)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000)
                        .addPoolSize(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000)
                        .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
                        .setMaxSets(1000)
                        .build();

        // 2: initialize imgui library

        // this initializes the core structures of imgui
        ImGui::CreateContext();
        ImGuiIO *IO = &ImGui::GetIO();
        IO->WantCaptureMouse = true;
        IO->WantCaptureKeyboard = true;
        ImGui::StyleColorsDark();

        // this initializes imgui for SDL
        ImGui_ImplGlfw_InitForVulkan(pWindow.getGLFWwindow(), true);

        // this initializes imgui for Vulkan
        ImGui_ImplVulkan_InitInfo init_info = {};

        pDevice.createImGuiInitInfo(init_info);
        init_info.DescriptorPool = imguiPool->getDescriptorPool();

        ImGui_ImplVulkan_Init(&init_info, pRenderer.getSwapChainRenderPass());

        // execute a gpu command to upload imgui font textures
        // immediate_submit([&](VkCommandBuffer cmd)
        //                  { ImGui_ImplVulkan_CreateFontsTexture(cmd); });

        ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);

        // clear font textures from cpu data

        // vkDestroyDescriptorPool(pDevice.device(), imguiPool->getDescriptorPool(), nullptr);

        // add the destroy the imgui created structures
        // ImGui_ImplVulkan_Shutdown();
    }

    void App::render_imgui()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();

        bool show_window = true;
        static float sliderFloat = 0.0f;

        ImGui::Begin("Object"); // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)

        static int selectedItem = 0;

        static int selectedObject = 0;
        static std::vector<char *> items(gameObjects.size());

        for (int i = 0; i < gameObjects.size(); i++)
        {
            items[i] = new char[2];
            for (int j = 0; j < 1; j++)
            {
                items[i][j] = '0' + i;
            }
            items[i][1] = '\0';
        }

        ImGui::Combo("Objects", &selectedObject, items.data(), items.size(), 4);

        auto object = &gameObjects.at(selectedObject);
        // std::cout << translation << std::endl;
        ImGui::Text("Translation");
        ImGui::InputFloat3(" ", &object->transform.translation.x);
        ImGui::SliderFloat3("  ", &object->transform.translation.x, -10.0f, 10.0f);
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        ImGui::Text("Rotation");
        ImGui::InputFloat3("   ", &object->transform.rotation.x);
        ImGui::SliderFloat3("    ", &object->transform.rotation.x, -10.0f, 10.0f);
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        ImGui::Text("Scale");
        ImGui::InputFloat3("     ", &object->transform.scale.x);
        ImGui::SliderFloat3("      ", &object->transform.scale.x, -10.0f, 10.0f);
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        ImGui::End();
        ImGui::Render();
    }

    void App::close_imgui()
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    App::App()
    {
        globalPool = veDescriptorPool::Builder(pDevice)
                         .setMaxSets(veSwapChain::MAX_FRAMES_IN_FLIGHT)
                         .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, veSwapChain::MAX_FRAMES_IN_FLIGHT)
                         .build();

        loadGameObjects();
    }

    App::~App() {}

    void App::run()
    {
        std::vector<std::unique_ptr<veBuffer>> uboBuffers(veSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < uboBuffers.size(); i++)
        {
            uboBuffers[i] = std::make_unique<veBuffer>(
                pDevice,
                sizeof(GlobalUbo),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            uboBuffers[i]->map();
        }

        auto globalSetLayout = veDescriptorSetLayout::Builder(pDevice)
                                   .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                                   .build();

        std::vector<VkDescriptorSet> globalDescriptorSets(veSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < globalDescriptorSets.size(); i++)
        {
            auto bufferInfo = uboBuffers[i]->descriptorInfo();
            veDescriptorWriter(*globalSetLayout, *globalPool)
                .writeBuffer(0, &bufferInfo)
                .build(globalDescriptorSets[i]);
        }

        SimpleRenderSystem srs{pDevice, pRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()}; // srs - simpleRenderSystem
        PointLightSystem pls{pDevice, pRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};   // pls - pointLightSystem
        veCamera camera{};

        auto viewerObject = veGameObject::createGameObject();
        viewerObject.transform.translation.z = -2.5f;
        KeyboardMovementController cameraController{};

        auto currentTime = std::chrono::high_resolution_clock::now();

        bool firstFrame = true;

        while (!pWindow.shouldClose())
        {
            glfwPollEvents();
            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;

            cameraController.moveInPlaneXZ(pWindow.getGLFWwindow(), frameTime, viewerObject);
            camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

            float aspect = pRenderer.getAspectRatio();

            camera.setPerspectiveProjection(glm::radians(60.f), aspect, 0.01f, 20.f);

            if (auto commandBuffer = pRenderer.beginFrame())
            {
                if (firstFrame)
                {
                    init_imgui(commandBuffer);
                    firstFrame = false;
                }

                int frameIndex = pRenderer.getFrameIndex();
                FrameInfo frameInfo{frameIndex, frameTime, commandBuffer, camera, globalDescriptorSets[frameIndex], gameObjects};

                // updating buffers
                GlobalUbo ubo{};
                ubo.projection = camera.getProjectionMatrix();
                ubo.view = camera.getViewMatrix();
                uboBuffers[frameIndex]->writeToBuffer(&ubo);
                uboBuffers[frameIndex]->flush();

                // rendering
                render_imgui();
                pRenderer.beginSwapChainRenderPass(frameInfo.commandBuffer);
                srs.renderGameObjects(frameInfo);
                pls.render(frameInfo);
                pRenderer.endSwapChainRenderPass(frameInfo.commandBuffer);
                pRenderer.endFrame();
            }
        }

        vkDeviceWaitIdle(pDevice.device());
        close_imgui();
    }

    void App::loadGameObjects()
    {

        std::shared_ptr<veModel> model1 = veModel::createModelFromFile(pDevice, "/home/milk/Vulkan_APP/models/smooth_vase.obj");

        auto obj1 = veGameObject::createGameObject();
        obj1.model = model1;
        obj1.transform.translation = {-.5f, .5f, 0.f};
        obj1.transform.rotation = {.0f, .0f, 0.0f};
        obj1.transform.scale = {1.5f, 1.5f, 1.5f};

        std::shared_ptr<veModel> model2 = veModel::createModelFromFile(pDevice, "/home/milk/Vulkan_APP/models/pose.obj");

        auto obj2 = veGameObject::createGameObject();
        obj2.model = model2;
        obj2.transform.translation = {.5f, .5f, 0.f};
        obj2.transform.rotation = {.0f, .0f, 0.0f};
        obj2.transform.scale = {1.5f, 1.5f, 1.5f};

        std::shared_ptr<veModel> floorModel = veModel::createModelFromFile(pDevice, "/home/milk/Vulkan_APP/models/floor.obj");
        auto floor = veGameObject::createGameObject();
        floor.model = floorModel;
        floor.transform.translation = {0.f, .5f, 0.f};
        floor.transform.rotation = {0.f, 0.f, 0.f};
        floor.transform.scale = {3.f, 1.f, 3.f};
        std::cout << obj1.getId();

        gameObjects.emplace(obj1.getId(), std::move(floor));
        gameObjects.emplace(obj2.getId(), std::move(obj1));
        gameObjects.emplace(floor.getId(), std::move(obj2));
        // print gameObjects ids
    }

    void App::addGameObject(std::string &model)
    {
        auto obj = veGameObject::createGameObject();
        obj.model = veModel::createModelFromFile(pDevice, "/home/milk/Vulkan_APP/models/" + model);
        obj.transform.translation = {0.f, 0.f, 0.f};
        obj.transform.rotation = {0.f, 0.f, 0.f};
        obj.transform.scale = {1.f, 1.f, 1.f};
        gameObjects.emplace(obj.getId(), std::move(obj));
    }

} // namespace ve