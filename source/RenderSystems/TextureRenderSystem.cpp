#include "RenderSystems/TextureRenderSystem.hpp"
#include "Core/FrameInfo.hpp"
#include "Core/SwapChain.hpp"

namespace Nyxis
{
	struct TexturePushConstantData {
		glm::mat4 modelMatrix{1.f};
		glm::mat4 normalMatrix{1.f};
		float roughness{0.0f};
	};

	TextureRenderSystem::TextureRenderSystem(VkRenderPass RenderPass, VkDescriptorSetLayout globalDescriptorSetLayout)
	{
		m_TexturePool.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
		auto framePoolBuilder = DescriptorPool::Builder()
			.setMaxSets(1000)
			.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000)
			.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000)
			.setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
		for (auto& pool : m_TexturePool) 
		{
			pool = framePoolBuilder.build();
		}

		CreatePipelineLayout(globalDescriptorSetLayout);
		CreatePipeline(RenderPass);
	}

	TextureRenderSystem::~TextureRenderSystem()
	{
		vkDestroyPipelineLayout(pDevice.device(), m_PipelineLayout, nullptr);
	}

	void TextureRenderSystem::OnUpdate()
	{

	}

	void TextureRenderSystem::Render(FrameInfo& frameInfo)
	{
		m_TexturePool[frameInfo.frameIndex]->resetPool();

		pPipeline->Bind(frameInfo.commandBuffer);

		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			m_PipelineLayout,
			0,
			1,
			&frameInfo.globalDescriptorSet,
			0,
			nullptr);

		// frameInfo.scene->m_Registry.view<RigidBody, MeshComponent, _Texture>().each([&](auto entity, auto& rigidBody, auto& mesh, auto& texture)
		// {
		//   auto& model = *mesh.model;
		// 	  if(model.loaded)
		// 	  {
		// 		  auto imageInfo = texture.GetDescriptorImageInfo();
		// 		  VkDescriptorSet descriptorSet;
		// 		  DescriptorWriter(*m_TextureSetLayout, *m_TexturePool[frameInfo.frameIndex])
		// 		        .writeImage(0, &imageInfo)
		// 			    .build(descriptorSet);
		//
		// 		  vkCmdBindDescriptorSets(
		// 			  frameInfo.commandBuffer,
		// 			  VK_PIPELINE_BIND_POINT_GRAPHICS,
		// 			  m_PipelineLayout,
		// 			  1,  // first set
		// 			  1,  // set count
		// 			  &descriptorSet,
		// 			  0,
		// 			  nullptr);
		//
		// 		  TexturePushConstantData push{};
		// 		  push.modelMatrix = rigidBody.mat4 ();
		// 		  push.normalMatrix = rigidBody.normalMatrix ();
		// 		  push.roughness = rigidBody.roughness;
		//
		// 		  vkCmdPushConstants (
		// 			  frameInfo.commandBuffer,
		// 			  m_PipelineLayout,
		// 			  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		// 		  0,
		// 		  sizeof (TexturePushConstantData),
		// 		  &push);
		//
		// 	  model.bind (frameInfo.commandBuffer);
		// 	  model.draw (frameInfo.commandBuffer);
		//   }
		// });

	}

	void TextureRenderSystem::Cleanup()
	{

	}

	void TextureRenderSystem::CreatePipelineLayout(VkDescriptorSetLayout globalDescriptorSetLayout)
	{
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(TexturePushConstantData);

		m_TextureSetLayout =
			DescriptorSetLayout::Builder()
				.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.build();

		std::vector<VkDescriptorSetLayout> descriptorSetLayouts{
			globalDescriptorSetLayout,
			m_TextureSetLayout->getDescriptorSetLayout()};

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
		pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(pDevice.device(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout) !=
			VK_SUCCESS) {
			throw std::runtime_error("failed to create pipeline layout!");
		}
	}

	void TextureRenderSystem::CreatePipeline(VkRenderPass renderPass)
	{
		pPipeline = std::make_unique<Pipeline>(
			"../shaders/texture_shader.vert.spv",
			"../shaders/texture_shader.frag.spv"
			);
		auto pipelineConfig = pPipeline->GetConfig();
		Pipeline::DefaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = m_PipelineLayout;
		pPipeline->Create();
	}
}
