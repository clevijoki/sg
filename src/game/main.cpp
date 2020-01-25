#include <cstdio>
#include <algorithm>

#include <SDL.h>
#include <SDL_main.h>
#include <SDL_assert.h>
#include <SDL_render.h>

#include "SgCodeGen.h"

void ShowError(const char* file, int line_num, const char* msg) {

	char out_msg[2048];
	const size_t written = std::snprintf(out_msg, sizeof(out_msg), "%s(%d): '%s' failed: %s", file, line_num, msg, SDL_GetError());
	out_msg[std::min(sizeof(out_msg)-1, written)] = 0;

	SDL_MessageBoxData mbd{};
	mbd.flags = SDL_MESSAGEBOX_ERROR;
	mbd.window = nullptr;
	mbd.title = "SDL API Error";
	mbd.message = out_msg;
	SDL_MessageBoxButtonData buttons[] = {
		{0, 0, "OK"}	
	};
	mbd.numbuttons = 1;
	mbd.colorScheme = nullptr;

	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", out_msg);

	int pressed;
	SDL_ShowMessageBox(&mbd, &pressed);

	std::exit(-1);
}

#define VERIFY(line) if ((line) != 0) ShowError(__FILE__, __LINE__, #line )

template<typename T>
T* VerifyNotNulll(T* value, const char* file, int line, const char* msg) {
	if (!value)
		ShowError(file, line, msg);

	return value;
}

#define VERIFY_NOT_NULL(line) VerifyNotNulll((line), __FILE__, __LINE__, #line)

std::vector<uint8_t> load_file(const char* path) {
	FILE *fp = fopen(path, "rb");
	if (!fp) {

		printf("Error loading '%s'\n", path);
		exit(-1);
		return {};
	}

	fseek(fp, SEEK_END, 0);
	std::vector<uint8_t> result(ftell(fp));
	fseek(fp, SEEK_SET, 0);

	fread(result.data(), 1, result.size(), fp);

	return result;
}

void RenderTransform(SDL_Renderer*, const sg::Transform*) {}

void RenderCircle(SDL_Renderer* renderer, const sg::Circle *c) {
	SDL_RenderDrawLine(renderer, c->transform.x, c->transform.y, c->transform.x + c->radius, c->transform.y + c->radius);						
}

void RenderRect(SDL_Renderer* renderer, const sg::Rect *r) {
	SDL_Rect r2;
	r2.x = r->transform.x;
	r2.y = r->transform.y;
	r2.w = r->width;
	r2.h = r->height;
	SDL_RenderFillRect(renderer, &r2);
}

int main_loop(SDL_Window *window, SDL_Renderer* renderer) {

	SDL_Event e;

	std::vector<uint8_t> scene_bytes = load_file("scene.bin");

	const sg::Scene *scene = sg::ToScene(scene_bytes.data());

	while (true) {
		while (SDL_PollEvent(&e) != 0) {
			if (e.type == SDL_QUIT)
				return 0;
		}

		uint32_t idx = 0;

		for (uint32_t range_idx = 0; range_idx < scene->componentRangeCount; ++range_idx) {
			const sg::ComponentRange& cr = scene->componentRanges[range_idx];

			switch (cr.typeId) {
#define X(component_id) case sg::TypeId::component_id:\
				for (const uint32_t end = idx + cr.count; idx < end; ++idx)\
					Render##component_id(renderer, reinterpret_cast<const sg::component_id*>(scene->components[idx]));

				SG_COMPONENTS
#undef X
			}
		}
	}
}

int main(int argc, char* argv[]) {

	VERIFY(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS));

	SDL_Window *window = VERIFY_NOT_NULL(SDL_CreateWindow(
		"SG Game",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		640,
		480,
		0
	));

	SDL_Renderer* renderer = VERIFY_NOT_NULL(SDL_CreateRenderer(
		window,
		-1,
		0
	));

	const int res = main_loop(window, renderer);

	SDL_DestroyWindow(window);
	SDL_DestroyRenderer(renderer);

	SDL_Quit();
	return res;
}
