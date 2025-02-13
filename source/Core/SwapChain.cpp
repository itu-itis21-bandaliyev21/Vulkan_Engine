#include "Core/SwapChain.hpp"

namespace Nyxis
{
    SwapChain::SwapChain(VkExtent2D extent)
		: m_WindowExtent{extent}
	{
		m_WorldExtent = m_WindowExtent;
		Init();
	}

	SwapChain::SwapChain(VkExtent2D windowExtent, VkExtent2D worldExtent, std::shared_ptr<SwapChain> previous)
		: m_WindowExtent{ windowExtent }, m_WorldExtent(worldExtent), m_OldSwapChain{ previous }
	{
		Init();
		m_OldSwapChain = nullptr;
	}

	SwapChain::~SwapChain()
	{
		for (auto imageView : m_WorldImageViews)
		{
			vkDestroyImageView(device.device(), imageView, nullptr);
		}
		m_WorldImageViews.clear();

		for (auto imageView : m_SwapChainImageViews)
		{
			vkDestroyImageView(device.device(), imageView, nullptr);
		}
		m_SwapChainImageViews.clear();

		if (m_SwapChain != nullptr)
		{
			vkDestroySwapchainKHR(device.device(), m_SwapChain, nullptr);
			m_SwapChain = nullptr;
		}

		for (int i = 0; i < m_DepthImages.size(); i++)
		{
			vkDestroyImageView(device.device(), m_DepthImageViews[i], nullptr);
			vkDestroyImage(device.device(), m_DepthImages[i], nullptr);
			vkFreeMemory(device.device(), m_DepthImageMemories[i], nullptr);
		}

		for (auto framebuffer : m_SwapChainFramebuffers)
		{
			vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
		}

		for (auto framebuffer : m_WorldFramebuffers)
		{
			vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
		}

		vkDestroyRenderPass(device.device(), m_MainRenderPass, nullptr);
		vkDestroyRenderPass(device.device(), m_UIRenderPass, nullptr);

		// cleanup synchronization objects
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroySemaphore(device.device(), m_RenderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(device.device(), m_ImageAvailableSemaphores[i], nullptr);
			vkDestroyFence(device.device(), m_InFlightFences[i], nullptr);
		}
	}

	void SwapChain::Init()
	{
		CreateSwapChain();
		CreateWorldImages();
		CreateSwapChainImageViews();
		CreateWorldImageViews();
		CreateRenderPass();
		CreateDepthResources();
		CreateSwapChainFramebuffers();
		CreateWorldFramebuffers();
		CreateSyncObjects();
    	LOG_INFO("[Core] Successfully created swap chain.");
	}

	VkResult SwapChain::AcquireNextImage(uint32_t* imageIndex)
	{
		vkWaitForFences(
			device.device(),
			1,
			&m_InFlightFences[m_CurrentFrame],
			VK_TRUE,
			std::numeric_limits<uint64_t>::max());

		VkResult result = vkAcquireNextImageKHR(
			device.device(),
			m_SwapChain,
			std::numeric_limits<uint64_t>::max(),
			m_ImageAvailableSemaphores[m_CurrentFrame], // must be a not signaled semaphore
			VK_NULL_HANDLE,
			imageIndex);

		return result;
	}

	void SwapChain::SubmitWorldCommandBuffers(const VkCommandBuffer* buffers, uint32_t* imageIndex)
	{
		if (m_ImagesInFlight[*imageIndex] != VK_NULL_HANDLE)
		{
			vkWaitForFences(device.device(), 1, &m_ImagesInFlight[*imageIndex], VK_TRUE, UINT64_MAX);
		}

		m_ImagesInFlight[*imageIndex] = m_InFlightFences[m_CurrentFrame];

		// create signal and wait semaphores for the first command buffer
		VkSemaphore signalSemaphore = m_WorldImageAvailableSemaphores[m_CurrentFrame];	
		VkSemaphore waitSemaphores = m_ImageAvailableSemaphores[m_CurrentFrame];

		// submit info for the first command buffer
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &waitSemaphores;
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = buffers;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &signalSemaphore;

		// submit the first command buffer
		vkResetFences(device.device(), 1, &m_InFlightFences[m_CurrentFrame]);
		if (vkQueueSubmit(device.graphicsQueue(), 1, &submitInfo, m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit draw command buffer!");
		}
	}

	VkResult SwapChain::SubmitSwapChainCommandBuffers(
		const VkCommandBuffer* buffers, uint32_t* imageIndex)
	{
		if (m_ImagesInFlight[*imageIndex] != VK_NULL_HANDLE)
		{
			vkWaitForFences(device.device(), 1, &m_ImagesInFlight[*imageIndex], VK_TRUE, UINT64_MAX);
		}

		// create signal and wait semaphores for the second command buffer
		VkSemaphore waitSemaphores = m_WorldImageAvailableSemaphores[m_CurrentFrame];
		VkSemaphore signalSemaphore = m_RenderFinishedSemaphores[m_CurrentFrame];

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &waitSemaphores;
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = buffers;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &signalSemaphore;

		// submit the second command buffer
		if (vkQueueSubmit(device.graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		// present info
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &signalSemaphore;

		VkSwapchainKHR swapChains[] = {m_SwapChain};
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;

		presentInfo.pImageIndices = imageIndex;

		auto result = vkQueuePresentKHR(device.presentQueue(), &presentInfo);

		m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

		return result;
	}

	void SwapChain::CreateSwapChain()
	{
		SwapChainSupportDetails swapChainSupport = device.getSwapChainSupport();

		VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
    	VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 &&
			imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = device.surface();

		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices indices = device.findPhysicalQueueFamilies();
		uint32_t queueFamilyIndices[] = {indices.graphicsFamily, indices.presentFamily};

		if (indices.graphicsFamily != indices.presentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional
		}
		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		createInfo.presentMode = m_PresentMode;
    	createInfo.clipped = VK_TRUE;

		createInfo.oldSwapchain = m_OldSwapChain == nullptr ? VK_NULL_HANDLE : m_OldSwapChain->m_SwapChain;

		if (vkCreateSwapchainKHR(device.device(), &createInfo, nullptr, &m_SwapChain) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create swap chain!");
		}

		// we only specified a minimum number of images in the swap chain, so the implementation is
		// allowed to create a swap chain with more. That's why we'll first query the final number of
		// images with vkGetSwapchainImagesKHR, then resize the container and finally call it again to
		// retrieve the handles.
		vkGetSwapchainImagesKHR(device.device(), m_SwapChain, &imageCount, nullptr);
		m_SwapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device.device(), m_SwapChain, &imageCount, m_SwapChainImages.data());

		m_SwapChainImageFormat = surfaceFormat.format;
		m_SwapChainExtent = extent;
	}

	void insertImageMemoryBarrier(VkCommandBuffer cmdbuffer,
		VkImage image,
		VkAccessFlags srcAccessMask,
		VkAccessFlags dstAccessMask,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkPipelineStageFlags srcStageMask,
		VkPipelineStageFlags dstStageMask,
		VkImageSubresourceRange subresourceRange)
	{
		VkImageMemoryBarrier imageMemoryBarrier{};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.srcAccessMask = srcAccessMask;
		imageMemoryBarrier.dstAccessMask = dstAccessMask;
		imageMemoryBarrier.oldLayout = oldImageLayout;
		imageMemoryBarrier.newLayout = newImageLayout;
		imageMemoryBarrier.image = image;
		imageMemoryBarrier.subresourceRange = subresourceRange;

		vkCmdPipelineBarrier(
			cmdbuffer,
			srcStageMask,
			dstStageMask,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageMemoryBarrier);
	}

	void SwapChain::CreateWorldImages()
	{
		m_WorldImages.resize(m_SwapChainImages.size());
		m_WorldImageMemories.resize(m_SwapChainImages.size());

		m_IDImages.resize(m_SwapChainImages.size());
		m_IDImageMemories.resize(m_SwapChainImages.size());

		auto func = [&](std::vector<VkImage>& images, std::vector<VkDeviceMemory>& memories, VkFormat format, VkImageUsageFlags usage) {
			for (size_t i = 0; i < images.size(); i++) {
				auto commandBuffer = device.beginSingleTimeCommands();

				VkImageCreateInfo imageInfo = {};
				imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
				imageInfo.imageType = VK_IMAGE_TYPE_2D;
				imageInfo.format = format;
				imageInfo.extent.height = m_WorldExtent.height;
				imageInfo.extent.width = m_WorldExtent.width;
				imageInfo.extent.depth = 1;
				imageInfo.mipLevels = 1;
				imageInfo.arrayLayers = 1;
				imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
				imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
				imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | usage;
				imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

				if (vkCreateImage(device.device(), &imageInfo, nullptr, &images[i]) != VK_SUCCESS)
					throw std::runtime_error("failed to create world image!");

				VkMemoryRequirements memoryRequirements;
				vkGetImageMemoryRequirements(device.device(), images[i], &memoryRequirements);
				VkMemoryAllocateInfo allocInfo = {};
				allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				allocInfo.allocationSize = memoryRequirements.size;
				allocInfo.memoryTypeIndex = device.findMemoryType(memoryRequirements.memoryTypeBits,
				                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

				if (vkAllocateMemory(device.device(), &allocInfo, nullptr, &memories[i]) != VK_SUCCESS)
					throw std::runtime_error("failed to allocate world image memory!");

				if (vkBindImageMemory(device.device(), images[i], memories[i], 0) != VK_SUCCESS)
					throw std::runtime_error("failed to bind world image memory!");
				VkImageMemoryBarrier barrier = {};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barrier.srcAccessMask = 0;
				barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = images[i];
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = 1;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = 1;

				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1,
				                     &barrier);
				device.endSingleTimeCommands(commandBuffer);
			}
		};

		func(m_WorldImages, m_WorldImageMemories, m_SwapChainImageFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
		func(m_IDImages, m_IDImageMemories, VK_FORMAT_R32_UINT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	}

	void SwapChain::CreateSwapChainImageViews()
	{
		m_SwapChainImageViews.resize(m_SwapChainImages.size());

		for (size_t i = 0; i < m_WorldImages.size(); i++)
		{
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = m_SwapChainImages[i];
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = m_SwapChainImageFormat;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;


			if (vkCreateImageView(device.device(), &viewInfo, nullptr, &m_SwapChainImageViews[i]) !=
				VK_SUCCESS)
			{
				throw std::runtime_error("failed to create swap chain image image view!");
			}
		}
	}

	void SwapChain::CreateWorldImageViews()
	{
		m_WorldImageViews.resize(m_WorldImages.size());
		m_IDImageViews.resize(m_IDImages.size());

		auto func = [&](std::vector<VkImage>& images, std::vector<VkImageView>& imageViews, VkFormat format)
		{
			for (size_t i = 0; i < imageViews.size(); i++)
			{
				VkImageViewCreateInfo viewInfo{};
				viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				viewInfo.image = images[i];
				viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				viewInfo.format = format;
				viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				viewInfo.subresourceRange.baseMipLevel = 0;
				viewInfo.subresourceRange.levelCount = 1;
				viewInfo.subresourceRange.baseArrayLayer = 0;
				viewInfo.subresourceRange.layerCount = 1;

				if (vkCreateImageView(device.device(), &viewInfo, nullptr, &imageViews[i]) !=
				    VK_SUCCESS)
				{
					throw std::runtime_error("failed to create world image view!");
				}
			}
		};

		func(m_WorldImages, m_WorldImageViews, m_SwapChainImageFormat);
		func(m_IDImages, m_IDImageViews, VK_FORMAT_R32_UINT);
	}

	void SwapChain::CreateRenderPass()
	{
		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = FindDepthFormat();
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment = 2;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = GetSwapChainImageFormat();
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentDescription idAttachment = {};
		idAttachment.format = VK_FORMAT_R32_UINT;
		idAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		idAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		idAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		idAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		idAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		idAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		idAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		std::array<VkAttachmentReference, 2> colorAttachmentRefs;
		colorAttachmentRefs[0].attachment = 0;
		colorAttachmentRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		colorAttachmentRefs[1].attachment = 1;
		colorAttachmentRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = colorAttachmentRefs.size();
		subpass.pColorAttachments = colorAttachmentRefs.data();
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.srcAccessMask = 0;
		dependency.srcStageMask =
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstSubpass = 0;
		dependency.dstStageMask =
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask =
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		std::array<VkAttachmentDescription, 3> mainAttachments = {colorAttachment, idAttachment, depthAttachment};
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(mainAttachments.size());
		renderPassInfo.pAttachments = mainAttachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(device.device(), &renderPassInfo, nullptr, &m_MainRenderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create main render pass!");
		}

		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRefs[0];
		subpass.pDepthStencilAttachment = nullptr;

		VkAttachmentDescription uiAttachments[] = {colorAttachment};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = uiAttachments;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		// create ui render pass
		if (vkCreateRenderPass(device.device(), &renderPassInfo, nullptr, &m_UIRenderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create ui render pass!");
		}
	}

	void SwapChain::CreateSwapChainFramebuffers()
	{
		m_SwapChainFramebuffers.resize(ImageCount());

		for (int i = 0; i < ImageCount(); i++)
		{
			VkImageView attachments[1] = {m_SwapChainImageViews[i]};
			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_UIRenderPass; // ui rendering is done in ui render pass
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = m_SwapChainExtent.width;
			framebufferInfo.height = m_SwapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(
				device.device(),
				&framebufferInfo,
				nullptr,
				&m_SwapChainFramebuffers[i]) != VK_SUCCESS)
			{
				// TODO: ADD NYXIS ASSERTIONS
				throw std::runtime_error("failed to create swap chain framebuffer!");
			}
		}
	}

	void SwapChain::CreateWorldFramebuffers()
    {
		m_WorldFramebuffers.resize(ImageCount());

    	for (size_t i = 0; i < ImageCount(); i++)
		{
			VkImageView attachments[3] = { m_WorldImageViews[i], m_IDImageViews[i], m_DepthImageViews[i] };
			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_MainRenderPass; // all object rendering is done in world render pass
			framebufferInfo.attachmentCount = 3;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = m_WorldExtent.width;
			framebufferInfo.height = m_WorldExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(
				device.device(),
				&framebufferInfo,
				nullptr,
				&m_WorldFramebuffers[i]) != VK_SUCCESS)
			{
				// TODO: ADD NYXIS ASSERTIONS
				throw std::runtime_error("failed to create world framebuffer!");
			}
		}
    }

	void SwapChain::CreateDepthResources()
	{
		VkFormat depthFormat = FindDepthFormat();
		VkExtent2D swapChainExtent = GetSwapChainExtent();

		m_DepthImages.resize(ImageCount());
		m_DepthImageMemories.resize(ImageCount());
		m_DepthImageViews.resize(ImageCount());

		for (int i = 0; i < m_DepthImages.size(); i++)
		{
			VkImageCreateInfo imageInfo{};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.extent.width = m_WorldExtent.width;
			imageInfo.extent.height = m_WorldExtent.height;
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.format = depthFormat;
			imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageInfo.flags = 0;

			device.createImageWithInfo(
				imageInfo,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				m_DepthImages[i],
				m_DepthImageMemories[i]);

			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = m_DepthImages[i];
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = depthFormat;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(device.device(), &viewInfo, nullptr, &m_DepthImageViews[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create texture image view!");
			}
		}
	}

	void SwapChain::CreateSyncObjects()
	{
		m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
		m_ImagesInFlight.resize(ImageCount(), VK_NULL_HANDLE);

		m_WorldImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_WorldRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_WorldInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
		m_WorldImagesInFlight.resize(ImageCount(), VK_NULL_HANDLE);

		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &m_WorldImageAvailableSemaphore);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			if (vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) !=
				VK_SUCCESS ||
				vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) !=
				VK_SUCCESS ||
				vkCreateFence(device.device(), &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS ||
				vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &m_WorldImageAvailableSemaphores[i]) !=
				VK_SUCCESS ||
				vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &m_WorldRenderFinishedSemaphores[i]) !=
				VK_SUCCESS ||
				vkCreateFence(device.device(), &fenceInfo, nullptr, &m_WorldInFlightFences[i]) != VK_SUCCESS
			)
			{
				throw std::runtime_error("failed to create synchronization objects for a frame!");
			}
		}
	}

	VkSurfaceFormatKHR SwapChain::ChooseSwapSurfaceFormat(
		const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		for (const auto& availableFormat : availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
				availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	VkPresentModeKHR SwapChain::ChooseSwapPresentMode(
		const std::vector<VkPresentModeKHR>& availablePresentModes)
	{
		for (const auto &availablePresentMode : availablePresentModes)
		{
		    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		    {

		        LOG_INFO("[Core] Present mode : Mailbox");
		        return availablePresentMode;
		    }
		}
		
		for (const auto &availablePresentMode : availablePresentModes)
		{
		    if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
		    {
		        LOG_INFO("[Core] Present mode: Immediate");
		        return availablePresentMode;
		    }
		}

		LOG_INFO("[Core] Present mode: V - Sync");
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D SwapChain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			VkExtent2D actualExtent = m_WindowExtent;
			actualExtent.width = std::max(
				capabilities.minImageExtent.width,
				std::min(capabilities.maxImageExtent.width, actualExtent.width));
			actualExtent.height = std::max(
				capabilities.minImageExtent.height,
				std::min(capabilities.maxImageExtent.height, actualExtent.height));

			return actualExtent;
		}
	}

	void SwapChain::RecreateWorldImages()
	{
		vkDeviceWaitIdle(device.device());
		// destroy old images and image views

    	for (auto& imageView : m_WorldImageViews)
		{
			vkDestroyImageView(device.device(), imageView, nullptr);
		}

    	for (auto framebuffer : m_WorldFramebuffers)
		{
			vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
		}

		for (int i = 0; i < m_DepthImages.size(); i++)
		{
			vkDestroyImageView(device.device(), m_DepthImageViews[i], nullptr);
			vkDestroyImage(device.device(), m_DepthImages[i], nullptr);
			vkFreeMemory(device.device(), m_DepthImageMemories[i], nullptr);
		}
		
		for (auto& image : m_WorldImages)
		{
			vkDestroyImage(device.device(), image, nullptr);
		}

		for (auto& imageMemory : m_WorldImageMemories)
		{
			vkFreeMemory(device.device(), imageMemory, nullptr);
		}

		m_WorldImages.clear();
		m_WorldImageViews.clear();
		m_DepthImages.clear();
    	m_WorldFramebuffers.clear();
		m_WorldImageMemories.clear();

    	CreateWorldImages();
		CreateWorldImageViews();
    	CreateDepthResources();
		CreateWorldFramebuffers();
	}

	VkFormat SwapChain::FindDepthFormat() const
	{
		return device.findSupportedFormat(
			{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}
} // namespace 