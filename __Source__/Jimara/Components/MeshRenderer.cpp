#include "MeshRenderer.h"
#include "../Graphics/Data/GraphicsPipelineSet.h"

namespace Jimara {
	namespace {
		struct InstancedBatchDesc {
			Reference<GraphicsContext> context;
			Reference<const TriMesh> mesh;
			Reference<const Material> material;
			bool isStatic;

			inline InstancedBatchDesc(GraphicsContext* ctx = nullptr, const TriMesh* geometry = nullptr, const Material* mat = nullptr, bool stat = false)
				: context(ctx), mesh(geometry), material(mat), isStatic(stat) {}

			inline bool operator<(const InstancedBatchDesc& desc)const {
				if (context < desc.context) return true;
				else if (context > desc.context) return false;
				else if (mesh < desc.mesh) return true;
				else if (mesh > desc.mesh) return false;
				else if (material < desc.material) return true;
				else if (material > desc.material) return false;
				else return isStatic < desc.isStatic;
			}

			inline bool operator==(const InstancedBatchDesc& desc)const {
				return (context == desc.context) && (mesh == desc.mesh) && (material == desc.material) && (isStatic == desc.isStatic);
			}
		};
	}
}

namespace std {
	template<>
	struct hash<Jimara::InstancedBatchDesc> {
		size_t operator()(const Jimara::InstancedBatchDesc& desc)const {
			size_t ctxHash = std::hash<Jimara::GraphicsContext*>()(desc.context);
			size_t meshHash = std::hash<const Jimara::TriMesh*>()(desc.mesh);
			size_t matHash = std::hash<const Jimara::Material*>()(desc.material);
			size_t staticHash = std::hash<bool>()(desc.isStatic);
			auto combine = [](size_t a, size_t b) {
				return a ^ (b + 0x9e3779b9 + (a << 6) + (a >> 2));
			};
			return combine(combine(ctxHash, combine(meshHash, matHash)), staticHash);
		}
	};
}

namespace Jimara {
	namespace {
#pragma warning(disable: 4250)
		class MeshRenderPipelineDescriptor 
			: public virtual ObjectCache<InstancedBatchDesc>::StoredObject
			, public virtual Graphics::GraphicsPipeline::Descriptor
			, public virtual GraphicsContext::GraphicsObjectSynchronizer {
		private:
			const InstancedBatchDesc m_desc;
			const Graphics::PipelineDescriptor::BindingSetDescriptor* const m_environmentBinding;

			class CapturedMaterial : public virtual Graphics::PipelineDescriptor::BindingSetDescriptor {
			private:
				const Reference<const Material> m_material;
				Reference<Graphics::Shader> m_vertexShader;
				Reference<Graphics::Shader> m_fragmentShader;
				std::vector<Reference<Graphics::Buffer>> m_constantBuffers;
				std::vector<Reference<Graphics::ArrayBuffer>> m_structuredBuffers;
				std::vector<Reference<Graphics::TextureSampler>> m_textureSamplers;

			public:
				inline void Update() {
					m_vertexShader = m_material->VertexShader();
					m_fragmentShader = m_material->FragmentShader();
					for (size_t i = 0; i < m_constantBuffers.size(); i++)
						m_constantBuffers[i] = m_material->ConstantBuffer(i);
					for (size_t i = 0; i < m_structuredBuffers.size(); i++)
						m_structuredBuffers[i] = m_material->StructuredBuffer(i);
					for (size_t i = 0; i < m_textureSamplers.size(); i++)
						m_textureSamplers[i] = m_material->Sampler(i);
				}

				inline CapturedMaterial(const Material* material)
					: m_material(material) {
					m_constantBuffers.resize(m_material->ConstantBufferCount());
					m_structuredBuffers.resize(m_material->StructuredBufferCount());
					m_textureSamplers.resize(m_material->TextureSamplerCount());
					Update();
				}

				inline Graphics::Shader* VertexShader()const { return m_vertexShader; }
				inline Graphics::Shader* FragmentShader()const { return m_fragmentShader; }

				inline virtual bool SetByEnvironment()const override { return m_material->SetByEnvironment(); }

				inline virtual size_t ConstantBufferCount()const override { return m_constantBuffers.size(); }
				inline virtual BindingInfo ConstantBufferInfo(size_t index)const override { return m_material->ConstantBufferInfo(index); }
				inline virtual Reference<Graphics::Buffer> ConstantBuffer(size_t index)const override { return m_constantBuffers[index]; }

				inline virtual size_t StructuredBufferCount()const override { return(m_structuredBuffers.size()); }
				inline virtual BindingInfo StructuredBufferInfo(size_t index)const override { return m_material->StructuredBufferInfo(index); }
				inline virtual Reference<Graphics::ArrayBuffer> StructuredBuffer(size_t index)const override { return m_structuredBuffers[index]; }

				inline virtual size_t TextureSamplerCount()const override { return m_textureSamplers.size(); }
				inline virtual BindingInfo TextureSamplerInfo(size_t index)const override { return m_material->TextureSamplerInfo(index); }
				inline virtual Reference<Graphics::TextureSampler> Sampler(size_t index)const override { return m_textureSamplers[index]; }
			} m_capturedMaterial;

			// Mesh data:
			class MeshBuffers : public virtual Graphics::VertexBuffer {
			private:
				const Reference<Graphics::GraphicsMesh> m_graphicsMesh;
				Graphics::ArrayBufferReference<MeshVertex> m_vertices;
				Graphics::ArrayBufferReference<uint32_t> m_indices;
				std::atomic<bool> m_dirty;

				inline void OnMeshDirty(Graphics::GraphicsMesh*) { m_dirty = true; }

			public:
				inline void Update() {
					if (!m_dirty) return;
					m_graphicsMesh->GetBuffers(m_vertices, m_indices);
					m_dirty = false;
				}

				inline MeshBuffers(PipelineDescriptor* pipeline, const InstancedBatchDesc& desc)
					: m_graphicsMesh(desc.context->MeshCache()->GetMesh(desc.mesh, false)), m_dirty(true) {
					m_graphicsMesh->GetBuffers(m_vertices, m_indices);
					m_graphicsMesh->OnInvalidate() += Callback<Graphics::GraphicsMesh*>(&MeshBuffers::OnMeshDirty, this);
					Update();
				}

				inline virtual ~MeshBuffers() {
					m_graphicsMesh->OnInvalidate() -= Callback<Graphics::GraphicsMesh*>(&MeshBuffers::OnMeshDirty, this);
				}

				inline virtual size_t AttributeCount()const override { return 3; }

				inline virtual AttributeInfo Attribute(size_t index)const override {
					static const AttributeInfo INFOS[] = {
						{ AttributeInfo::Type::FLOAT3, 0, offsetof(MeshVertex, position) },
						{ AttributeInfo::Type::FLOAT3, 1, offsetof(MeshVertex, normal) },
						{ AttributeInfo::Type::FLOAT2, 2, offsetof(MeshVertex, uv) },
					};
					return INFOS[index];
				}

				inline virtual size_t BufferElemSize()const override { return sizeof(MeshVertex); }

				inline virtual Reference<Graphics::ArrayBuffer> Buffer() override { return m_vertices; }

				inline Graphics::ArrayBufferReference<uint32_t> IndexBuffer()const { return m_indices; }
			} m_meshBuffers;

			// Instancing data:
			class InstanceBuffer : public virtual Graphics::InstanceBuffer {
			private:
				Graphics::GraphicsDevice* const m_device;
				const bool m_isStatic;
				std::mutex m_transformLock;
				std::unordered_map<const Transform*, size_t> m_transformIndices;
				std::vector<Reference<const Transform>> m_transforms;
				std::vector<Matrix4> m_transformBufferData;
				Graphics::ArrayBufferReference<Matrix4> m_buffer;
				std::atomic<bool> m_dirty;
				std::atomic<size_t> m_instanceCount;


			public:
				inline void Update() {
					if ((!m_dirty) && m_isStatic) return;
					std::unique_lock<std::mutex> lock(m_transformLock);
					m_instanceCount = m_transforms.size();

					bool bufferDirty = (m_buffer == nullptr || m_buffer->ObjectCount() < m_instanceCount);
					size_t i = 0;
					if (bufferDirty) {
						size_t count = m_instanceCount;
						if (count <= 0) count = 1;
						m_buffer = m_device->CreateArrayBuffer<Matrix4>(count);
					}
					else while (i < m_instanceCount) {
						if (m_transforms[i]->WorldMatrix() != m_transformBufferData[i]) {
							bufferDirty = true;
							break;
						}
						else i++;
					}
					if (bufferDirty) {
						while (i < m_transforms.size()) {
							m_transformBufferData[i] = m_transforms[i]->WorldMatrix();
							i++;
						}
						memcpy(m_buffer.Map(), m_transformBufferData.data(), m_transforms.size() * sizeof(Matrix4));
						m_buffer->Unmap(true);
					}

					m_dirty = false;
				}

				inline InstanceBuffer(Graphics::GraphicsDevice* device, bool isStatic) : m_device(device), m_isStatic(isStatic), m_dirty(true), m_instanceCount(0) { Update(); }

				inline virtual size_t AttributeCount()const override { return 1; }

				inline virtual Graphics::InstanceBuffer::AttributeInfo Attribute(size_t)const {
					return { Graphics::InstanceBuffer::AttributeInfo::Type::MAT_4X4, 3, 0 };
				}

				inline virtual size_t BufferElemSize()const override { return sizeof(Matrix4); }

				inline virtual Reference<Graphics::ArrayBuffer> Buffer() override { return m_buffer; }

				inline size_t InstanceCount() { return m_instanceCount; }

				inline size_t AddTransform(const Transform* transform) {
					std::unique_lock<std::mutex> lock(m_transformLock);
					if (m_transformIndices.find(transform) != m_transformIndices.end()) return m_transforms.size();
					m_transformIndices[transform] = m_transforms.size();
					m_transforms.push_back(transform);
					while (m_transformBufferData.size() < m_transforms.size())
						m_transformBufferData.push_back(Matrix4(0.0f));
					m_dirty = true;
					return m_transforms.size();
				}

				inline size_t RemoveTransform(const Transform* transform) {
					std::unique_lock<std::mutex> lock(m_transformLock);
					std::unordered_map<const Transform*, size_t>::iterator it = m_transformIndices.find(transform);
					if (it == m_transformIndices.end()) return m_transforms.size();
					const size_t lastIndex = m_transforms.size() - 1;
					const size_t index = it->second;
					m_transformIndices.erase(it);
					if (index < lastIndex) {
						const Transform* last = m_transforms[lastIndex];
						m_transforms[index] = last;
						m_transformIndices[last] = index;
					}
					m_transforms.pop_back();
					m_dirty = true;
					return m_transforms.size();
				}
			} m_instanceBuffer;


		public:
			inline MeshRenderPipelineDescriptor(const InstancedBatchDesc& desc)
				: m_desc(desc), m_environmentBinding(desc.material->EnvironmentDescriptor())
				, m_capturedMaterial(desc.material)
				, m_meshBuffers(this, desc)
				, m_instanceBuffer(desc.context->Device(), desc.isStatic) {}

			/** PipelineDescriptor: */

			inline virtual size_t BindingSetCount()const override { return (m_environmentBinding == nullptr) ? 1 : 2; }

			inline virtual const Graphics::PipelineDescriptor::BindingSetDescriptor* BindingSet(size_t index)const override {
				return (index > 0) ? static_cast<const Graphics::PipelineDescriptor::BindingSetDescriptor*>(&m_capturedMaterial) : m_environmentBinding;
			}

			/** GraphicsPipeline::Descriptor: */

			inline virtual Reference<Graphics::Shader> VertexShader() override { return m_capturedMaterial.VertexShader(); }

			inline virtual Reference<Graphics::Shader> FragmentShader() override { return m_capturedMaterial.FragmentShader(); }

			inline virtual size_t VertexBufferCount() override { return 1; }

			inline virtual Reference<Graphics::VertexBuffer> VertexBuffer(size_t index) override { return &m_meshBuffers; }

			inline virtual size_t InstanceBufferCount() override { return 1; }

			inline virtual Reference<Graphics::InstanceBuffer> InstanceBuffer(size_t index) override { return &m_instanceBuffer; }

			inline virtual Graphics::ArrayBufferReference<uint32_t> IndexBuffer() override { return m_meshBuffers.IndexBuffer(); }

			inline virtual size_t IndexCount() override { return m_meshBuffers.IndexBuffer()->ObjectCount(); }

			inline virtual size_t InstanceCount() override { return m_instanceBuffer.InstanceCount(); }


			/** GraphicsContext::GraphicsObjectSynchronizer: */

			virtual inline void OnGraphicsSynch() override {
				WriteLock lock(this);
				m_capturedMaterial.Update();
				m_meshBuffers.Update();
				m_instanceBuffer.Update();
			}

			/** Writer */
			class Writer : public virtual Graphics::PipelineDescriptor::WriteLock {
			private:
				MeshRenderPipelineDescriptor* const m_desc;

			public:
				inline Writer(MeshRenderPipelineDescriptor* desc) : Graphics::PipelineDescriptor::WriteLock(desc), m_desc(desc) {}

				void AddTransform(const Transform* transform) {
					if (transform == nullptr) return;
					if (m_desc->m_instanceBuffer.AddTransform(transform) == 1)
						m_desc->m_desc.context->AddSceneObjectPipeline(m_desc);
				}

				void RemoveTransform(const Transform* transform) {
					if (transform == nullptr) return;
					if (m_desc->m_instanceBuffer.RemoveTransform(transform) <= 0)
						m_desc->m_desc.context->RemoveSceneObjectPipeline(m_desc);
				}
			};


			/** Instancer: */
			class Instancer : ObjectCache<InstancedBatchDesc> {
			private:
				inline static Instancer& Instance() {
					static Instancer instance;
					return instance;
				}

			public:
				inline static Reference<MeshRenderPipelineDescriptor> GetDescriptor(const InstancedBatchDesc& desc) {
					return Instance().GetCachedOrCreate(desc, false,
						[&]() -> Reference<MeshRenderPipelineDescriptor> { return Object::Instantiate<MeshRenderPipelineDescriptor>(desc); });
				}
			};
		};
#pragma warning(default: 4250)
	}

	MeshRenderer::MeshRenderer(Component* parent, const std::string& name, const TriMesh* mesh, const Jimara::Material* material, bool instanced, bool isStatic)
		: Component(parent, name), m_mesh(mesh), m_material(material), m_instanced(instanced), m_isStatic(isStatic), m_alive(true), m_descriptorTransform(nullptr) {
		RecreatePipelineDescriptor();
		OnParentChanged() += Callback(&MeshRenderer::RecreateOnParentChanged, this);
		OnDestroyed() += Callback(&MeshRenderer::RecreateWhenDestroyed, this);
	}

	MeshRenderer::~MeshRenderer() {
		OnParentChanged() -= Callback(&MeshRenderer::RecreateOnParentChanged, this);
		OnDestroyed() -= Callback(&MeshRenderer::RecreateWhenDestroyed, this);
		m_alive = false;
		RecreatePipelineDescriptor();
	}

	const TriMesh* MeshRenderer::Mesh()const { return m_mesh; }

	void MeshRenderer::SetMesh(const TriMesh* mesh) {
		if (mesh == m_mesh) return;
		m_mesh = mesh;
		RecreatePipelineDescriptor();
	}

	const Jimara::Material* MeshRenderer::Material()const { return m_material; }

	void MeshRenderer::SetMaterial(const Jimara::Material* material) {
		if (material == m_material) return;
		m_material = material;
		RecreatePipelineDescriptor();
	}

	bool MeshRenderer::IsInstanced()const { return m_instanced; }

	void MeshRenderer::RenderInstanced(bool instanced) {
		if (instanced == m_instanced) return;
		m_instanced = instanced;
		RecreatePipelineDescriptor();
	}

	bool MeshRenderer::IsStatic()const { return m_isStatic; }

	void MeshRenderer::MarkStatic(bool isStatic) {
		if (isStatic == m_isStatic) return;
		m_isStatic = isStatic;
		RecreatePipelineDescriptor();
	}


	void MeshRenderer::RecreatePipelineDescriptor() {
		if (m_pipelineDescriptor != nullptr) {
			MeshRenderPipelineDescriptor* descriptor = dynamic_cast<MeshRenderPipelineDescriptor*>(m_pipelineDescriptor.operator->());
			{
				MeshRenderPipelineDescriptor::Writer writer(descriptor);
				writer.RemoveTransform(m_descriptorTransform);
			}
			m_pipelineDescriptor = nullptr;
			m_descriptorTransform = nullptr;
		}
		if (m_alive && m_mesh != nullptr && m_material != nullptr) {
			m_descriptorTransform = GetTransfrom();
			if (m_descriptorTransform == nullptr) return;
			const InstancedBatchDesc desc(Context()->Graphics(), m_mesh, m_material, m_isStatic);
			Reference<MeshRenderPipelineDescriptor> descriptor;
			if (m_instanced) descriptor = MeshRenderPipelineDescriptor::Instancer::GetDescriptor(desc);
			else descriptor = Object::Instantiate<MeshRenderPipelineDescriptor>(desc);
			{
				MeshRenderPipelineDescriptor::Writer writer(descriptor);
				writer.AddTransform(m_descriptorTransform);
			}
			m_pipelineDescriptor = descriptor;
		}
	}

	void MeshRenderer::RecreateOnParentChanged(const Component*) {
		RecreatePipelineDescriptor();
	}

	void MeshRenderer::RecreateWhenDestroyed(Component*) {
		m_alive = false;
		RecreatePipelineDescriptor();
	}
}
