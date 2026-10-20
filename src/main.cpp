#include "LumenPCH.h"
#include "Framework/Window.h"
#include "RayTracer/RayTracer.h"

void window_size_callback(GLFWwindow* window, int width, int height) {}

int main(int argc, char* argv[]) {
#ifdef _DEBUG
	bool enable_debug = true;
#else
	bool enable_debug = false;
#endif
	bool fullscreen = false;
	int width = getenv("LUMEN_WIDTH") ? atoi(getenv("LUMEN_WIDTH")) : 1920;
	int height = getenv("LUMEN_HEIGHT") ? atoi(getenv("LUMEN_HEIGHT")) : 1080;
	bool headless = false;
	int frames = 500;
	std::string output = "output/headless.exr";
	bool save_output = true;
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "--headless") headless = true;
		else if (arg == "--frames" && i + 1 < argc) frames = std::atoi(argv[++i]);
		else if (arg == "--output" && i + 1 < argc) output = argv[++i];
		else if (arg == "--no-output") save_output = false;
	}
	Logger::init();
	lumen::ThreadPool::init();
	Window::init(width, height, fullscreen, headless);
	{
		RayTracer app(enable_debug, argc, argv);
		app.init();
		if (headless) {
			for (int i = 0; i < frames; ++i) {
				if (save_output && i == frames - 1) app.capture_next_output();
				app.update();
			}
			if (save_output && frames > 0) app.save_output(output);
		} else {
			while (!Window::should_close()) {
				Window::poll();
				app.update();
			}
		}
		app.cleanup();
	}
	Window::destroy();
	lumen::ThreadPool::destroy();
	return 0;
}
