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
	// Small resolution so a full frame completes under GPGPU-Sim's cycle-accurate simulation
	// (the 1850x1016 default is ~1.88M pixels, far too many to simulate). Overridable via env
	// vars for native-GPU runs that don't have this constraint (e.g. LUMEN_WIDTH=1850
	// LUMEN_HEIGHT=1016 ./Lumen ...).
	int width = getenv("LUMEN_WIDTH") ? atoi(getenv("LUMEN_WIDTH")) : 128;
	int height = getenv("LUMEN_HEIGHT") ? atoi(getenv("LUMEN_HEIGHT")) : 128;
	Logger::init();
	lumen::ThreadPool::init();
	Window::init(width, height, fullscreen);
	{
		RayTracer app(enable_debug, argc, argv);
		app.init();
		while (!Window::should_close()) {
			Window::poll();
			app.update();
		}
		app.cleanup();
	}
	Window::destroy();
	lumen::ThreadPool::destroy();
	return 0;
}