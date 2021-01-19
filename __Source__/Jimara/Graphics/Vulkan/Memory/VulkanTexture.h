#pragma once
#include "../../Memory/Texture.h"
#include "../VulkanDevice.h"

namespace Jimara {
	namespace Graphics {
		namespace Vulkan {
			/// <summary> Basic VkImage wrapper </summary>
			class VulkanImage : public virtual Texture {
			public:
				/// <summary> Type cast to API object </summary>
				virtual operator VkImage()const = 0;

				/// <summary> Vulkan color format </summary>
				virtual VkFormat Format()const = 0;

				/// <summary> "Owner" device </summary>
				virtual VulkanDevice* Device()const = 0;
			};
		}
	}
}