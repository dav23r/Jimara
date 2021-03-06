#include "VulkanDynamicTexture.h"
#include "../TextureViews/VulkanDynamicTextureView.h"


#pragma warning(disable: 26812)
#pragma warning(disable: 26110)
namespace Jimara {
	namespace Graphics {
		namespace Vulkan {
			VulkanDynamicTexture::VulkanDynamicTexture(VulkanDevice* device, TextureType type, PixelFormat format, Size3 size, uint32_t arraySize, bool generateMipmaps)
				: m_device(device), m_textureType(type), m_pixelFormat(format), m_textureSize(size), m_arraySize(arraySize)
				, m_mipLevels(generateMipmaps ? VulkanStaticTexture::CalculateSupportedMipLevels(device, format, size) : 1u), m_cpuMappedData(nullptr), m_updater(*device) {}

			VulkanDynamicTexture::~VulkanDynamicTexture() {}

			Texture::TextureType VulkanDynamicTexture::Type()const {
				return m_textureType;
			}

			Texture::PixelFormat VulkanDynamicTexture::ImageFormat()const {
				return m_pixelFormat;
			}

			Texture::Multisampling VulkanDynamicTexture::SampleCount()const {
				return Multisampling::SAMPLE_COUNT_1;
			}

			Size3 VulkanDynamicTexture::Size()const {
				return m_textureSize;
			}

			uint32_t VulkanDynamicTexture::ArraySize()const {
				return m_arraySize;
			}

			uint32_t VulkanDynamicTexture::MipLevels()const {
				return m_mipLevels;
			}

			VkFormat VulkanDynamicTexture::VulkanFormat()const {
				return NativeFormatFromPixelFormat(m_pixelFormat);
			}

			VulkanDevice* VulkanDynamicTexture::Device()const {
				return m_device;
			}

			Reference<TextureView> VulkanDynamicTexture::CreateView(TextureView::ViewType type
				, uint32_t baseMipLevel, uint32_t mipLevelCount
				, uint32_t baseArrayLayer, uint32_t arrayLayerCount) {
				return Object::Instantiate<VulkanDynamicTextureView>(this, type, baseMipLevel, mipLevelCount, baseArrayLayer, arrayLayerCount);
			}


			ImageTexture::CPUAccess VulkanDynamicTexture::HostAccess()const {
				return CPUAccess::CPU_WRITE_ONLY;
			}

			void* VulkanDynamicTexture::Map() {
				if (m_cpuMappedData != nullptr) return m_cpuMappedData;

				m_bufferLock.lock();

				if (m_stagingBuffer == nullptr)
					m_stagingBuffer = Object::Instantiate<VulkanStaticBuffer>(m_device
						, VulkanImage::BytesPerPixel(m_pixelFormat)
						, m_textureSize.x * m_textureSize.y * m_textureSize.z * m_arraySize
						, true
						, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

				m_cpuMappedData = m_stagingBuffer->Map();

				return m_cpuMappedData;
			}

			void VulkanDynamicTexture::Unmap(bool write) {
				if (m_cpuMappedData == nullptr) return;
				m_stagingBuffer->Unmap(write);
				m_cpuMappedData = nullptr;
				if (write) m_texture = nullptr;
				else m_stagingBuffer = nullptr;
				m_bufferLock.unlock();
			}

			Reference<VulkanStaticImage> VulkanDynamicTexture::GetStaticHandle(VulkanCommandBuffer* commandBuffer) {
				Reference<VulkanStaticTexture> texture = m_texture;
				if (texture != nullptr) {
					m_updater.WaitForTimeline(commandBuffer);
					commandBuffer->RecordBufferDependency(texture);
					return texture;
				}

				std::unique_lock<std::mutex> lock(m_bufferLock);
				if (m_texture == nullptr) {
					m_texture = Object::Instantiate<VulkanStaticTexture>(m_device, m_textureType, m_pixelFormat, m_textureSize, m_arraySize, m_mipLevels > 1
						, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
						| VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
						, Multisampling::SAMPLE_COUNT_1);
				}

				commandBuffer->RecordBufferDependency(m_texture);

				if (m_stagingBuffer == nullptr || m_cpuMappedData != nullptr) {
					m_updater.WaitForTimeline(commandBuffer);
					return m_texture;
				}

				m_updater.Update(commandBuffer, Callback<VulkanCommandBuffer*>(&VulkanDynamicTexture::UpdateData, this));

				return m_texture;
			}

			void VulkanDynamicTexture::UpdateData(VulkanCommandBuffer* commandBuffer) {
				m_texture->TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, m_mipLevels, 0, m_arraySize);

				VkBufferImageCopy region = {};
				{
					region.bufferOffset = 0;
					region.bufferRowLength = 0;
					region.bufferImageHeight = 0;

					region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					region.imageSubresource.mipLevel = 0;
					region.imageSubresource.baseArrayLayer = 0;
					region.imageSubresource.layerCount = m_arraySize;

					region.imageOffset = { 0, 0, 0 };
					region.imageExtent = { m_textureSize.x, m_textureSize.y, m_textureSize.z };
				}
				vkCmdCopyBufferToImage(*commandBuffer, *m_stagingBuffer, *m_texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

				m_texture->GenerateMipmaps(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
				commandBuffer->RecordBufferDependency(m_texture);
				commandBuffer->RecordBufferDependency(m_stagingBuffer);
				m_stagingBuffer = nullptr;
			}
		}
	}
}
#pragma warning(default: 26812)
#pragma warning(default: 26110)
