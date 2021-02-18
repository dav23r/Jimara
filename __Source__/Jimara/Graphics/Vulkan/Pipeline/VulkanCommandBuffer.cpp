#include "VulkanCommandBuffer.h"

namespace Jimara {
	namespace Graphics {
		namespace Vulkan {
			VulkanCommandBuffer::VulkanCommandBuffer(VulkanCommandPool* commandPool, VkCommandBuffer buffer)
				: m_commandPool(commandPool), m_commandBuffer(buffer) {}

			VulkanCommandBuffer::operator VkCommandBuffer()const { return m_commandBuffer; }

			VulkanCommandPool* VulkanCommandBuffer::CommandPool()const { return m_commandPool; }

			namespace {
				template<typename InfoType, typename WaitInfo>
				inline static void IncludeSemaphore(VkSemaphore semaphore, const WaitInfo& info, std::unordered_map<VkSemaphore, InfoType>& collection) {
					typename std::unordered_map<VkSemaphore, InfoType>::iterator it = collection.find(semaphore);
					if (it != collection.end()) {
						WaitInfo waitInfo = it->second;
						if (waitInfo.count < info.count)
							waitInfo.count = info.count;
						waitInfo.stageFlags |= info.stageFlags;
						it->second = waitInfo;
					}
					else collection[semaphore] = info;
				}
			}

			void VulkanCommandBuffer::WaitForSemaphore(VulkanSemaphore* semaphore, VkPipelineStageFlags waitStages) {
				IncludeSemaphore(*semaphore, WaitInfo(semaphore, 0, waitStages), m_semaphoresToWait);
			}

			void VulkanCommandBuffer::WaitForSemaphore(VulkanTimelineSemaphore* semaphore, uint64_t count, VkPipelineStageFlags waitStages) {
				IncludeSemaphore(*semaphore, WaitInfo(semaphore, count, waitStages), m_semaphoresToWait);
			}

			void VulkanCommandBuffer::SignalSemaphore(VulkanSemaphore* semaphore) {
				IncludeSemaphore(*semaphore, WaitInfo(semaphore, 0, 0), m_semaphoresToSignal);
			}

			void VulkanCommandBuffer::SignalSemaphore(VulkanTimelineSemaphore* semaphore, uint64_t count) {
				IncludeSemaphore(*semaphore, WaitInfo(semaphore, count, 0), m_semaphoresToSignal);
			}

			void VulkanCommandBuffer::RecordBufferDependency(Object* dependency) { 
				m_bufferDependencies.push_back(dependency); 
			}

			void VulkanCommandBuffer::GetSemaphoreDependencies(
				std::vector<VkSemaphore>& waitSemaphores, std::vector<uint64_t>& waitCounts, std::vector<VkPipelineStageFlags>& waitStages,
				std::vector<VkSemaphore>& signalSemaphores, std::vector<uint64_t>& signalCounts)const {
				for (std::unordered_map<VkSemaphore, WaitInfo>::const_iterator it = m_semaphoresToWait.begin(); it != m_semaphoresToWait.end(); ++it) {
					waitSemaphores.push_back(it->first);
					waitCounts.push_back(it->second.count);
					waitStages.push_back(it->second.stageFlags);
				}
				for (std::unordered_map<VkSemaphore, SemaphoreInfo>::const_iterator it = m_semaphoresToSignal.begin(); it != m_semaphoresToSignal.end(); ++it) {
					signalSemaphores.push_back(it->first);
					signalCounts.push_back(it->second.count);
				}
			}

			void VulkanCommandBuffer::Reset() {
				if (vkResetCommandBuffer(m_commandBuffer, 0) != VK_SUCCESS)
					m_commandPool->Queue()->Device()->Log()->Fatal("VulkanCommandBuffer - Can not reset command buffer!");
				
				m_semaphoresToWait.clear();
				m_semaphoresToSignal.clear();
				m_bufferDependencies.clear();
			}

			void VulkanCommandBuffer::BeginRecording() {
				VkCommandBufferBeginInfo beginInfo = {};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = 0; // Optional
				beginInfo.pInheritanceInfo = nullptr; // Optional
				if (vkBeginCommandBuffer(m_commandBuffer, &beginInfo) != VK_SUCCESS)
					m_commandPool->Queue()->Device()->Log()->Fatal("VulkanCommandBuffer - Failed to begin command buffer!");
			}

			void VulkanCommandBuffer::EndRecording() {
				if (vkEndCommandBuffer(m_commandBuffer) != VK_SUCCESS)
					m_commandPool->Queue()->Device()->Log()->Fatal("VulkanCommandBuffer - Failed to end command buffer!");
			}


			VulkanPrimaryCommandBuffer::VulkanPrimaryCommandBuffer(VulkanCommandPool* commandPool, VkCommandBuffer buffer)
				: VulkanCommandBuffer(commandPool, buffer), m_fence(commandPool->Queue()->Device()), m_running(false) {}

			VulkanPrimaryCommandBuffer::~VulkanPrimaryCommandBuffer() {
				Wait();
			}
			
			void VulkanPrimaryCommandBuffer::Reset() {
				Wait();
				VulkanCommandBuffer::Reset();
			}

			void VulkanPrimaryCommandBuffer::Wait() {
				bool expected = true;
				bool running = m_running.compare_exchange_strong(expected, false);
				if (running && expected) m_fence.WaitAndReset();
			}

			void VulkanPrimaryCommandBuffer::SumbitOnQueue(VkQueue queue) {
				// SubmitInfo needs the list of semaphores to wait for and their corresponding values:
				static thread_local std::vector<VkSemaphore> waitSemaphores;
				static thread_local std::vector<uint64_t> waitValues;
				static thread_local std::vector<VkPipelineStageFlags> waitStages;

				// SubmitInfo needs the list of semaphores to signal and their corresponding values:
				static thread_local std::vector<VkSemaphore> signalSemaphores;
				static thread_local std::vector<uint64_t> signalValues;

				// Clear semaphore lists and refill them from command buffer:
				{
					waitSemaphores.clear();
					waitValues.clear();
					waitStages.clear();
					signalSemaphores.clear();
					signalValues.clear();
					GetSemaphoreDependencies(waitSemaphores, waitValues, waitStages, signalSemaphores, signalValues);
				}

				// Submition:
				VkTimelineSemaphoreSubmitInfo timelineInfo = {};
				{
					timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
					timelineInfo.pNext = nullptr;

					timelineInfo.waitSemaphoreValueCount = static_cast<uint32_t>(waitValues.size());
					timelineInfo.pWaitSemaphoreValues = waitValues.data();

					timelineInfo.signalSemaphoreValueCount = static_cast<uint32_t>(signalValues.size());
					timelineInfo.pSignalSemaphoreValues = signalValues.data();
				}

				VkSubmitInfo submitInfo = {};
				VkCommandBuffer apiHandle = *this;
				{
					submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
					submitInfo.pNext = &timelineInfo;

					submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
					submitInfo.pWaitSemaphores = waitSemaphores.data();
					submitInfo.pWaitDstStageMask = waitStages.data();

					submitInfo.commandBufferCount = 1;
					submitInfo.pCommandBuffers = &apiHandle;

					submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
					submitInfo.pSignalSemaphores = signalSemaphores.data();
				}

				Wait();
				if (vkQueueSubmit(queue, 1, &submitInfo, m_fence) != VK_SUCCESS)
					CommandPool()->Queue()->Device()->Log()->Fatal("VulkanPrimaryCommandBuffer - Failed to submit command buffer!");
				else m_running = true;
			}

			VulkanSecondaryCommandBuffer::VulkanSecondaryCommandBuffer(VulkanCommandPool* commandPool, VkCommandBuffer buffer)
				: VulkanCommandBuffer(commandPool, buffer) {}
		}
	}
}