#pragma once
#include "Component.h"
#include "../Math/Math.h"


namespace Jimara {
	/// <summary>
	/// Transform Component
	/// </summary>
	class Transform : public virtual Component {
	public:
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="parent"> Parent component </param>
		/// <param name="name"> Transform component name </param>
		/// <param name="localPosition"> Local position </param>
		/// <param name="localEulerAngles"> Local auler angles </param>
		/// <param name="localScale"> Local scale </param>
		Transform(Component* parent, const std::string& name
			, const Vector3& localPosition = Vector3(0.0f, 0.0f, 0.0f)
			, const Vector3& localEulerAngles = Vector3(0.0f, 0.0f, 0.0f)
			, const Vector3& localScale = Vector3(1.0f, 1.0f, 1.0f));


		/// <summary> Position in "relative to parent transform" coordinate system </summary>
		Vector3 LocalPosition()const;

		/// <summary>
		/// Sets local position
		/// </summary>
		/// <param name="value"> New position to set </param>
		void SetLocalPosition(const Vector3& value);

		/// <summary> World space position </summary>
		Vector3 WorldPosition()const;

		/// <summary>
		/// Sets world-space position
		/// </summary>
		/// <param name="value"> New position to set </param>
		void SetWorldPosition(const Vector3& value);


		/// <summary> Euler angles in "relative to parent transform" coordinate system </summary>
		Vector3 LocalEulerAngles()const;

		/// <summary>
		/// Sets local euler angles
		/// </summary>
		/// <param name="value"> Euler angles to set </param>
		void SetLocalEulerAngles(const Vector3& value);

		/// <summary> World space euler angles </summary>
		Vector3 WorldEulerAngles()const;

		/// <summary>
		/// Sets world-space euler angles
		/// </summary>
		/// <param name="value"> Euler angles to set </param>
		void SetWorldEulerAngles(const Vector3& value);


		/// <summary> Scale in "relative to parent transform" coordinate system </summary>
		Vector3 LocalScale()const;
		
		/// <summary>
		/// Sets local scale
		/// </summary>
		/// <param name="value"> Scale to set </param>
		void SetLocalScale(const Vector3& value);


		/// <summary> Transformation matrix in "relative to parent transform" coordinate system </summary>
		const Matrix4& LocalMatrix()const;

		/// <summary> Rotation matrix in "relative to parent transform" coordinate system </summary>
		const Matrix4& LocalRotationMatrix()const;

		/// <summary> Transformation matrix in world coordinate system </summary>
		Matrix4 WorldMatrix()const;

		/// <summary> Rotation matrix in world coordinate system </summary>
		Matrix4 WorldRotationMatrix()const;


		/// <summary>
		/// Translates direction from local space to "relative to parent transform" coordinate system
		/// </summary>
		/// <param name="localDirection"> Transform space direction </param>
		/// <returns> "Relative to parent transform" direction </returns>
		Vector3 LocalToParentSpaceDirection(const Vector3& localDirection)const;

		/// <summary> Forward direction in "relative to parent transform" coordinate system (ei local Vector3(0.0f, 0.0f, 1.0f)) </summary>
		Vector3 LocalForward()const;

		/// <summary> Right direction in "relative to parent transform" coordinate system (ei local Vector3(1.0f, 0.0f, 0.0f)) </summary>
		Vector3 LocalRight()const;

		/// <summary> Up direction in "relative to parent transform" coordinate system (ei local Vector3(0.0f, 1.0f, 0.0f)) </summary>
		Vector3 LocalUp()const;

		/// <summary>
		/// Translates direction from local to world space
		/// </summary>
		/// <param name="localDirection"> Transform space direction </param>
		/// <returns> World space direction </returns>
		Vector3 LocalToWorldDirection(const Vector3& localDirection)const;

		/// <summary> Forward direction in world space (ei local Vector3(0.0f, 0.0f, 1.0f)) </summary>
		Vector3 Forward()const;

		/// <summary> Right direction in world space (ei local Vector3(1.0f, 0.0f, 0.0f)) </summary>
		Vector3 Right()const;

		/// <summary> Up direction in world space (ei local Vector3(0.0f, 1.0f, 0.0f)) </summary>
		Vector3 Up()const;


		/// <summary>
		/// Translates position from local space to "relative to parent transform" coordinate system
		/// </summary>
		/// <param name="localPosition"> Local position </param>
		/// <returns> "Relative to parent transform" position </returns>
		Vector3 LocalToParentSpacePosition(const Vector3& localPosition)const;

		/// <summary>
		/// Translates position from local to world space
		/// </summary>
		/// <param name="localPosition"> Local position </param>
		/// <returns> World space position </returns>
		Vector3 LocalToWorldPosition(const Vector3& localPosition)const;


	private:
		// Local position
		Vector3 m_localPosition;

		// Local euler angles
		Vector3 m_localEulerAngles;

		// Local scale
		Vector3 m_localScale;
		
		// True, when matrices are invalidated
		mutable std::atomic<bool> m_matrixDirty;

		// We'll use a simplistic spinlock to protect matrix initialisation
		mutable std::atomic<uint32_t> m_matrixLock;

		// Local rotation matrix
		mutable Matrix4 m_rotationMatrix;
		
		// Local transform matrix
		mutable Matrix4 m_transformationMatrix;

		// Updates matrices if m_matrixDirty flag is set
		void UpdateMatrices()const;
	};
}
