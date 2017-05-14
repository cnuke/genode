#include <base/component.h>
#include <libc/component.h>
#include <window.h>
#include <os/backtrace.h>

static Genode::Env *_env;

void Window::sync_handler() { }
void Window::mode_handler() { }


Genode::Env &genode_env()
{
	return *_env;
}

extern "C" int ioq3_main(int argc, char *argv[]);
extern void drm_init(Genode::Env &env);

Genode::size_t Component::stack_size() { return 768u<<10; }

void Libc::Component::construct(Libc::Env &env)
{
	_env = &env;

	//XXX: does not work for swrast
	try {
		drm_init(env);
	} catch (...) {
		Genode::error("could not use Drm session, falling back to swrast");
	}

	static char *argv[] = { "ioq3" };
	Libc::with_libc([] () { ioq3_main(1, argv); });
}
