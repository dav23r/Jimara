#include "GraphicsMesh.h"


namespace Jimara {
	namespace Graphics {
		GraphicsMesh::GraphicsMesh(GraphicsDevice* device, const TriMesh* mesh)
			: m_device(device), m_mesh(mesh), m_revision(0) {
			MeshChanged(mesh);
			m_mesh->OnDirty() += Callback<Mesh<MeshVertex, TriangleFace>*>(&GraphicsMesh::OnMeshChanged, this);
		}

		GraphicsMesh::~GraphicsMesh() {
			m_mesh->OnDirty() -= Callback<Mesh<MeshVertex, TriangleFace>*>(&GraphicsMesh::OnMeshChanged, this);
		}

		void GraphicsMesh::GetBuffers(ArrayBufferReference<MeshVertex>& vertexBuffer, ArrayBufferReference<uint32_t>& indexBuffer) {
			{
				uint64_t startRevision = m_revision;
				vertexBuffer = m_vertexBuffer;
				indexBuffer = m_indexBuffer;
				uint64_t endRevision = m_revision;
				if (startRevision == endRevision && vertexBuffer != nullptr && indexBuffer != nullptr) return;
			}
			std::unique_lock<std::recursive_mutex> lock(m_bufferLock);
			TriMesh::Reader reader(m_mesh);
			if (m_vertexBuffer == nullptr) {
				m_vertexBuffer = m_device->CreateArrayBuffer<MeshVertex>(reader.VertCount());
				MeshVertex* verts = m_vertexBuffer.Map();
				for (uint32_t i = 0; i < reader.VertCount(); i++)
					verts[i] = reader.Vert(i);
				m_vertexBuffer->Unmap(true);
			}
			if (m_indexBuffer == nullptr) {
				m_indexBuffer = m_device->CreateArrayBuffer<uint32_t>(reader.FaceCount() * 3);
				uint32_t* indices = m_indexBuffer.Map();
				for (uint32_t i = 0; i < reader.FaceCount(); i++) {
					TriangleFace face = reader.Face(i);
					uint32_t index = 3u * i;
					indices[index] = face.a;
					indices[index + 1] = face.b;
					indices[index + 2] = face.c;
				}
				m_indexBuffer->Unmap(true);
			}
			vertexBuffer = m_vertexBuffer;
			indexBuffer = m_indexBuffer;
		}

		Event<GraphicsMesh*>& GraphicsMesh::OnInvalidate() { return m_onInvalidate; }

		void GraphicsMesh::MeshChanged(const Mesh<MeshVertex, TriangleFace>* mesh) {
			std::unique_lock<std::recursive_mutex> lock(m_bufferLock);
			m_vertexBuffer = nullptr;
			m_indexBuffer = nullptr;
			m_revision++;
			m_onInvalidate(this);
		}


		GraphicsMeshCache::GraphicsMeshCache(GraphicsDevice* device)
			: m_device(device) { }

		Reference<GraphicsMesh> GraphicsMeshCache::GetMesh(const TriMesh* mesh, bool storePermanently) {
			if (mesh == nullptr) return nullptr;
			return GetCachedOrCreate(mesh, storePermanently,
				[&]()->Reference<GraphicsMesh> { return Object::Instantiate<GraphicsMesh>(m_device, mesh); });
		}
	}
}
