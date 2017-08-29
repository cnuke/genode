#include <libc/component.h>


Genode::Env *genode_env;
static Genode::Constructible<Genode::Entrypoint>  signal_ep;


Genode::Entrypoint &genode_entrypoint()
{
	return *signal_ep;
}


extern "C" int ioq3_main(int argc, char *argv[]);

Genode::size_t Libc::Component::stack_size() { return 768u * 1024; }
void Libc::Component::construct(Libc::Env &env)
{
	genode_env = &env;
	char name[] = { 'i', 'o', 'q', '3', 0 };
	char *argv[] = { name };

	signal_ep.construct(env, 1024*sizeof(long), "sdl_signal_ep");
	Libc::with_libc([&] () { ioq3_main(1, argv); });
}
