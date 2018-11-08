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

			public:

				enum { CHANNELS = 2, };
				const char *_channel_names[CHANNELS] = { "front left", "front right" };

				Genode::Constructible<Audio_out::Connection> _out[CHANNELS];
				Audio_out::Packet *_out_packet[CHANNELS] { };

				Audio(Genode::Env &env, Label &)
				{
					for (int i = 0; i < CHANNELS; i++) {
						try {
							_out[i].construct(env, _channel_names[i], false, false);
							_out_packet[i] = nullptr;
						} catch (...) {
							Genode::error("could not create Audio_out channel ", i);
							throw;
						}
					}
				}

				void pause()
				{
					for (int i = 0; i < CHANNELS; i++) {
						_out[i]->stop();
						_out_packet[i] = nullptr;
					}
				}

				bool write(char const *buf, file_size buf_size, file_size &out_size)
				{
					if (!_out_packet[0]) {
						for (int i = 0; i < CHANNELS; i++) { _out[i]->start(); }
						_out_packet[0] = _out[0]->stream()->next();
						_out_packet[0] = _out[0]->stream()->next(_out_packet[0]);
					}

					unsigned const pos = _out[0]->stream()->packet_position(_out_packet[0]);
					_out_packet[1] = _out[1]->stream()->get(pos);

					for (Genode::size_t sample = 0; sample < Audio_out::PERIOD; sample++) {
						for (int c = 0; c < CHANNELS; c++) {
							float *p = _out_packet[c]->content();
							p[sample] = (float)(((Genode::int16_t*)buf)[sample * CHANNELS + c]) / 32768;
						}
					}

					for (int c = 0; c < CHANNELS; c++) { _out[c]->submit(_out_packet[c]); }

					/* XXX */
					out_size = buf_size;
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
			                    Audio &audio)
			: Device_vfs_handle(ds, fs, alloc, 0), _audio(audio) { }

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

		using Config = Genode::String<256>;
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
			});
			return Config(Genode::Cstring(buf));
		}

		Readonly_value_file_system<unsigned> _channels_fs    { _env, "channels",    Audio::CHANNELS };
		Readonly_value_file_system<unsigned> _period_fs      { _env, "period",      Audio_out::PERIOD };
		Readonly_value_file_system<unsigned> _queue_size_fs  { _env, "queue_size",  Audio_out::QUEUE_SIZE };
		Readonly_value_file_system<unsigned> _queued_fs      { _env, "queued",      0 };
		Readonly_value_file_system<unsigned> _sample_rate_fs { _env, "sample_rate", Audio_out::SAMPLE_RATE };
		Readonly_value_file_system<unsigned> _sample_size_fs { _env, "sample_size", sizeof (Genode::int16_t) };

		Vfs::File_system *create(Vfs::Env &, Genode::Xml_node node) override
		{
			if (node.has_type(Readonly_value_file_system<unsigned>::type_name())) {
				return _channels_fs.matches(node)    ? &_channels_fs
				     : _period_fs.matches(node)      ? &_period_fs
				     : _queue_size_fs.matches(node)  ? &_queue_size_fs
				     : _queued_fs.matches(node)      ? &_queued_fs
				     : _sample_rate_fs.matches(node) ? &_sample_rate_fs
				     : _sample_size_fs.matches(node) ? &_sample_size_fs
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
			_audio(env.env(), _label)
		{
			Device_file_system::construct(env, Genode::Xml_node(_dir_config(config).string()), *this);
		}

		static const char *name()   { return "oss"; }
		char const *type() override { return "oss"; }

		/*********************************
		 ** Directory service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			try {
				if (_device_file(path)) {
					*out_handle = new (alloc)
						Registered_handle(_handle_registry, *this, *this, alloc, _audio);
					return OPEN_OK;
				} else

				if (_device_dir_file(path)) {
					return Device_file_system::open(path, 0, out_handle, alloc);
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
