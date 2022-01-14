#include <platform_session/device.h>


Platform::Connection::Connection(Genode::Env &env)
:
	_env { env }
{
	try {
		_legacy_platform.construct(env);
	} catch (...) {
		Genode::error("could not construct legacy Platform connection");
		throw;
	}
}


void Platform::Connection::update()
{
	Genode::error(__func__, ": not implemented");
}


Genode::Ram_dataspace_capability
Platform::Connection::alloc_dma_buffer(size_t size, Cache cache)
{
	return _legacy_platform->with_upgrade([&] () {
		return _legacy_platform->alloc_dma_buffer(size, cache);
	});
}


void Platform::Connection::free_dma_buffer(Ram_dataspace_capability)
{
	Genode::error(__func__, ": not implemented");
}


Genode::addr_t Platform::Connection::dma_addr(Ram_dataspace_capability ds_cap)
{
	return _legacy_platform->dma_addr(ds_cap);
}
