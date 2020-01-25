/* This code is all generated from SG Edit. Any edits to it may be lost.*/

#include "SgCodeGen.h"

namespace sg {

const Scene* ToScene(void *data) {

#if SG_IMPL
	const uintptr_t data_start = uintptr_t(data);

	Scene *result = reinterpret_cast<Scene*>(data);
	result->components = reinterpret_cast<const void**>(data_start + reinterpret_cast<uintptr_t>(result->components));
	result->componentRanges = reinterpret_cast<const ComponentRange*>(data_start + reinterpret_cast<uintptr_t>(result->componentRanges));
	const ComponentRange* r = result->componentRanges;
	const ComponentRange* r_end = result->componentRanges + result->componentRangeCount;
	const void** c = result->components;

	if (r->typeId == TypeId::Transform) {
		c += r->count;
		if (++r == r_end)
			return result;
	}

	if (r->typeId == TypeId::Circle) {
		c += r->count;
		if (++r == r_end)
			return result;
	}

	if (r->typeId == TypeId::Rect) {
		c += r->count;
		if (++r == r_end)
			return result;
	}
	return result;
#endif
return nullptr;
}

#ifdef SG_BUILDER
struct SceneSizes {
	size_t scene_size;
	size_t component_array_size;
	size_t component_size;
	size_t text_size;
}
SceneSizes CalcByteSize(const Scene *scene) {

	SceneSizes result;
	result.scene_size = sizeof(Scene);
	result.component_array_size + sizeof(DeferredPtr) * scene->component_count;
	result.component_size = 0;
	result.text_size = 0;

	for (const DeferredPtr& p : *scene) {

		switch (p.typeId) {

			case TypeId::Transform {
				result.component_size += sizeof(Transform);
			} break;

			case TypeId::Circle {
				result.component_size += sizeof(Circle);
			} break;

			case TypeId::Rect {
				result.component_size += sizeof(Rect);
			} break;
		}
	}
	return result;
}

std::vector<uint8_t> ToBytes(const Scene *scene) {

	const SceneSizes sizes = CalcByteSize(scene);
	std::vector<uint8_t> result(sizes.scene_size + sizes.component_size + sizes.text_size);
	Scene* scene_start = reinterpret_cast<Scene*>(result.data());
	DeferredPtr *out_p = reinterpret_cast<DeferredPtr*>(result.data() + sizes.scene_size);
	uint8_t* component_start = result.data() + sizes.scene_size + sizes.component_array_size;
	uint8_t* text_start = result.data() + sizes.text_size;
	reinterpret_cast<uintptr_t&>(scene_start->components) = sizeof(Scene);
	scene_start->component_count = scene->component_count;
	for (const DeferredPtr& in_p : scene) {
		DeferredPtr& out_p = 	}
}
#endif
}
