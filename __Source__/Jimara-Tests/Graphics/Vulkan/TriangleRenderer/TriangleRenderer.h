#pragma once
#include "Graphics/Vulkan/Rendering/VulkanRenderEngine.h"
#include "Core/Stopwatch.h"
#include <thread>

namespace Jimara {
	namespace Graphics {
		namespace Vulkan {
			class TriangleRenderer : public virtual VulkanImageRenderer {
			public:
				/// <summary>
				/// Constructor
				/// </summary>
				/// <param name="device"> "Owner" device </param>
				TriangleRenderer(VulkanDevice* device);

				/// <summary> Virtual destructor </summary>
				virtual ~TriangleRenderer();

				/// <summary>
				/// Creates RenderEngine-specific data (normally requested exclusively by VulkanRenderEngine objects)
				/// </summary>
				/// <param name="engineInfo"> Render engine information </param>
				/// <returns> New instance of an EngineData object </returns>
				virtual Reference<EngineData> CreateEngineData(VulkanRenderEngineInfo* engineInfo) override;

				/// <summary> Shader cache </summary>
				Graphics::ShaderCache* ShaderCache()const;

				/// <summary> Cbuffer </summary>
				Buffer* ConstantBuffer()const;

				/// <summary> Triangle texture sampler </summary>
				TextureSampler* Sampler()const;

				/// <summary> Vertex position buffer </summary>
				VertexBuffer* PositionBuffer();

				/// <summary> Instance position offset buffer </summary>
				InstanceBuffer* InstanceOffsetBuffer();


			protected:
				/// <summary>
				/// Should record all rendering commands via commandRecorder
				/// </summary>
				/// <param name="engineData"> RenderEngine-specific data </param>
				/// <param name="commandRecorder"> Command recorder </param>
				virtual void Render(EngineData* engineData, VulkanCommandRecorder* commandRecorder) override;


			private:
				Reference<VulkanDevice> m_device;
				Reference<Graphics::ShaderCache> m_shaderCache;

				BufferReference<float> m_cbuffer;

				Reference<ImageTexture> m_texture;
				Reference<TextureSampler> m_sampler;

				class VertexPositionBuffer : public virtual VertexBuffer {
				private:
					BufferArrayReference<Vector2> m_buffer;

				public:
					VertexPositionBuffer(GraphicsDevice* device);

					virtual Reference<ArrayBuffer> Buffer() override;

					virtual size_t AttributeCount()const override;

					virtual AttributeInfo Attribute(size_t index)const override;

					virtual size_t BufferElemSize()const override;
				} m_positionBuffer;

				class InstanceOffsetBuffer : public virtual InstanceBuffer {
				private:
					BufferArrayReference<Vector2> m_buffer;

				public:
					InstanceOffsetBuffer(GraphicsDevice* device);

					virtual Reference<ArrayBuffer> Buffer() override;

					virtual size_t AttributeCount()const override;

					virtual AttributeInfo Attribute(size_t index)const override;

					virtual size_t BufferElemSize()const override;
				} m_instanceOffsetBuffer;


				volatile bool m_rendererAlive;

				std::thread m_imageUpdateThread;
			};
		}
	}
}