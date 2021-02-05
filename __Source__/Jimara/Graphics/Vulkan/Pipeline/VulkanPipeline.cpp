#include "VulkanPipeline.h"

#pragma warning(disable: 26812)
namespace Jimara {
	namespace Graphics {
		namespace Vulkan {
			namespace {
				inline static std::vector<VkDescriptorSetLayout> CreateDescriptorSetLayouts(VulkanDevice* device, const PipelineDescriptor* descriptor) {
					std::vector<VkDescriptorSetLayout> layouts;
					const size_t setCount = descriptor->BindingSetCount();
					for (size_t setIndex = 0; setIndex < setCount; setIndex++) {
						const PipelineDescriptor::BindingSetDescriptor* setDescriptor = descriptor->BindingSet(setIndex);

						static thread_local std::vector<VkDescriptorSetLayoutBinding> bindings;
						bindings.clear();

						auto addBinding = [](const PipelineDescriptor::BindingSetDescriptor::BindingInfo info, VkDescriptorType type) {
							VkDescriptorSetLayoutBinding binding = {};
							binding.binding = info.binding;
							binding.descriptorType = type;
							binding.descriptorCount = 1;
							binding.stageFlags = 
								(((info.stages & StageMask(PipelineStage::COMPUTE)) != 0) ? VK_SHADER_STAGE_COMPUTE_BIT : 0) |
								(((info.stages & StageMask(PipelineStage::VERTEX)) != 0) ? VK_SHADER_STAGE_VERTEX_BIT : 0) |
								(((info.stages & StageMask(PipelineStage::FRAGMENT)) != 0) ? VK_SHADER_STAGE_FRAGMENT_BIT : 0);
							binding.pImmutableSamplers = nullptr;
							bindings.push_back(binding);
						};

						{
							size_t count = setDescriptor->ConstantBufferCount();
							for (size_t i = 0; i < count; i++)
								addBinding(setDescriptor->ConstantBufferInfo(i), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
						}
						{
							size_t count = setDescriptor->TextureSamplerCount();
							for (size_t i = 0; i < count; i++)
								addBinding(setDescriptor->TextureSamplerInfo(i), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
						}
						if (bindings.size() <= 0) continue;

						VkDescriptorSetLayoutCreateInfo layoutInfo = {};
						{
							layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
							layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
							layoutInfo.pBindings = bindings.data();
						}
						VkDescriptorSetLayout layout;
						if (vkCreateDescriptorSetLayout(*device, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
							device->Log()->Fatal("VulkanPipeline - Failed to create descriptor set layout!");
							layout = VK_NULL_HANDLE;
						}
						layouts.push_back(layout);
					}
					return layouts;
				}

				inline static VkPipelineLayout CreateVulkanPipelineLayout(VulkanDevice* device, const std::vector<VkDescriptorSetLayout>& setLayouts) {
					VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
					{
						pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
						pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
						pipelineLayoutInfo.pSetLayouts = (setLayouts.size() > 0) ? setLayouts.data() : nullptr;
						pipelineLayoutInfo.pushConstantRangeCount = 0;
					}
					VkPipelineLayout pipelineLayout;
					if (vkCreatePipelineLayout(*device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
						device->Log()->Fatal("VulkanPipeline - Failed to create pipeline layout!");
						return VK_NULL_HANDLE;
					}
					return pipelineLayout;
				}


				inline static VkDescriptorPool CreateDescriptorPool(VulkanDevice* device, const PipelineDescriptor* descriptor, size_t maxInFlightCommandBuffers) {
					VkDescriptorPoolSize sizes[2];

					uint32_t sizeCount = 0;
					uint32_t constantBufferCount = 0;
					uint32_t textureSamplerCount = 0;

					const size_t setCount = descriptor->BindingSetCount();
					for (size_t setIndex = 0; setIndex < setCount; setIndex++) {
						const PipelineDescriptor::BindingSetDescriptor* setDescriptor = descriptor->BindingSet(setIndex);
						if (setDescriptor->SetByEnvironment()) continue;

						constantBufferCount += static_cast<uint32_t>(setDescriptor->ConstantBufferCount());
						textureSamplerCount += static_cast<uint32_t>(setDescriptor->TextureSamplerCount());
					}

					{
						VkDescriptorPoolSize& size = sizes[sizeCount];
						size = {};
						size.descriptorCount = constantBufferCount * static_cast<uint32_t>(maxInFlightCommandBuffers);
						size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
						if (size.descriptorCount > 0) sizeCount++;
					}
					{
						VkDescriptorPoolSize& size = sizes[sizeCount];
						size = {};
						size.descriptorCount = textureSamplerCount * static_cast<uint32_t>(maxInFlightCommandBuffers);
						size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						if (size.descriptorCount > 0) sizeCount++;
					}
					if (sizeCount <= 0) return VK_NULL_HANDLE;

					VkDescriptorPoolCreateInfo createInfo = {};
					{
						createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
						createInfo.poolSizeCount = sizeCount;
						createInfo.pPoolSizes = (sizeCount > 0) ? sizes : nullptr;
						createInfo.maxSets = static_cast<uint32_t>(setCount * maxInFlightCommandBuffers);
					}
					VkDescriptorPool pool;
					if (vkCreateDescriptorPool(*device, &createInfo, nullptr, &pool) != VK_SUCCESS) {
						pool = VK_NULL_HANDLE;
						device->Log()->Fatal("VulkanPipeline - Failed to create descriptor pool!");
					}
					return pool;
				}

				inline static std::vector<VkDescriptorSet> CreateDescriptorSets(
					VulkanDevice* device, PipelineDescriptor* descriptor, size_t maxInFlightCommandBuffers
					, VkDescriptorPool pool, const std::vector<VkDescriptorSetLayout>& setLayouts) {

					static thread_local std::vector<VkDescriptorSetLayout> layouts;

					const uint32_t maxSetCount = static_cast<uint32_t>(maxInFlightCommandBuffers * setLayouts.size());
					if (layouts.size() < maxSetCount)
						layouts.resize(maxSetCount);

					uint32_t setCountPerCommandBuffer = 0;
					for (size_t i = 0; i < setLayouts.size(); i++)
						if (!descriptor->BindingSet(i)->SetByEnvironment()) {
							layouts[setCountPerCommandBuffer] = setLayouts[i];
							setCountPerCommandBuffer++;
						}

					for (size_t i = 1; i < maxInFlightCommandBuffers; i++)
						for (size_t j = 0; j < setCountPerCommandBuffer; j++)
							layouts[(i * setCountPerCommandBuffer) + j] = layouts[j];
					
					const uint32_t setCount = static_cast<uint32_t>(maxInFlightCommandBuffers * setCountPerCommandBuffer);
					if (setCount <= 0) return std::vector<VkDescriptorSet>();

					std::vector<VkDescriptorSet> sets(setCount);

					VkDescriptorSetAllocateInfo allocInfo = {};
					{
						allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
						allocInfo.descriptorPool = pool;
						allocInfo.descriptorSetCount = setCount;
						allocInfo.pSetLayouts = layouts.data();
					}
					if (vkAllocateDescriptorSets(*device, &allocInfo, sets.data()) != VK_SUCCESS) {
						device->Log()->Fatal("VulkanPipeline - Failed to allocate descriptor sets!");
						sets.clear();
					}
					return sets;
				}


				inline static void PrepareCache(const PipelineDescriptor* descriptor, size_t maxInFlightCommandBuffers
					, std::vector<Reference<VulkanPipelineConstantBuffer>>& constantBuffers
					, std::vector<Reference<VulkanStaticImageSampler>>& samplerRefs) {
					
					size_t constantBufferCount = 0;
					size_t textureSamplerCount = 0;
					
					const size_t setCount = descriptor->BindingSetCount();
					for (size_t setIndex = 0; setIndex < setCount; setIndex++) {
						const PipelineDescriptor::BindingSetDescriptor* setDescriptor = descriptor->BindingSet(setIndex);
						if (setDescriptor->SetByEnvironment()) continue;
						constantBufferCount += setDescriptor->ConstantBufferCount();
						textureSamplerCount += setDescriptor->TextureSamplerCount();
					}

					constantBuffers.resize(constantBufferCount);
					samplerRefs.resize(textureSamplerCount * maxInFlightCommandBuffers);
				}
			}

			VulkanPipeline::VulkanPipeline(VulkanDevice* device, PipelineDescriptor* descriptor, size_t maxInFlightCommandBuffers)
				: m_device(device), m_descriptor(descriptor), m_commandBufferCount(maxInFlightCommandBuffers)
				, m_descriptorPool(VK_NULL_HANDLE), m_pipelineLayout(VK_NULL_HANDLE) {
				
				m_descriptorSetLayouts = CreateDescriptorSetLayouts(m_device, m_descriptor);
				m_pipelineLayout = CreateVulkanPipelineLayout(m_device, m_descriptorSetLayouts);

				m_descriptorPool = CreateDescriptorPool(m_device, m_descriptor, m_commandBufferCount);
				m_descriptorSets = CreateDescriptorSets(m_device, m_descriptor, m_commandBufferCount, m_descriptorPool, m_descriptorSetLayouts);

				PrepareCache(m_descriptor, m_commandBufferCount, m_descriptorCache.constantBuffers, m_descriptorCache.samplers);

				m_bindingRanges.resize(m_commandBufferCount);
				if (m_commandBufferCount > 0) {
					const size_t setsPerCommandBuffer = (m_descriptorSets.size() / m_commandBufferCount);
					const size_t setCount = descriptor->BindingSetCount();
					bool shouldStartNew = true;
					uint32_t setId = 0;
					for (size_t i = 0; i < setCount; i++) {
						const PipelineDescriptor::BindingSetDescriptor* setDescriptor = descriptor->BindingSet(i);
						if (setDescriptor->SetByEnvironment()) {
							shouldStartNew = true;
							continue;
						}
						if (shouldStartNew) {
							for (size_t buffer = 0; buffer < m_commandBufferCount; buffer++) {
								DescriptorBindingRange range;
								range.start = i;
								m_bindingRanges[buffer].push_back(range);
							}
							shouldStartNew = false;
						}
						for (size_t buffer = 0; buffer < m_commandBufferCount; buffer++)
							m_bindingRanges[buffer].back().sets.push_back(m_descriptorSets[(setsPerCommandBuffer * buffer) + setId]);
						setId++;
					}
				}
			}

			VulkanPipeline::~VulkanPipeline() {
				m_bindingRanges.clear();
				if (m_pipelineLayout != VK_NULL_HANDLE) {
					vkDestroyPipelineLayout(*m_device, m_pipelineLayout, nullptr);
					m_pipelineLayout = VK_NULL_HANDLE;
				}
				if (m_descriptorPool != VK_NULL_HANDLE) {
					vkDestroyDescriptorPool(*m_device, m_descriptorPool, nullptr);
					m_descriptorPool = VK_NULL_HANDLE;
				}
				for (size_t i = 0; i < m_descriptorSetLayouts.size(); i++)
					vkDestroyDescriptorSetLayout(*m_device, m_descriptorSetLayouts[i], nullptr);
				m_descriptorSetLayouts.clear();
			}

			VkPipelineLayout VulkanPipeline::PipelineLayout()const { 
				return m_pipelineLayout; 
			}

			void VulkanPipeline::UpdateDescriptors(VulkanCommandRecorder* recorder) {
				static thread_local std::vector<VkWriteDescriptorSet> updates;

				static thread_local std::vector<VkDescriptorBufferInfo> bufferInfos;
				if (bufferInfos.size() < m_descriptorCache.constantBuffers.size() * m_commandBufferCount)
					bufferInfos.resize(m_descriptorCache.constantBuffers.size() * m_commandBufferCount);

				static thread_local std::vector<VkDescriptorImageInfo> samplerInfos;
				if (samplerInfos.size() < m_descriptorCache.samplers.size())
					samplerInfos.resize(m_descriptorCache.samplers.size());

				if(m_commandBufferCount > 0) {
					const size_t commandBufferIndex = recorder->CommandBufferIndex();
					const size_t setsPerCommandBuffer = (m_descriptorSets.size() / m_commandBufferCount);

					size_t constantBufferId = 0;
					auto addConstantBuffers = [&](PipelineDescriptor::BindingSetDescriptor* setDescriptor, size_t setIndex) {
						size_t cbufferCount = setDescriptor->ConstantBufferCount();
						for (size_t cbufferId = 0; cbufferId < cbufferCount; cbufferId++) {
							Reference<VulkanConstantBuffer> buffer = setDescriptor->ConstantBuffer(cbufferId);
							Reference<VulkanPipelineConstantBuffer>& pipelineBuffer = m_descriptorCache.constantBuffers[constantBufferId];
							
							if (pipelineBuffer == nullptr || pipelineBuffer->TargetBuffer() != buffer) {
								pipelineBuffer = (buffer == nullptr) ? nullptr : Object::Instantiate<VulkanPipelineConstantBuffer>(m_device, buffer, m_commandBufferCount);

								uint32_t binding = setDescriptor->ConstantBufferInfo(cbufferId).binding;
								for (size_t i = 0; i < m_commandBufferCount; i++) {
									VkDescriptorBufferInfo& bufferInfo = bufferInfos[(i * m_descriptorCache.constantBuffers.size()) + constantBufferId];
									bufferInfo = {};
									bufferInfo.buffer = (pipelineBuffer != nullptr) ? pipelineBuffer->GetBuffer(i) : nullptr;
									bufferInfo.offset = 0;
									bufferInfo.range = VK_WHOLE_SIZE;

									VkWriteDescriptorSet write = {};
									write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
									write.dstSet = m_descriptorSets[(setsPerCommandBuffer * i) + setIndex];
									write.dstBinding = binding;
									write.dstArrayElement = 0;
									write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
									write.descriptorCount = 1;
									write.pBufferInfo = &bufferInfo;

									updates.push_back(write);
								}
							}
							else pipelineBuffer->GetBuffer(commandBufferIndex);

							if (pipelineBuffer != nullptr) recorder->RecordBufferDependency(pipelineBuffer);

							constantBufferId++;
						}
					};

					size_t samplerCacheIndex = commandBufferIndex;
					auto addSamplers = [&](PipelineDescriptor::BindingSetDescriptor* setDescriptor, VkDescriptorSet set) {
						size_t samplerCount = setDescriptor->TextureSamplerCount();
						for (size_t samplerId = 0; samplerId < samplerCount; samplerId++) {
							Reference<VulkanImageSampler> sampler = setDescriptor->Sampler(samplerId);
							Reference<VulkanStaticImageSampler> staticSampler = (sampler != nullptr) ? sampler->GetStaticHandle(recorder) : nullptr;
							Reference<VulkanStaticImageSampler>& cachedSampler = m_descriptorCache.samplers[samplerCacheIndex];
							if (cachedSampler != staticSampler) {
								cachedSampler = staticSampler;

								VkDescriptorImageInfo& samplerInfo = samplerInfos[samplerCacheIndex];
								samplerInfo = {};
								samplerInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
								samplerInfo.imageView = *dynamic_cast<VulkanStaticImageView*>(cachedSampler->TargetView());
								samplerInfo.sampler = *cachedSampler;

								VkWriteDescriptorSet write = {};
								write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
								write.dstSet = set;
								write.dstBinding = setDescriptor->TextureSamplerInfo(samplerId).binding;
								write.dstArrayElement = 0;
								write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
								write.descriptorCount = 1;
								write.pImageInfo = &samplerInfo;

								updates.push_back(write);
							}
							samplerCacheIndex += m_commandBufferCount;
						}
					};

					VkDescriptorSet* sets = m_descriptorSets.data() + (setsPerCommandBuffer * commandBufferIndex);
					const size_t setCount = m_descriptor->BindingSetCount();
					size_t setIndex = 0;
					for (size_t i = 0; i < setCount; i++) {
						PipelineDescriptor::BindingSetDescriptor* setDescriptor = m_descriptor->BindingSet(i);
						if (setDescriptor->SetByEnvironment()) continue;
						VkDescriptorSet set = sets[setIndex];
						addConstantBuffers(setDescriptor, setIndex);
						addSamplers(setDescriptor, set);
						setIndex++;
					}
				}
				if (updates.size() > 0) {
					vkUpdateDescriptorSets(*m_device, static_cast<uint32_t>(updates.size()), updates.data(), 0, nullptr);
					updates.clear();
				}
			}

			void VulkanPipeline::SetDescriptors(VulkanCommandRecorder* recorder, VkPipelineBindPoint bindPoint) {
				const size_t commandBufferIndex = recorder->CommandBufferIndex();
				const VkCommandBuffer commandBuffer = recorder->CommandBuffer();

				const std::vector<DescriptorBindingRange>& ranges = m_bindingRanges[commandBufferIndex];

				for (size_t i = 0; i < ranges.size(); i++) {
					const DescriptorBindingRange& range = ranges[i];
					vkCmdBindDescriptorSets(commandBuffer, bindPoint, m_pipelineLayout, range.start, static_cast<uint32_t>(range.sets.size()), range.sets.data(), 0, nullptr);
				}
			}
		}
	}
}
#pragma warning(default: 26812)
