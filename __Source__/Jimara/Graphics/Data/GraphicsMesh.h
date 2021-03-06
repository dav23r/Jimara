#pragma once
#include "../../Data/Mesh.h"
#include "../GraphicsDevice.h"
#include <mutex>


namespace Jimara {
	namespace Graphics {
		/// <summary>
		/// TriMesh representation as graphics buffers
		/// </summary>
		class GraphicsMesh : public virtual ObjectCache<const TriMesh*>::StoredObject {
		public:
			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="device"> Graphics device </param>
			/// <param name="mesh"> Triangulated mesh </param>
			GraphicsMesh(GraphicsDevice* device, const TriMesh* mesh);

			/// <summary> Virtual destructor </summary>
			virtual ~GraphicsMesh();

			/// <summary>
			/// Atomically fetches buffers
			/// </summary>
			/// <param name="vertexBuffer"> Vertex buffer reference's reference </param>
			/// <param name="indexBuffer"> Index buffer reference's reference </param>
			void GetBuffers(ArrayBufferReference<MeshVertex>& vertexBuffer, ArrayBufferReference<uint32_t>& indexBuffer);

			/// <summary> Invoked, whenever the underlying mesh gets altered and buffers are no longer up to date </summary>
			Event<GraphicsMesh*>& OnInvalidate();



		private:
			// Graphics device
			const Reference<GraphicsDevice> m_device;
			
			// Mesh
			const Reference<const TriMesh> m_mesh;

			// Current vertex buffer
			ArrayBufferReference<MeshVertex> m_vertexBuffer;

			// Current index buffer
			ArrayBufferReference<uint32_t> m_indexBuffer;

			// Revision (to control data refreshes)
			std::atomic<uint64_t> m_revision;

			// Lock for updating buffers
			std::recursive_mutex m_bufferLock;

			// Invoked, whenever the underlying mesh gets altered and buffers are no longer up to date
			EventInstance<GraphicsMesh*> m_onInvalidate;

			// Mesh change callback
			void MeshChanged(const Mesh<MeshVertex, TriangleFace>* mesh);
			inline void OnMeshChanged(Mesh<MeshVertex, TriangleFace>* mesh) { MeshChanged(mesh); }
		};



		/// <summary>
		/// Graphics mesh cache for instance reuse
		/// </summary>
		class GraphicsMeshCache : public virtual ObjectCache<const TriMesh*> {
		public:
			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="device"> Graphics device </param>
			GraphicsMeshCache(GraphicsDevice* device);

			/// <summary>
			/// Fetches stored mesh or creates a new one
			/// </summary>
			/// <param name="mesh"> Source mesh </param>
			/// <param name="storePermanently"> If true, the GraphicsMesh will stay in the cache till the cache goes out of scope </param>
			/// <returns> Graphics mesh reference </returns>
			Reference<GraphicsMesh> GetMesh(const TriMesh* mesh, bool storePermanently);

		private:
			// "Owner" device
			const Reference<GraphicsDevice> m_device;
		};
	}
}
