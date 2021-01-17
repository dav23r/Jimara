#pragma once
#include "../VulkanDevice.h"
#include "VulkanRenderSurface.h"
#include "VulkanSwapChain.h"
#include "../Synch/VulkanSemaphore.h"
#include "../Synch/VulkanFence.h"

namespace Jimara {
	namespace Graphics {
		namespace Vulkan {
			class VulkanSurfaceRenderEngineInfo : public virtual SurfaceRenderEngineInfo {
			public:
				virtual size_t ImageCount()const = 0;

				virtual VkSampleCountFlagBits MSAASamples(GraphicsSettings::MSAA desired)const = 0;
			};

			class VulkanSurfaceRenderEngine : public SurfaceRenderEngine {
			public:
				VulkanSurfaceRenderEngine(VulkanDevice* device, VulkanWindowSurface* surface);

				virtual ~VulkanSurfaceRenderEngine();

				virtual void Update() override;

			private:
				Reference<VulkanDevice> m_device;
				Reference<VulkanWindowSurface> m_windowSurface;

				Reference<VulkanSwapChain> m_swapChain;
				
				std::vector<VulkanSemaphore> m_imageAvailableSemaphores;
				std::vector<VulkanSemaphore> m_renderFinishedSemaphores;
				std::vector<VulkanFence> m_inFlightFences;

				size_t m_semaphoreIndex;

				bool m_shouldRecreateComponents;

				void RecreateComponents();

				void SurfaceSizeChanged(VulkanWindowSurface*);
			};
		}
	}
}
