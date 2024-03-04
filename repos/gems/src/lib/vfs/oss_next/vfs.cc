/*
 * \brief  OSS to Record and Play session translator plugin
 * \author Josef Soentgen
 * \date   2024-02-20
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/registry.h>
#include <base/signal.h>
#include <os/vfs.h>
#include <play_session/connection.h>
#include <record_session/connection.h>
#include <timer_session/connection.h>
#include <util/xml_generator.h>
#include <vfs/dir_file_system.h>
#include <vfs/readonly_value_file_system.h>
#include <vfs/single_file_system.h>
#include <vfs/value_file_system.h>

#include "ring_buffer.h"

namespace Vfs { struct Oss_file_system; }


struct Vfs::Oss_file_system
{
	using Name = String<32>;

	struct Audio;

	struct Data_file_system;
	struct Local_factory;
	struct Compound_file_system;
};


struct Vfs::Oss_file_system::Audio
{
	public:

		struct Info
		{
			unsigned  channels;
			unsigned  format;
			unsigned  sample_rate;
			unsigned  ifrag_total;
			unsigned  ifrag_size;
			unsigned  ifrag_avail;
			unsigned  ifrag_bytes;
			unsigned  ofrag_total;
			unsigned  ofrag_size;
			unsigned  ofrag_avail;
			unsigned  ofrag_bytes;
			long long optr_samples;
			unsigned  optr_fifo_samples;
			unsigned  play_underruns;

			Readonly_value_file_system<unsigned>  &_channels_fs;
			Readonly_value_file_system<unsigned>  &_format_fs;
			Readonly_value_file_system<unsigned>  &_sample_rate_fs;
			Value_file_system<unsigned>           &_ifrag_total_fs;
			Value_file_system<unsigned>           &_ifrag_size_fs;
			Readonly_value_file_system<unsigned>  &_ifrag_avail_fs;
			Readonly_value_file_system<unsigned>  &_ifrag_bytes_fs;
			Value_file_system<unsigned>           &_ofrag_total_fs;
			Value_file_system<unsigned>           &_ofrag_size_fs;
			Readonly_value_file_system<unsigned>  &_ofrag_avail_fs;
			Readonly_value_file_system<unsigned>  &_ofrag_bytes_fs;
			Readonly_value_file_system<long long> &_optr_samples_fs;
			Readonly_value_file_system<unsigned>  &_optr_fifo_samples_fs;
			Value_file_system<unsigned>           &_play_underruns_fs;

			Info(Readonly_value_file_system<unsigned>  &channels_fs,
			     Readonly_value_file_system<unsigned>  &format_fs,
			     Readonly_value_file_system<unsigned>  &sample_rate_fs,
			     Value_file_system<unsigned>           &ifrag_total_fs,
			     Value_file_system<unsigned>           &ifrag_size_fs,
			     Readonly_value_file_system<unsigned>  &ifrag_avail_fs,
			     Readonly_value_file_system<unsigned>  &ifrag_bytes_fs,
			     Value_file_system<unsigned>           &ofrag_total_fs,
			     Value_file_system<unsigned>           &ofrag_size_fs,
			     Readonly_value_file_system<unsigned>  &ofrag_avail_fs,
			     Readonly_value_file_system<unsigned>  &ofrag_bytes_fs,
			     Readonly_value_file_system<long long> &optr_samples_fs,
			     Readonly_value_file_system<unsigned>  &optr_fifo_samples_fs,
			     Value_file_system<unsigned>           &play_underruns_fs)
			:
				channels              { 0 },
				format                { 0 },
				sample_rate           { 0 },
				ifrag_total           { 0 },
				ifrag_size            { 0 },
				ifrag_avail           { 0 },
				ifrag_bytes           { 0 },
				ofrag_total           { 0 },
				ofrag_size            { 0 },
				ofrag_avail           { 0 },
				ofrag_bytes           { 0 },
				optr_samples          { 0 },
				optr_fifo_samples     { 0 },
				play_underruns        { 0 },
				_channels_fs          { channels_fs },
				_format_fs            { format_fs },
				_sample_rate_fs       { sample_rate_fs },
				_ifrag_total_fs       { ifrag_total_fs },
				_ifrag_size_fs        { ifrag_size_fs },
				_ifrag_avail_fs       { ifrag_avail_fs },
				_ifrag_bytes_fs       { ifrag_bytes_fs },
				_ofrag_total_fs       { ofrag_total_fs },
				_ofrag_size_fs        { ofrag_size_fs },
				_ofrag_avail_fs       { ofrag_avail_fs },
				_ofrag_bytes_fs       { ofrag_bytes_fs },
				_optr_samples_fs      { optr_samples_fs },
				_optr_fifo_samples_fs { optr_fifo_samples_fs },
				_play_underruns_fs    { play_underruns_fs }
			{ }

			void update()
			{
				_channels_fs         .value(channels);
				_format_fs           .value(format);
				_sample_rate_fs      .value(sample_rate);
				_ifrag_total_fs      .value(ifrag_total);
				_ifrag_size_fs       .value(ifrag_size);
				_ifrag_avail_fs      .value(ifrag_avail);
				_ifrag_bytes_fs      .value(ifrag_bytes);
				_ofrag_total_fs      .value(ofrag_total);
				_ofrag_size_fs       .value(ofrag_size);
				_ofrag_avail_fs      .value(ofrag_avail);
				_ofrag_bytes_fs      .value(ofrag_bytes);
				_optr_samples_fs     .value(optr_samples);
				_optr_fifo_samples_fs.value(optr_fifo_samples);
				_play_underruns_fs   .value(play_underruns);
			}

			void print(Genode::Output &out) const
			{
				char buf[512] { };

				Genode::Xml_generator xml(buf, sizeof(buf), "oss", [&] () {
					xml.attribute("channels",          channels);
					xml.attribute("format",            format);
					xml.attribute("sample_rate",       sample_rate);
					xml.attribute("ifrag_total",       ifrag_total);
					xml.attribute("ifrag_size",        ifrag_size);
					xml.attribute("ifrag_avail",       ifrag_avail);
					xml.attribute("ifrag_bytes",       ifrag_bytes);
					xml.attribute("ofrag_total",       ofrag_total);
					xml.attribute("ofrag_size",        ofrag_size);
					xml.attribute("ofrag_avail",       ofrag_avail);
					xml.attribute("ofrag_bytes",       ofrag_bytes);
					xml.attribute("optr_samples",      optr_samples);
					xml.attribute("optr_fifo_samples", optr_fifo_samples);
					xml.attribute("play_underruns",    play_underruns);
				});

				Genode::print(out, Genode::Cstring(buf));
			}
		};

		using Write_result = Vfs::File_io_service::Write_result;

	private:

		Audio(Audio const &);
		Audio &operator = (Audio const &);

		Timer::Connection _timer;

		enum {
			MAX_CHANNELS = 2u,
			SAMPLE_RATE  = 44100u,
		};
		Genode::Constructible<Play::Connection> _play[MAX_CHANNELS];

		Info &_info;
		Readonly_value_file_system<Info, 512> &_info_fs;

		using Ring_buffer = Util::Ring_buffer<128u << 10>;
		Ring_buffer _buffer[MAX_CHANNELS] { };

		static size_t _sanitize_ms(unsigned ms)
		{
			if (ms < 5)  return  5;
			if (ms > 50) return 50;

			return ms;
		}

		static unsigned _fragment_size(unsigned const sample_rate,
		                             unsigned const ms,
		                             size_t   const sample_size,
		                             unsigned const channels)
		{
			/*
			 * Get fragment size (power of two) from proper periode size.
			 */
			size_t const periode_size = sample_rate * _sanitize_ms(ms)
			                          / 1000 * sample_size * channels;

			size_t size = 0;
			while (size < periode_size) {
				size <<= 1;
			}
			return (unsigned)size;
		}

		void _with_output_duration(size_t const bytes, auto const &fn)
		{
			unsigned const frame_size   = _info.channels * _format_size(_info.format);
			unsigned const samples      = (unsigned)bytes / frame_size;
			float    const tmp_duration = float(1'000'000u)
			                            / float(_info.sample_rate)
			                            * float(samples);

			fn(Play::Duration { unsigned(tmp_duration) }, samples);
		}

		Play::Duration    _timer_trigger_duration { 0 };
		Play::Time_window _time_window { };
		bool _write_ready { true };

		unsigned _samples_per_fragment = 0;

		void _update_output_info()
		{
			_info.ofrag_bytes = unsigned((_info.ofrag_total * _info.ofrag_size)
			                  - (_info.optr_fifo_samples * _info.channels * sizeof(short)));
			_info.ofrag_avail = _info.ofrag_bytes / _info.ofrag_size;

			_info.update();
			_info_fs.value(_info);
		}

		void _for_each_sample(Ring_buffer    &buffer,
		                      unsigned const  samples,
		                      auto     const &fn) const
		{
			for (unsigned i = 0; i < samples; i++) {
				float v = 0;
				Byte_range_ptr range { (char*)&v, sizeof(v) };
				buffer.read(range);

				fn(v);
			}
		}

		void _stereo_output(unsigned       const  samples,
		                    Play::Duration const  duration)
		{
			_time_window = _play[0]->schedule_and_enqueue(_time_window, duration,
				[&] (auto &submit) {
					_for_each_sample(_buffer[0], samples,
						[&] (float const v) { submit(v); }); });

			_play[1]->enqueue(_time_window,
				[&] (auto &submit) {
					_for_each_sample(_buffer[1], samples,
						[&] (float const v) { submit(v); }); });
		}

		static unsigned _format_size(unsigned fmt)
		{
			if (fmt == 0x00000010u) /* S16LE */
				return 2u;

			return 0u;
		}

		unsigned _sample_count(Const_byte_range_ptr const &range) const
		{
			return (unsigned)range.num_bytes / (_format_size(_info.format) * _info.channels);
		}

		void _fill_buffer(Const_byte_range_ptr const &src, Ring_buffer &buffer, int channel)
		{
			short const *data = (short const*)(src.start);
			float const scale = 1.0f/32768;

			unsigned int const channels = _info.channels;
			size_t       const samples  = _sample_count(src);

			for (size_t i = 0; i < samples; i++) {
				float const v = scale * float(data[i * channels + channel]);
				buffer.write(Const_byte_range_ptr { (char const*)&v, sizeof(v) });
			}
		}

		bool _buffer_write_samples_avail(unsigned samples) const
		{
			return _buffer[0].samples_write_avail<float>() >= samples;
		}

		bool _buffer_range_avail(Const_byte_range_ptr const &src) const
		{
			return _buffer_write_samples_avail(_sample_count(src));
		}

		bool _buffer_read_samples_avail(unsigned samples) const
		{
			return _buffer[0].samples_read_avail<float>() >= samples;
		}

		bool _timer_pending { false };

		void _try_schedule_and_enqueue(bool &timer_pending)
		{
			if (timer_pending)
				return;

			if (timer_pending || !_buffer_read_samples_avail(_samples_per_fragment))
				return;

			_stereo_output(_samples_per_fragment,
			               _timer_trigger_duration);

			_info.optr_fifo_samples += _samples_per_fragment;
			_update_output_info();

			timer_pending = true;
			_timer.trigger_once(_timer_trigger_duration.us);
		}


	public:

		Audio(Genode::Env &env,
		      Info        &info,
		      Readonly_value_file_system<Info, 512> &info_fs)
		:
			_timer        { env },
			_info         { info },
			_info_fs      { info_fs }
		{
			_play[0].construct(env, "left");
			_play[1].construct(env, "right");

			_info.channels    = MAX_CHANNELS;
			_info.format      = (unsigned)0x00000010;  /* S16LE */
			_info.sample_rate = SAMPLE_RATE;

			_info.ofrag_size  = 2048;
			_info.ofrag_total = 4;
			_info.ofrag_avail = _info.ofrag_total;
			_info.ofrag_bytes = _info.ofrag_avail * _info.ofrag_size;

			update_output_duration(_info.ofrag_size);

			_info.update();
			_info_fs.value(_info);
		}

		void update_output_duration(unsigned const bytes)
		{
			_with_output_duration(bytes,
				[&] (Play::Duration const duration, unsigned const samples) {
					_timer_trigger_duration = duration;
					_samples_per_fragment   = samples;
				});
		}

		void play_timer_sigh(Genode::Signal_context_capability cap)
		{
			_timer.sigh(cap);
		}

		bool handle_play_timer()
		{
			_info.optr_fifo_samples -= _samples_per_fragment;
			_update_output_info();

			/*
			 * Try to schedule the next duration if there are
			 * samples left in the buffers. If that's not the
			 * case reset the time window to prevent the mixer
			 * for overcorrecting any late incoming schedule.
			 */
			_timer_pending = false;
			_try_schedule_and_enqueue(_timer_pending);
			if (!_timer_pending) {
				Genode::error("optr_fifo_samples: ", _info.optr_fifo_samples, " avail: ",  _buffer[0].samples_read_avail<float>());
			// 	_time_window = Play::Time_window { };
			}

			return true;
		}

		void halt_output()
		{
			_play[0]->stop();
			_play[1]->stop();

			_time_window = Play::Time_window { };
		}

		void enable_output(bool enable)
		{
			if (enable == false)
				halt_output();
		}

		bool read_ready() const
		{
			return false;
		}

		bool write_ready() const
		{
			/* XXX is checking for _samples_per_fragment okay in case ? */
			return _buffer_write_samples_avail(_samples_per_fragment);
		}

		bool read(Byte_range_ptr const &dst, size_t &out_size)
		{
			(void)dst;

			out_size = 0;

			return false;
		}

		Write_result write(Const_byte_range_ptr const &src, size_t &out_size)
		{
			/* XXX support partial write? */
			if (!_buffer_range_avail(src))
				return Write_result::WRITE_ERR_WOULD_BLOCK;

			out_size = src.num_bytes;

			/* check channel has its own buffer */
			for (unsigned i = 0; i < _info.channels; i++)
				_fill_buffer(src, _buffer[i], i);

			_try_schedule_and_enqueue(_timer_pending);

			return Write_result::WRITE_OK;
		}
};


class Vfs::Oss_file_system::Data_file_system : public Single_file_system
{
	private:

		Data_file_system(Data_file_system const &);
		Data_file_system &operator = (Data_file_system const &);

		Genode::Entrypoint &_ep;
		Vfs::Env::User     &_vfs_user;
		Audio              &_audio;

		struct Oss_vfs_handle : public Single_vfs_handle
		{
			Audio &_audio;

			Oss_vfs_handle(Directory_service      &ds,
			                    File_io_service   &fs,
			                    Genode::Allocator &alloc,
			                    int                flags,
			                    Audio             &audio)
			:
				Single_vfs_handle { ds, fs, alloc, flags },
				_audio { audio }
			{ }

			Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
			{
				(void)dst;
				out_count = 0;

				return READ_ERR_INVALID;
			}

			Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
			{
				Write_result const result = _audio.write(src, out_count);

				return result;
			}

			bool read_ready() const override
			{
				return _audio.read_ready();
			}

			bool write_ready() const override
			{
				return _audio.write_ready();
			}
		};

		using Registered_handle = Genode::Registered<Oss_vfs_handle>;
		using Handle_registry   = Genode::Registry<Registered_handle>;

		Handle_registry _handle_registry { };

		Genode::Io_signal_handler<Vfs::Oss_file_system::Data_file_system> _play_timer {
			_ep, *this, &Vfs::Oss_file_system::Data_file_system::_handle_play_timer };

		void _handle_play_timer()
		{
			if (_audio.handle_play_timer())
				_vfs_user.wakeup_vfs_user();
		}


	public:

		Data_file_system(Genode::Entrypoint &ep,
		                 Vfs::Env::User     &vfs_user,
		                 Audio              &audio,
		                 Name         const &name)
		:
			Single_file_system { Node_type::CONTINUOUS_FILE, name.string(),
			                     Node_rwx::ro(), Genode::Xml_node("<data/>") },

			_ep       { ep },
			_vfs_user { vfs_user },
			_audio    { audio }
		{
			_audio.play_timer_sigh(_play_timer);
		}

		static const char *name()   { return "data"; }
		char const *type() override { return "data"; }

		/*********************************
		 ** Directory service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned flags,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (!_single_file(path)) {
				return OPEN_ERR_UNACCESSIBLE;
			}

			try {
				*out_handle = new (alloc)
					Registered_handle(_handle_registry, *this, *this, alloc, flags,
					                  _audio);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs_handle *, file_size) override
		{
			return FTRUNCATE_OK;
		}
};


struct Vfs::Oss_file_system::Local_factory : File_system_factory
{
	using Label = Genode::String<64>;
	Label const _label;
	Name  const _name;

	Vfs::Env &_env;

	/* RO/RW files */
	Readonly_value_file_system<unsigned>  _channels_fs          { "channels", 0U };
	Readonly_value_file_system<unsigned>  _format_fs            { "format", 0U };
	Readonly_value_file_system<unsigned>  _sample_rate_fs       { "sample_rate", 0U };
	Value_file_system<unsigned>           _ifrag_total_fs       { "ifrag_total", 0U };
	Value_file_system<unsigned>           _ifrag_size_fs        { "ifrag_size", 0U} ;
	Readonly_value_file_system<unsigned>  _ifrag_avail_fs       { "ifrag_avail", 0U };
	Readonly_value_file_system<unsigned>  _ifrag_bytes_fs       { "ifrag_bytes", 0U };
	Value_file_system<unsigned>           _ofrag_total_fs       { "ofrag_total", 0U };
	Value_file_system<unsigned>           _ofrag_size_fs        { "ofrag_size", 0U} ;
	Readonly_value_file_system<unsigned>  _ofrag_avail_fs       { "ofrag_avail", 0U };
	Readonly_value_file_system<unsigned>  _ofrag_bytes_fs       { "ofrag_bytes", 0U };
	Readonly_value_file_system<long long> _optr_samples_fs      { "optr_samples", 0LL };
	Readonly_value_file_system<unsigned>  _optr_fifo_samples_fs { "optr_fifo_samples", 0U };
	Value_file_system<unsigned>           _play_underruns_fs    { "play_underruns", 0U };
	Value_file_system<unsigned>           _enable_input_fs      { "enable_input", 1U };
	Value_file_system<unsigned>           _enable_output_fs     { "enable_output", 1U };

	/* WO files */
	Value_file_system<unsigned>           _halt_input_fs        { "halt_input", 0U };
	Value_file_system<unsigned>           _halt_output_fs       { "halt_output", 0U };

	Audio::Info _info { _channels_fs, _format_fs, _sample_rate_fs,
	                    _ifrag_total_fs, _ifrag_size_fs,
	                    _ifrag_avail_fs, _ifrag_bytes_fs,
	                    _ofrag_total_fs, _ofrag_size_fs,
	                    _ofrag_avail_fs, _ofrag_bytes_fs,
	                    _optr_samples_fs, _optr_fifo_samples_fs,
	                    _play_underruns_fs };

	Readonly_value_file_system<Audio::Info, 512> _info_fs { "info", _info };

	Audio _audio { _env.env(), _info, _info_fs };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _enable_input_handler {
		_enable_input_fs, "/enable_input",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_enable_input_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _enable_output_handler {
		_enable_output_fs, "/enable_output",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_enable_output_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _halt_input_handler {
		_halt_input_fs, "/halt_input",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_halt_input_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _halt_output_handler {
		_halt_output_fs, "/halt_output",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_halt_output_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _ofrag_total_handler {
		_ofrag_total_fs, "/ofrag_total",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_ofrag_total_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _ofrag_size_handler {
		_ofrag_size_fs, "/ofrag_size",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_ofrag_size_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _play_underruns_handler {
		_play_underruns_fs, "/play_underruns",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_play_underruns_changed };


	/********************
	 ** Watch handlers **
	 ********************/

	void _enable_input_changed()
	{
	}

	void _halt_input_changed()
	{
	}

	void _enable_output_changed()
	{
		bool enable = (bool)_enable_output_fs.value();
		_audio.enable_output(enable);
	}

	void _halt_output_changed()
	{
		_audio.halt_output();
	}

	void _ofrag_total_changed()
	{
		/*
		 * NOP for now as it is set in tandem with ofrag_size
		 * that in return limits number of fragments.
		 */
	}

	void _ofrag_size_changed()
	{
		unsigned ofrag_size_new = _ofrag_size_fs.value();

		ofrag_size_new = Genode::max(ofrag_size_new,  2048u);  /*  512 S16LE stereo -> 11.6 ms at 44.1kHz */
		ofrag_size_new = Genode::min(ofrag_size_new, 16384u);  /* 4096 S16LE stereo -> 92.8 ms at 44.1kHz */

		_info.ofrag_size = ofrag_size_new;

		_info.ofrag_total = 3;

		_info.ofrag_avail = _info.ofrag_total;
		_info.ofrag_bytes = _info.ofrag_total * _info.ofrag_size;

		_audio.update_output_duration(_info.ofrag_size);

		_info.update();
		_info_fs.value(_info);

		Genode::log(__func__, ": ", _info);
	}

	void _play_underruns_changed()
	{
		/* reset counter */
		_info.play_underruns = 0;

		_info.update();
		_info_fs.value(_info);
	}

	static Name name(Xml_node config)
	{
		return config.attribute_value("name", Name("oss_next"));
	}

	Data_file_system _data_fs;

	Local_factory(Vfs::Env &env, Xml_node config)
	:
		_label   { config.attribute_value("label", Label("")) },
		_name    { name(config) },
		_env     { env },
		_data_fs { _env.env().ep(), env.user(), _audio, name(config) }
	{ }

	Vfs::File_system *create(Vfs::Env&, Xml_node node) override
	{
		if (node.has_type("data"))
			return &_data_fs;

		if (node.has_type("info"))
			return &_info_fs;

		if (node.has_type(Readonly_value_file_system<unsigned>::type_name())) {

			if (_channels_fs.matches(node))          return &_channels_fs;

			if (_sample_rate_fs.matches(node))       return &_sample_rate_fs;

			if (_ifrag_avail_fs.matches(node))       return &_ifrag_avail_fs;

			if (_ifrag_bytes_fs.matches(node))       return &_ifrag_bytes_fs;

			if (_ofrag_avail_fs.matches(node))       return &_ofrag_avail_fs;

			if (_ofrag_bytes_fs.matches(node))       return &_ofrag_bytes_fs;

			if (_format_fs.matches(node))            return &_format_fs;

			if (_optr_samples_fs.matches(node))      return &_optr_samples_fs;

			if (_optr_fifo_samples_fs.matches(node)) return &_optr_fifo_samples_fs;
		}

		if (node.has_type(Value_file_system<unsigned>::type_name())) {

			if (_enable_input_fs.matches(node))   return &_enable_input_fs;

			if (_enable_output_fs.matches(node))  return &_enable_output_fs;

			if (_halt_input_fs.matches(node))     return &_halt_input_fs;

			if (_halt_output_fs.matches(node))    return &_halt_output_fs;

			if (_ifrag_total_fs.matches(node))    return &_ifrag_total_fs;

			if (_ifrag_size_fs.matches(node))     return &_ifrag_size_fs;

			if (_ofrag_total_fs.matches(node))    return &_ofrag_total_fs;

			if (_ofrag_size_fs.matches(node))     return &_ofrag_size_fs;

			if (_play_underruns_fs.matches(node)) return &_play_underruns_fs;
		}

		return nullptr;
	}
};


class Vfs::Oss_file_system::Compound_file_system : private Local_factory,
                                                   public  Vfs::Dir_file_system
{
	private:

		using Name = Oss_file_system::Name;

		using Config = String<1024>;
		static Config _config(Name const &name)
		{
			char buf[Config::capacity()] { };

			/*
			 * By not using the node type "dir", we operate the
			 * 'Dir_file_system' in root mode, allowing multiple sibling nodes
			 * to be present at the mount point.
			 */
			Genode::Xml_generator xml(buf, sizeof(buf), "compound", [&] () {

				xml.node("data", [&] () {
					xml.attribute("name", name); });

				xml.node("dir", [&] () {
					xml.attribute("name", Name(".", name));
					xml.node("info", [&] () { });

					xml.node("readonly_value", [&] {
						xml.attribute("name", "channels");
					});

					xml.node("readonly_value", [&] {
							 xml.attribute("name", "sample_rate");
					});

					xml.node("readonly_value", [&] {
						xml.attribute("name", "format");
					});

					xml.node("value", [&] {
						xml.attribute("name", "enable_input");
					});

					xml.node("value", [&] {
						xml.attribute("name", "enable_output");
					});

					xml.node("value", [&] {
						xml.attribute("name", "halt_input");
					});

					xml.node("value", [&] {
						xml.attribute("name", "halt_output");
					});

					xml.node("value", [&] {
						xml.attribute("name", "ifrag_total");
					});

					xml.node("value", [&] {
						 xml.attribute("name", "ifrag_size");
					});

					xml.node("readonly_value", [&] {
						 xml.attribute("name", "ifrag_avail");
					});

					xml.node("readonly_value", [&] {
						 xml.attribute("name", "ifrag_bytes");
					});

					xml.node("value", [&] {
						xml.attribute("name", "ofrag_total");
					});

					xml.node("value", [&] {
						 xml.attribute("name", "ofrag_size");
					});

					xml.node("readonly_value", [&] {
						 xml.attribute("name", "ofrag_avail");
					});

					xml.node("readonly_value", [&] {
						 xml.attribute("name", "ofrag_bytes");
					});

					xml.node("readonly_value", [&] {
						 xml.attribute("name", "optr_samples");
					});

					xml.node("readonly_value", [&] {
						 xml.attribute("name", "optr_fifo_samples");
					});

					xml.node("value", [&] {
						 xml.attribute("name", "play_underruns");
					});
				});
			});

			return Config(Genode::Cstring(buf));
		}

	public:

		Compound_file_system(Vfs::Env &vfs_env, Genode::Xml_node node)
		:
			Local_factory { vfs_env, node },
			Vfs::Dir_file_system { vfs_env,
			                       Xml_node(_config(Local_factory::name(node)).string()),
			                       *this }
		{ }

		static const char *name() { return "oss_next"; }

		char const *type() override { return name(); }
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node config) override
		{
			return new (env.alloc())
				Vfs::Oss_file_system::Compound_file_system(env, config);
		}
	};

	static Factory f;
	return &f;
}
