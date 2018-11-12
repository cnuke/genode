/*
 * \brief  OSS emulation to Audio_out file system
 * \author Josef Soentgen
 * \date   2018-10-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <audio_out_session/connection.h>
#include <base/registry.h>
#include <base/signal.h>
#include <gems/magic_ring_buffer.h>
#include <util/xml_generator.h>
#include <vfs/device_file_system.h>
#include <vfs/readonly_value_file_system.h>


namespace Vfs_oss {
	using namespace Vfs;

	class File_system;
} /* namespace Vfs_oss */


class Vfs_oss::File_system : public Device_file_system
{
	private:

		File_system(File_system const &);
		File_system &operator = (File_system const &);

		typedef Genode::String<64> Label;
		Label _label;
		typedef Genode::String<64> Name;
		Label _name;

		Vfs::Env               &_env;
		Io_response_handler    &_io_handler;
		Watch_response_handler &_watch_handler;

		struct Audio
		{
			private:

				Audio(Audio const &);
				Audio &operator = (Audio const &);

				Genode::Magic_ring_buffer<float> _left_buffer;
				Genode::Magic_ring_buffer<float> _right_buffer;

				bool _started { false };

			public:

				enum { CHANNELS = 2, };
				const char *_channel_names[CHANNELS] = { "front left", "front right" };

				Genode::Constructible<Audio_out::Connection> _out[CHANNELS];

				Audio(Genode::Env &env, Label &)
				: _left_buffer(env, 1u << 20), _right_buffer(env, 1u << 20)
				{
					for (int i = 0; i < CHANNELS; i++) {
						try {
							_out[i].construct(env, _channel_names[i], false, false);
						} catch (...) {
							Genode::error("could not create Audio_out channel ", i);
							throw;
						}
					}
				}

				void pause()
				{
					for (int i = 0; i < CHANNELS; i++) { _out[i]->stop(); }
					_started = false;
				}

				bool write(char const *buf, file_size buf_size, file_size &out_size)
				{
					Genode::size_t const samples = Genode::min(_left_buffer.write_avail(), buf_size/2);

					float *dest[2] = { _left_buffer.write_addr(), _right_buffer.write_addr() };

					for (Genode::size_t i = 0; i < samples/2; i++) {
						for (int c = 0; c < CHANNELS; c++) {
							float *p = dest[c];
							Genode::int16_t const v = ((Genode::int16_t const*)buf)[i * CHANNELS + c];
							p[i] = ((float)v) / 32768.0f;
						}
					}

					_left_buffer.fill(samples/2);

					while (_left_buffer.read_avail() > Audio_out::PERIOD) {

						if (!_started) {
							_started = true;

							_out[0]->start();
							_out[1]->start();
						}

						Audio_out::Packet *lp = _out[0]->stream()->next();
						unsigned const pos = _out[0]->stream()->packet_position(lp);
						Audio_out::Packet *rp = _out[1]->stream()->get(pos);

						float const *src[CHANNELS] = { _left_buffer.read_addr(),
						                               _right_buffer.read_addr() };

						Genode::memcpy(lp->content(), src[0], Audio_out::PERIOD * 4);
						_left_buffer.drain(Audio_out::PERIOD);
						Genode::memcpy(rp->content(), src[1], Audio_out::PERIOD * 4);
						_right_buffer.drain(Audio_out::PERIOD);

						_out[0]->submit(lp);
						_out[1]->submit(rp);
					}

					/* XXX */
					out_size = samples * 2;
					return true;
				}
		};
		Audio _audio;

		struct Oss_vfs_file_handle : public Device_vfs_handle
		{
			Audio &_audio;

			Oss_vfs_file_handle(Directory_service    &ds,
			                    File_io_service      &fs,
			                    Genode::Allocator    &alloc,
			                    Audio &audio, int flags)
			: Device_vfs_handle(ds, fs, alloc, flags), _audio(audio) { }

			/* not supported */
			Read_result read(char *, file_size, file_size &) {
				return READ_ERR_INVALID; }

			Write_result write(char const *buf, file_size buf_size, file_size &out_count)
			{
				return _audio.write(buf, buf_size, out_count) ? WRITE_OK : WRITE_ERR_INVALID;
			}

			bool read_ready() { return true; }
		};

		typedef Genode::Registered<Oss_vfs_file_handle> Registered_handle;
		typedef Genode::Registry<Registered_handle>     Handle_registry;
		Handle_registry _handle_registry { };

		using Config = Genode::String<4096>;
		static Config _dir_config(Genode::Xml_node node)
		{
			char buf[Config::capacity()] { };

			Genode::Xml_generator xml(buf, sizeof(buf), "dir", [&] {

				using Name = Genode::String<64>;
				Name dir_name = node.attribute_value("name", Name(name()));

				xml.attribute("name", Name(".", dir_name));

				xml.node("readonly_value", [&] { xml.attribute("name", "channels"); });
				xml.node("readonly_value", [&] { xml.attribute("name", "period"); });
				xml.node("readonly_value", [&] { xml.attribute("name", "queue_size"); });
				xml.node("readonly_value", [&] { xml.attribute("name", "queued"); });
				xml.node("readonly_value", [&] { xml.attribute("name", "sample_rate"); });
				xml.node("readonly_value", [&] { xml.attribute("name", "sample_size"); });
				xml.node("readonly_value", [&] { xml.attribute("name", "frag_size"); });
				xml.node("readonly_value", [&] { xml.attribute("name", "frag_avail"); });
				xml.node("readonly_value", [&] { xml.attribute("name", "frag_used"); });
			});
			return Config(Genode::Cstring(buf));
		}

		Readonly_value_file_system<unsigned> _channels_fs    { _env, "channels",    Audio::CHANNELS };
		Readonly_value_file_system<unsigned> _period_fs      { _env, "period",      Audio_out::PERIOD };
		Readonly_value_file_system<unsigned> _queue_size_fs  { _env, "queue_size",  Audio_out::QUEUE_SIZE };
		Readonly_value_file_system<unsigned> _queued_fs      { _env, "queued",      0 };
		Readonly_value_file_system<unsigned> _sample_rate_fs { _env, "sample_rate", Audio_out::SAMPLE_RATE };
		Readonly_value_file_system<unsigned> _sample_size_fs { _env, "sample_size", sizeof (Genode::int16_t) };

		Readonly_value_file_system<unsigned> _ofrag_size_fs  { _env, "frag_size", 8192 }; // XXX calculate
		Readonly_value_file_system<unsigned> _ofrag_avail_fs { _env, "frag_avail",  32 };
		Readonly_value_file_system<unsigned> _ofrag_used_fs  { _env, "frag_used",    0 };

		Vfs::File_system *create(Vfs::Env &, Genode::Xml_node node) override
		{
			if (node.has_type(Readonly_value_file_system<unsigned>::type_name())) {
				return _channels_fs.matches(node)    ? &_channels_fs
				     : _period_fs.matches(node)      ? &_period_fs
				     : _queue_size_fs.matches(node)  ? &_queue_size_fs
				     : _queued_fs.matches(node)      ? &_queued_fs
				     : _sample_rate_fs.matches(node) ? &_sample_rate_fs
				     : _sample_size_fs.matches(node) ? &_sample_size_fs
				     : _ofrag_size_fs.matches(node)  ? &_ofrag_size_fs
				     : _ofrag_avail_fs.matches(node) ? &_ofrag_avail_fs
				     : _ofrag_used_fs.matches(node)  ? &_ofrag_used_fs
				     : nullptr;
			}
			return nullptr;
		}

	public:

		File_system(Vfs::Env &env, Genode::Xml_node config)
		:
			Device_file_system(NODE_TYPE_CHAR_DEVICE, name(), config),
			_label(config.attribute_value("label", Label())),
			_name(config.attribute_value("name", Name(name()))),
			_env(env), _io_handler(env.io_handler()),
			_watch_handler(env.watch_handler()),
			_audio(_env.env(), _label)
		{
			Device_file_system::construct(env, Genode::Xml_node(_dir_config(config).string()), *this);
		}

		static const char *name()   { return "oss"; }
		char const *type() override { return "oss"; }

		/*********************************
		 ** Directory service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned flags,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			try {
				if (_device_file(path)) {
					*out_handle = new (alloc)
						Registered_handle(_handle_registry, *this, *this, alloc, _audio, flags);
					return OPEN_OK;
				} else

				if (_device_dir_file(path)) {
					return Device_file_system::open(path, flags, out_handle, alloc);
				}
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }

			return OPEN_ERR_UNACCESSIBLE;
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs_handle *, file_size) override
		{
			return FTRUNCATE_OK;
		}

		bool check_unblock(Vfs_handle *, bool, bool, bool) override
		{
			/* XXX check if is enough space left in Audio packet stream */
			return true;
		}
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node config) override
		{
			return new (env.alloc()) Vfs_oss::File_system(env, config);
		}
	};

	static Factory f;
	return &f;
}
