#include "VulkanDynamicDataUpdater.h"


namespace Jimara {
	namespace Graphics {
		namespace Vulkan {
			VulkanDynamicDataUpdater::VulkanDynamicDataUpdater(VkDeviceHandle* device)
				: m_timeline(Object::Instantiate<VulkanTimelineSemaphore>(device, 0)), m_revision(0), m_lastKnownRevision(0) {}

			VulkanDynamicDataUpdater::~VulkanDynamicDataUpdater() {}

			namespace {
				inline bool UpdateLastKnownRevision(std::queue<std::pair<Reference<VulkanPrimaryCommandBuffer>, uint64_t>>& queue
					, std::atomic<uint64_t>& lastKnown, VulkanTimelineSemaphore* timeline, uint64_t lastSubmitted) {
					if (lastSubmitted <= lastKnown) return true;
					lastKnown = timeline->Count();
					while (queue.size() > 0 && queue.front().second <= lastKnown) queue.pop();
					return false;
				}
			}


			void VulkanDynamicDataUpdater::WaitForTimeline(VulkanCommandBuffer* commandBuffer) {
				if (m_revision <= m_lastKnownRevision) return;
				std::unique_lock<std::mutex> lock(m_lock);
				if (UpdateLastKnownRevision(m_updateBuffers, m_lastKnownRevision, m_timeline, m_revision)) return;
				commandBuffer->WaitForSemaphore(m_timeline, m_revision, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
			}

			void VulkanDynamicDataUpdater::Update(VulkanCommandBuffer* commandBuffer, Callback<VulkanCommandBuffer*> dataUpdateFn) {
				std::unique_lock<std::mutex> lock(m_lock);
				UpdateLastKnownRevision(m_updateBuffers, m_lastKnownRevision, m_timeline, m_revision);
				if (m_revision == (~((uint64_t)0))) {
					m_updateBuffers = std::queue<std::pair<Reference<VulkanPrimaryCommandBuffer>, uint64_t>>();
					m_timeline = Object::Instantiate<VulkanTimelineSemaphore>(commandBuffer->CommandPool()->Queue()->Device(), 0);
					m_revision = 0;
					m_lastKnownRevision = 0;
				}
				uint64_t waitValue = m_revision.fetch_add(1);
				Reference<VulkanPrimaryCommandBuffer> updateBuffer = commandBuffer->CommandPool()->CreatePrimaryCommandBuffer();
				updateBuffer->BeginRecording();
				updateBuffer->WaitForSemaphore(m_timeline, waitValue, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
				updateBuffer->SignalSemaphore(m_timeline, m_revision);
				dataUpdateFn(updateBuffer);
				updateBuffer->EndRecording();
				m_updateBuffers.push(std::pair<Reference<VulkanPrimaryCommandBuffer>, uint64_t>(updateBuffer, m_revision));
				updateBuffer->CommandPool()->Queue()->ExecuteCommandBuffer(updateBuffer);
				commandBuffer->WaitForSemaphore(m_timeline, m_revision, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
			}
		}
	}
}
