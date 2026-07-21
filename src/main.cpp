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
	int width = 1850;
	int height = 1016;

	// Headless (offscreen) rendering options. The scene file argument is parsed
	// separately by RayTracer::parse_args and left untouched here.
	bool headless = false;
	int headless_frames = 500;
	std::string headless_output = "output/headless.exr";
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg == "--headless") {
			headless = true;
		} else if (arg == "--frames" && i + 1 < argc) {
			headless_frames = std::atoi(argv[++i]);
		} else if (arg == "--output" && i + 1 < argc) {
			headless_output = argv[++i];
		}
	}

	Logger::init();
	lumen::ThreadPool::init();
	Window::init(width, height, fullscreen, headless);
	{
		RayTracer app(enable_debug, argc, argv);
		app.init();
		if (headless) {
			for (int i = 0; i < headless_frames; i++) {
				app.update();
			}
			app.save_output(headless_output);
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