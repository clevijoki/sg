/* This code is all generated from SG Edit. Any edits to it may be lost.*/

#pragma once

#include <cstdint>

#ifdef SG_BUILDER
#include <vector>
#endif
namespace sg {

	enum class TypeId {
		Transform,
		Circle,
		Rect,
	};

	#define SG_COMPONENTS\
		X(Transform)\
		X(Circle)\
		X(Rect)


	struct alignas(16) Transform {
		static const TypeId StaticTypeId = TypeId::Transform;

		float x;
		float y;
	};

	struct alignas(16) Circle {
		static const TypeId StaticTypeId = TypeId::Circle;

		float radius;
		Transform transform;
	};

	struct alignas(16) Rect {
		static const TypeId StaticTypeId = TypeId::Rect;

		float width;
		float height;
		Transform transform;
	};

	struct ComponentRange {
		TypeId typeId;
		uint32_t count;
	};

	struct Scene {
		const void **components;
		uintptr_t componentCount;

		const ComponentRange *componentRanges;
		uintptr_t componentRangeCount;
	};

	const Scene* ToScene(void *data);

#ifdef SG_BUILDER
	const std::vector<uint8_t> ToBytes(const Scene* scene);
#endif
}
