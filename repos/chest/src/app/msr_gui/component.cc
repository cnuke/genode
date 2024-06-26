/*
 * \brief  GUI for managing power states for AMD & Intel
 * \author Alexander Boettcher
 * \date   2022-10-15
 */

/*
 * Copyright (C) 2022-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/duration.h>
#include <base/signal.h>

#include <os/reporter.h>

#include "xml_tools.h"
#include "button.h"

using namespace Genode;


struct State
{
	unsigned value { ~0U };

	bool valid()      const { return value != ~0U; }
	void invalidate()       { value = ~0U; }

	bool operator == (State const &o) const { return value == o.value; }
	bool operator != (State const &o) const { return value != o.value; }
};


class Power
{
	private:
		enum { CPU_MUL = 10000 };

		Env                    &_env;
		Attached_rom_dataspace  _info     { _env, "info" };
		Signal_handler<Power>   _info_sig { _env.ep(), *this, &Power::_info_update };

		Attached_rom_dataspace  _hover     { _env, "hover" };
		Signal_handler<Power>   _hover_sig { _env.ep(), *this, &Power::_hover_update};

		Expanding_reporter      _dialog          { _env, "dialog", "dialog" };
		Expanding_reporter      _msr_config      { _env, "config", "config" };

		State                   _setting_cpu       { };
		State                   _setting_hovered   { };
		unsigned                _last_cpu          { ~0U };
		bool                    _initial_hwp_cap   { false };
		bool                    _none_hovered      { false };
		bool                    _apply_period      { false };
		bool                    _apply_hovered     { false };
		bool                    _apply_all_hovered { false };
		bool                    _hwp_epp_perf      { false };
		bool                    _hwp_epp_bala      { false };
		bool                    _hwp_epp_ener      { false };
		bool                    _hwp_epp_custom    { false };
		bool                    _epb_perf          { false };
		bool                    _epb_bala          { false };
		bool                    _epb_ener          { false };
		bool                    _epb_custom        { false };
		bool                    _hwp_on_selected   { false };
		bool                    _hwp_on_hovered    { false };
		bool                    _epb_custom_select { false };
		bool                    _epp_custom_select { false };
		bool                    _hwp_req_custom    { false };
		bool                    _hwp_req_cus_sel   { false };
		bool                    _hwp_req_auto      { false };
		bool                    _hwp_req_auto_sel  { false };
		bool                    _apply_select      { false };
		bool                    _apply_all_select  { false };
		bool                    _apply_select_per  { false };
		bool                    _pstate_max        { false };
		bool                    _pstate_mid        { false };
		bool                    _pstate_min        { false };
		bool                    _pstate_custom     { false };
		bool                    _pstate_custom_sel { false };
		bool                    _hwp_enabled_once  { false };
		bool                    _hover_normal      { false };
		bool                    _hover_advanced    { false };
		bool                    _select_normal     { true  };
		bool                    _select_advanced   { false };
		bool                    _hover_rapl_detail  { false };
		bool                    _select_rapl_detail { false };

		Button_hub<5, 0, 9, 0>  _timer_period { };

		/* ranges are set by read out hardware features */
		Button_hub<1, 0, 10, 0> _amd_pstate { };

		/* PERFORMANCE = 0, BALANCED = 7, POWER_SAVING = 15 */
		enum { EPB_PERF = 0, EPB_BALANCED = 7, EPB_POWER_SAVE = 15 };
		Button_hub<1, 0, 15, 7>    _intel_epb  { };

		/* ranges are set by read out hardware features */
		Button_hub<1, 0, 255, 128> _intel_hwp_min { };
		Button_hub<1, 0, 255, 128> _intel_hwp_max { };
		Button_hub<1, 0, 255, 128> _intel_hwp_des { };

		Button_hub<1, 0, 255, 128> _intel_hwp_pck_min { };
		Button_hub<1, 0, 255, 128> _intel_hwp_pck_max { };
		Button_hub<1, 0, 255, 128> _intel_hwp_pck_des { };

		/* PERFORMANCE = 0, BALANCED = 128, ENERGY = 255 */
		enum { EPP_PERF = 0, EPP_BALANCED = 128, EPP_ENERGY = 255 };
		Button_hub<1, 0, 255, 128> _intel_hwp_epp { };

		void _generate_msr_config(bool, bool = false);
		void _generate_msr_cpu(Xml_generator &, unsigned, unsigned);
		void _info_update();
		void _hover_update();
		void _settings_period(Xml_generator &);
		void _settings_mode  (Xml_generator &);
		void _cpu_energy(Xml_generator &, Xml_node const &, unsigned &);
		void _cpu_energy_detail(Xml_generator &, Xml_node const &, unsigned &,
		                        char const *);
		void _cpu_power_info(Xml_generator &, Xml_node const &, unsigned &);
		void _cpu_power_info_detail(Xml_generator &, Xml_node const &,
		                            unsigned &, char const *);
		void _cpu_power_limit(Xml_generator &, Xml_node const &, unsigned &);
		void _cpu_power_limit_dram_pp0_pp1(Xml_generator &, Xml_node const &,
		                                   unsigned &, char const *);
		void _cpu_power_limit_common(Xml_generator &, Xml_node const &,
		                             unsigned &, char const *);
		void _cpu_power_limit_headline(Xml_generator &, unsigned &, char const *);
		void _cpu_perf_status(Xml_generator &, Xml_node const &, unsigned &);
		void _cpu_perf_status_detail(Xml_generator &, Xml_node const &,
		                             char const *, unsigned &);
		void _cpu_temp(Xml_generator &, Xml_node const &);
		void _cpu_freq(Xml_generator &, Xml_node const &);
		void _cpu_setting(Xml_generator &, Xml_node const &);
		void _settings_view(Xml_generator &, Xml_node const &,
		                    String<12> const &, unsigned, bool);
		void _settings_amd(Xml_generator &, Xml_node const &, bool);
		void _settings_intel_epb(Xml_generator &, Xml_node const &, bool);
		void _settings_intel_hwp(Xml_generator &, Xml_node const &, bool);
		void _settings_intel_hwp_req(Xml_generator &, Xml_node const &,
		                             unsigned, unsigned, uint64_t, bool, bool,
		                             unsigned &);

		unsigned _cpu_name(Xml_generator &, Xml_node const &, unsigned);

		template <typename T>
		void hub(Xml_generator &xml, T &hub, char const *name)
		{
			hub.for_each([&](Button_state &state, unsigned pos) {
				xml.attribute("name", Genode::String<20>("hub-", name, "-", pos));

				Genode::String<12> number(state.current);

				xml.node("button", [&] () {
					xml.attribute("name", Genode::String<20>("hub-", name, "-", pos));
					xml.node("label", [&] () {
						xml.attribute("text", number);
					});

					if (state.active())
						xml.attribute("hovered", true);
				});
			});
		}

		unsigned cpu_id(Genode::Xml_node const &cpu) const
		{
			auto const affinity_x = cpu.attribute_value("x", 0U);
			auto const affinity_y = cpu.attribute_value("y", 0U);
			return affinity_x * CPU_MUL + affinity_y;
		}

	public:

		Power(Env &env) : _env(env)
		{
			_info.sigh(_info_sig);
			_hover.sigh(_hover_sig);

			_timer_period.set(unsigned(Milliseconds(4000).value));

			_info_update();
		}
};


void Power::_hover_update()
{
	_hover.update();

	if (!_hover.valid())
		return;

	Genode::Xml_node const hover = _hover.xml();

	/* settings and apply button */
	typedef Genode::String<20> Button;
	Button button = query_attribute<Button>(hover, "dialog", "frame", "hbox",
	                                        "vbox", "hbox", "button", "name");
	if (button == "") /* intel hwp, epb, epp & AMD pstate buttons */
		button = query_attribute<Button>(hover, "dialog", "frame", "hbox",
		                                 "vbox", "frame", "hbox", "button", "name");

	if (button == "") /* intel rapl button */
		button = query_attribute<Button>(hover, "dialog", "frame", "hbox",
		                                 "vbox", "frame", "hbox", "vbox", "hbox", "button", "name");

	bool click_valid = false;
	Button click = query_attribute<Button>(hover, "button", "left");
	if (click == "yes") {
		click = "left";
		click_valid = true;
	} else {
		click = query_attribute<Button>(hover, "button", "right");
		if (click == "yes") {
			click = "right";
			click_valid = true;
		} else {
/*
			long y = query_attribute<long>(hover, "button", "wheel");
			click_valid = y;
			if (y < 0) click = "wheel_down";
			if (y > 0) click = "wheel_up";
*/
		}
	}

	if (_apply_select)      _apply_select     = false;
	if (_apply_all_select)  _apply_all_select = false;
	if (_apply_select_per)  _apply_select_per = false;

	bool refresh = false;

	if (click_valid && _setting_hovered.valid()) {
		if (_setting_cpu == _setting_hovered) {
			_setting_cpu.invalidate();
		} else
			_setting_cpu = _setting_hovered;

		refresh = true;
	}

	if (click_valid && (_hover_normal || _hover_advanced)) {

		if (_hover_normal)   { _select_normal = true; _select_advanced = false; }
		if (_hover_advanced) { _select_advanced = true; _select_normal = false; }

		refresh = true;
	}

	if (click_valid && (_hover_rapl_detail)) {

		_select_rapl_detail = !_select_rapl_detail;

		refresh = true;
	}

	if (click_valid && (_apply_hovered || _apply_all_hovered)) {

		_generate_msr_config(_apply_all_hovered);

		if (_apply_hovered)     _apply_select     = true;
		if (_apply_all_hovered) _apply_all_select = true;

		refresh = true;
	}

	if (click_valid && (_apply_period)) {

		_generate_msr_config(_apply_all_hovered, _apply_period);

		_apply_select_per = true;

		refresh = true;
	}

	if (click_valid && _setting_cpu.valid()) {

		if (_timer_period.any_active()) {
			if (click == "left")
				refresh = refresh || _timer_period.update_inc();
			else
			if (click == "right")
				refresh = refresh || _timer_period.update_dec();

			if (_timer_period.value() < 100)
				_timer_period.set(100);
		}

		if (_amd_pstate.any_active()) {
			if (click == "left")
				refresh = refresh || _amd_pstate.update_inc();
			else
			if (click == "right")
				refresh = refresh || _amd_pstate.update_dec();
		}

		if (_intel_epb.any_active()) {
			if (click == "left")
				refresh = refresh || _intel_epb.update_inc();
			else
			if (click == "right")
				refresh = refresh || _intel_epb.update_dec();
		}

		if (_intel_hwp_min.any_active()) {
			if (click == "left")
				refresh = refresh || _intel_hwp_min.update_inc();
			else
			if (click == "right")
				refresh = refresh || _intel_hwp_min.update_dec();
		}

		if (_intel_hwp_max.any_active()) {
			if (click == "left")
				refresh = refresh || _intel_hwp_max.update_inc();
			else
			if (click == "right")
				refresh = refresh || _intel_hwp_max.update_dec();
		}

		if (_intel_hwp_des.any_active()) {
			if (click == "left")
				refresh = refresh || _intel_hwp_des.update_inc();
			else
			if (click == "right")
				refresh = refresh || _intel_hwp_des.update_dec();
		}

		if (_intel_hwp_epp.any_active()) {
			if (click == "left")
				refresh = refresh || _intel_hwp_epp.update_inc();
			else
			if (click == "right")
				refresh = refresh || _intel_hwp_epp.update_dec();
		}

		if (click_valid && _hwp_on_hovered) {
			_hwp_on_selected = true;
			refresh = true;
		}

		if (_hwp_epp_perf) {
			_intel_hwp_epp.set(EPP_PERF);
			refresh = true;
		}

		if (_hwp_epp_bala) {
			_intel_hwp_epp.set(EPP_BALANCED);
			refresh = true;
		}

		if (_hwp_epp_ener) {
			_intel_hwp_epp.set(EPP_ENERGY);
			refresh = true;
		}

		if (_hwp_epp_custom) {
			_epp_custom_select = !_epp_custom_select;
			refresh = true;
		}

		if (_hwp_req_custom) {
			_hwp_req_cus_sel = !_hwp_req_cus_sel;
			refresh = true;
		}

		if (_hwp_req_auto) {
			_hwp_req_auto_sel = !_hwp_req_auto_sel;
			refresh = true;
		}

		if (_epb_perf) {
			_intel_epb.set(EPB_PERF);
			refresh = true;
		}

		if (_epb_bala) {
			_intel_epb.set(EPB_BALANCED);
			refresh = true;
		}

		if (_epb_ener) {
			_intel_epb.set(EPB_POWER_SAVE);
			refresh = true;
		}

		if (_epb_custom) {
			_epb_custom_select = !_epb_custom_select;
			refresh = true;
		}

		if (_pstate_max) {
			_amd_pstate.set(_amd_pstate.min());
			refresh = true;
		}

		if (_pstate_mid) {
			_amd_pstate.set((_amd_pstate.max() - _amd_pstate.min() + 1) / 2);
			refresh = true;
		}

		if (_pstate_min) {
			_amd_pstate.set(_amd_pstate.max());
			refresh = true;
		}

		if (_pstate_custom) {
			_pstate_custom_sel = !_pstate_custom_sel;
			refresh = true;
		}
	}

	if (click_valid) {
		if (refresh)
			_info_update();
		return;
	}

	auto const before_hovered        = _setting_hovered;
	auto const before_cpu            = _setting_cpu;
	auto const before_period         = _timer_period.any_active();
	auto const before_pstate         = _amd_pstate.any_active();
	auto const before_epb            = _intel_epb.any_active();
	auto const before_hwp_min        = _intel_hwp_min.any_active();
	auto const before_hwp_max        = _intel_hwp_max.any_active();
	auto const before_hwp_des        = _intel_hwp_des.any_active();
	auto const before_hwp_epp        = _intel_hwp_epp.any_active();
	auto const before_none           = _none_hovered;
	auto const before_apply_period   = _apply_period;
	auto const before_apply          = _apply_hovered;
	auto const before_all_apply      = _apply_all_hovered;
	auto const before_hwp_epp_perf   = _hwp_epp_perf;
	auto const before_hwp_epp_bala   = _hwp_epp_bala;
	auto const before_hwp_epp_ener   = _hwp_epp_ener;
	auto const before_hwp_epp_custom = _hwp_epp_custom;
	auto const before_hwp_req_custom = _hwp_req_custom;
	auto const before_hwp_req_auto   = _hwp_req_auto;
	auto const before_epb_perf       = _epb_perf;
	auto const before_epb_bala       = _epb_bala;
	auto const before_epb_ener       = _epb_ener;
	auto const before_epb_custom     = _epb_custom;
	auto const before_hwp_on         = _hwp_on_hovered;
	auto const before_pstate_max     = _pstate_max;
	auto const before_pstate_mid     = _pstate_mid;
	auto const before_pstate_min     = _pstate_min;
	auto const before_pstate_custom  = _pstate_custom;
	auto const before_normal         = _hover_normal;
	auto const before_advanced       = _hover_advanced;
	auto const before_rapl_detail    = _hover_rapl_detail;

	bool any = button != "";

	bool const hovered_setting = any && (button == "settings")                && (!(any = false));
	bool const hovered_period  = any && (String<11>(button) == "hub-period")  && (!(any = false));
	bool const hovered_pstate  = any && (String<11>(button) == "hub-pstate")  && (!(any = false));
	bool const hovered_epb     = any && (String< 8>(button) == "hub-epb")     && (!(any = false));
	bool const hovered_hwp_min = any && (String<12>(button) == "hub-hwp_min") && (!(any = false));
	bool const hovered_hwp_max = any && (String<12>(button) == "hub-hwp_max") && (!(any = false));
	bool const hovered_hwp_des = any && (String<12>(button) == "hub-hwp_des") && (!(any = false));
	bool const hovered_hwp_epp = any && (String<12>(button) == "hub-hwp_epp") && (!(any = false));

	_none_hovered      = any && (button == "none")         && (!(any = false));
	_apply_hovered     = any && (button == "apply")        && (!(any = false));
	_apply_all_hovered = any && (button == "applyall")     && (!(any = false));
	_apply_period      = any && (button == "apply_period") && (!(any = false));

	_hwp_on_hovered    = any && (button == "hwp_on")  && (!(any = false));

	_hwp_epp_perf      = any && (button == "hwp_epp-perf")   && (!(any = false));
	_hwp_epp_bala      = any && (button == "hwp_epp-bala")   && (!(any = false));
	_hwp_epp_ener      = any && (button == "hwp_epp-ener")   && (!(any = false));
	_hwp_epp_custom    = any && (button == "hwp_epp-custom") && (!(any = false));

	_hwp_req_custom    = any && (button == "hwp_req-custom") && (!(any = false));
	_hwp_req_auto      = any && (button == "hwp_req-auto")   && (!(any = false));

	_epb_perf   = any && (button == "epb-perf")   && (!(any = false));
	_epb_bala   = any && (button == "epb-bala")   && (!(any = false));
	_epb_ener   = any && (button == "epb-ener")   && (!(any = false));
	_epb_custom = any && (button == "epb-custom") && (!(any = false));

	_pstate_max    = any && (button == "pstate-max")    && (!(any = false));
	_pstate_mid    = any && (button == "pstate-mid")    && (!(any = false));
	_pstate_min    = any && (button == "pstate-min")    && (!(any = false));
	_pstate_custom = any && (button == "pstate-custom") && (!(any = false));

	_hover_normal   = any && (button == "normal") && (!(any = false));
	_hover_advanced = any && (button == "advanced") && (!(any = false));

	_hover_rapl_detail = any && (button == "info") && (!(any = false));

	if (hovered_setting) {
		_setting_hovered.value = query_attribute<unsigned>(hover, "dialog", "frame",
		                                                   "hbox", "vbox", "hbox", "name");
	} else if (_setting_hovered.valid())
		_setting_hovered.invalidate();

	if (hovered_period || before_period) {
		_timer_period.for_each([&](Button_state &state, unsigned pos) {
			Genode::String<20> pos_name { "hub-period-", pos };
			if (Genode::String<20>(button) == pos_name) {
				state.hovered = hovered_period;
			} else
				state.hovered = false;
		});
	}

	_amd_pstate.for_each([&](Button_state &state, unsigned) {
		state.hovered = hovered_pstate;
	});

	_intel_epb.for_each([&](Button_state &state, unsigned) {
		state.hovered = hovered_epb;
	});

	_intel_hwp_min.for_each([&](Button_state &state, unsigned) {
		state.hovered = hovered_hwp_min;
	});

	_intel_hwp_max.for_each([&](Button_state &state, unsigned) {
		state.hovered = hovered_hwp_max;
	});

	_intel_hwp_des.for_each([&](Button_state &state, unsigned) {
		state.hovered = hovered_hwp_des;
	});

	_intel_hwp_epp.for_each([&](Button_state &state, unsigned) {
		state.hovered = hovered_hwp_epp;
	});

	if ((before_hovered        != _setting_hovered)   ||
	    (before_cpu            != _setting_cpu)       ||
	    (before_period         != hovered_period)     ||
	    (before_pstate         != hovered_pstate)     ||
	    (before_epb            != hovered_epb)        ||
	    (before_hwp_min        != hovered_hwp_min)    ||
	    (before_hwp_max        != hovered_hwp_max)    ||
	    (before_hwp_des        != hovered_hwp_des)    ||
	    (before_hwp_epp        != hovered_hwp_epp)    ||
	    (before_none           != _none_hovered)      ||
	    (before_apply_period   != _apply_period)      ||
	    (before_apply          != _apply_hovered)     ||
	    (before_all_apply      != _apply_all_hovered) ||
	    (before_hwp_epp_perf   != _hwp_epp_perf)      ||
	    (before_hwp_epp_bala   != _hwp_epp_bala)      ||
	    (before_hwp_epp_ener   != _hwp_epp_ener)      ||
	    (before_hwp_epp_custom != _hwp_epp_custom)    ||
	    (before_hwp_req_custom != _hwp_req_custom)    ||
	    (before_hwp_req_auto   != _hwp_req_auto)      ||
	    (before_epb_perf       != _epb_perf)          ||
	    (before_epb_bala       != _epb_bala)          ||
	    (before_epb_ener       != _epb_ener)          ||
	    (before_epb_custom     != _epb_custom)        ||
	    (before_hwp_on         != _hwp_on_hovered)    ||
	    (before_normal         != _hover_normal)      ||
	    (before_advanced       != _hover_advanced)    ||
	    (before_rapl_detail    != _hover_rapl_detail) ||
	    (before_pstate_max     != _pstate_max)        ||
	    (before_pstate_mid     != _pstate_mid)        ||
	    (before_pstate_min     != _pstate_min)        ||
	    (before_pstate_custom  != _pstate_custom))
		refresh = true;

	if (refresh)
		_info_update();
}


void Power::_info_update ()
{
	_info.update();

	if (!_info.valid())
		return;

	_dialog.generate([&] (Xml_generator &xml) {

		xml.node("frame", [&] {

			xml.node("hbox", [&] {

				unsigned cpu_count = 0;

				xml.node("vbox", [&] {
					xml.attribute("name", 1);

					unsigned loc_x_last = ~0U;

					_info.xml().for_each_sub_node("cpu", [&](Genode::Xml_node const &cpu) {
						loc_x_last = _cpu_name(xml, cpu, loc_x_last);
						cpu_count ++;
					});
				});

				xml.node("vbox", [&] {
					xml.attribute("name", 2);
					_info.xml().for_each_sub_node("cpu", [&](Genode::Xml_node const &cpu) {
						_cpu_temp(xml, cpu);
					});
				});

				xml.node("vbox", [&] {
					xml.attribute("name", 3);
					_info.xml().for_each_sub_node("cpu", [&](Genode::Xml_node const &cpu) {
						_cpu_freq(xml, cpu);
					});
				});

				xml.node("vbox", [&] {
					xml.attribute("name", 4);
					_info.xml().for_each_sub_node("cpu", [&](Genode::Xml_node const &cpu) {
						_cpu_setting(xml, cpu);
					});
				});

				bool const re_eval = _setting_cpu.value != _last_cpu;

				_info.xml().for_each_sub_node("cpu", [&](Genode::Xml_node const &cpu) {
					if (cpu_id(cpu) != _setting_cpu.value)
						return;

					auto const affinity_x = cpu.attribute_value("x", 0U);
					auto const affinity_y = cpu.attribute_value("y", 0U);

					xml.node("vbox", [&] {
						xml.attribute("name", 5);

						auto const name = String<12>("CPU ", affinity_x, "x", affinity_y);
						_settings_view(xml, cpu, name, cpu_count, re_eval);
					});

					_last_cpu = cpu_id(cpu);
				});
			});
		});
	});
}


void Power::_generate_msr_cpu(Xml_generator &xml,
                              unsigned affinity_x, unsigned affinity_y)
{
	xml.node("cpu", [&] {
		xml.attribute("x", affinity_x);
		xml.attribute("y", affinity_y);

		xml.node("pstate", [&] {
			xml.attribute("rw_command", _amd_pstate.value());
		});

		xml.node("hwp_request", [&] {
			xml.attribute("min",     _intel_hwp_min.value());
			xml.attribute("max",     _intel_hwp_max.value());
			if (_hwp_req_auto_sel)
				xml.attribute("desired", 0);
			else
				xml.attribute("desired", _intel_hwp_des.value());
			xml.attribute("epp",     _intel_hwp_epp.value());
		});

		xml.node("energy_perf_bias", [&] {
			xml.attribute("raw", _intel_epb.value());
		});

		if (_hwp_on_selected && !_hwp_enabled_once) {
			xml.node("hwp", [&] {
				xml.attribute("enable", _hwp_on_selected);
			});
		}
	});
}


void Power::_generate_msr_config(bool all_cpus, bool const apply_period)
{
	if (!_setting_cpu.valid())
		return;

	_msr_config.generate([&] (Xml_generator &xml) {

/*
		xml.attribute("verbose", false);
*/
		xml.attribute("update_rate_us", _timer_period.value() * 1000);

		/* if soley period changed, don't rewrite HWP parameters */
		if (apply_period)
			return;

		if (all_cpus) {
			_info.xml().for_each_sub_node("cpu", [&](Genode::Xml_node const &cpu) {

				auto const affinity_x = cpu.attribute_value("x", 0U);
				auto const affinity_y = cpu.attribute_value("y", 0U);

				_generate_msr_cpu(xml, affinity_x, affinity_y);
			});
		} else {
			auto const affinity_x = _setting_cpu.value / CPU_MUL;
			auto const affinity_y = _setting_cpu.value % CPU_MUL;

			_generate_msr_cpu(xml, affinity_x, affinity_y);
		}
	});
}


unsigned Power::_cpu_name(Xml_generator &xml, Xml_node const &cpu, unsigned last_x)
{
	auto const affinity_x = cpu.attribute_value("x", 0U);
	auto const affinity_y = cpu.attribute_value("y", 0U);
	auto const core_type  = cpu.attribute_value("type", String<2> (""));
	bool const same_x     = (affinity_x == last_x) &&
	                        (core_type != "E");

	xml.node("hbox", [&] {
		auto const name = String<12>(same_x ? "" : "CPU ", affinity_x, "x",
		                             affinity_y, " ", core_type, " |");

		xml.attribute("name", cpu_id(cpu));

		xml.node("label", [&] {
			xml.attribute("name", 1);
			xml.attribute("align", "right");
			xml.attribute("text", name);
		});
	});

	return affinity_x;
}


static String<12> align_string(double const value)
{
	String<12> string { };

	if (value >= 1.0d) {
		string = String<12>(uint64_t(value));

		auto const rest = uint64_t(value * 100) % 100;

		string = String<12>(string, ".", (rest < 10) ? "0" : "", rest);
	} else {
		if (value == 0.0d)
			string = String<12>("0.00");
		else
			string = String<12>(value);
	}

	/* align right */
	for (auto i = string.length(); i < string.capacity() - 1; i++) {
		string = String<12>(" ", string);
	}

	return string;
}


void Power::_cpu_energy_detail(Xml_generator &xml, Xml_node const &node,
                               unsigned      &id , char     const *text)
{
	auto const raw = node.attribute_value("raw", 0ULL);
	if (!raw)
		return;

	xml.node("hbox", [&] {
		xml.attribute("name", id++);

		double joule = 0, watt = 0;

		watt  = node.attribute_value("Watt",  watt);
		joule = node.attribute_value("Joule", joule);

		xml.node("label", [&] {
			xml.attribute("name", id++);
			xml.attribute("align", "left");
			xml.attribute("text", text);
		});

		xml.node("label", [&] {
			xml.attribute("font", "monospace/regular");
			xml.attribute("name", id++);
			xml.attribute("align", "right");
			xml.attribute("text", String<40>(align_string(watt) , " Watt | ",
			                                 align_string(joule), " Joule"));
		});
	});
}


void Power::_cpu_energy(Xml_generator &xml, Xml_node const &energy, unsigned &frames)
{
	unsigned id = 0;

	xml.node("vbox", [&] {

		xml.node("hbox", [&] {
			xml.attribute("name", id++);

			xml.node("label", [&] {
				xml.attribute("name", id++);
				xml.attribute("align", "left");
				xml.attribute("text", String<30>(" Running Average Power Limit - energy:"));
			});

			xml.node("button", [&] () {
				xml.attribute("align", "right");
				xml.attribute("name", "info");
				xml.node("label", [&] () {
					xml.attribute("text", "info");
				});

				if (_hover_rapl_detail)
					xml.attribute("hovered", true);
				if (_select_rapl_detail)
					xml.attribute("selected", true);
			});
		});

		energy.with_optional_sub_node("package", [&](auto const &node) {
			frames++;
			_cpu_energy_detail(xml, node, id, " Domain package:");
		});

		energy.with_optional_sub_node("dram", [&](auto const &node) {
			frames++;
			_cpu_energy_detail(xml, node, id, " Domain DRAM:");
		});

		energy.with_optional_sub_node("pp0", [&](auto const &node) {
			frames++;
			_cpu_energy_detail(xml, node, id, " Domain PP0: (CPUs)");
		});

		energy.with_optional_sub_node("pp1", [&](auto const &node) {
			frames++;
			_cpu_energy_detail(xml, node, id, " Domain PP1: (GPU?)");
		});
	});
}


void Power::_cpu_power_info_detail(Xml_generator &xml, Xml_node const &node,
                                   unsigned      &id , char     const *text)
{
	xml.node("vbox", [&] {
		xml.attribute("name", id++);

		double spec = 0, min = 0, max = 0, wnd = 0;

		spec = node.attribute_value("ThermalSpecPower" , spec);
		min  = node.attribute_value("MinimumPower"     , min);
		max  = node.attribute_value("MaximumPower"     , max);
		wnd  = node.attribute_value("MaximumTimeWindow", wnd);

		xml.node("hbox", [&] {
			xml.attribute("name", id++);

			xml.node("label", [&] {
				xml.attribute("name", id++);
				xml.attribute("align", "left");
				xml.attribute("text", text);
			});
		});

		xml.node("hbox", [&] {
			xml.attribute("name", id++);

			xml.node("label", [&] {
				xml.attribute("font", "monospace/regular");
				xml.attribute("name", id++);
				xml.attribute("align", "right");
				xml.attribute("text", String<40>(" Thermal spec. power ", align_string(spec), " Watt"));
			});
		});

		xml.node("hbox", [&] {
			xml.attribute("name", id++);

			xml.node("label", [&] {
				xml.attribute("font", "monospace/regular");
				xml.attribute("name", id++);
				xml.attribute("align", "right");
				xml.attribute("text", String<40>(" Minimal power ", align_string(min), " Watt"));
			});
		});

		xml.node("hbox", [&] {
			xml.attribute("name", id++);

			xml.node("label", [&] {
				xml.attribute("font", "monospace/regular");
				xml.attribute("name", id++);
				xml.attribute("align", "right");
				xml.attribute("text", String<40>(" Maximum power ", align_string(max), " Watt"));
			});
		});

		xml.node("hbox", [&] {
			xml.attribute("name", id++);

			xml.node("label", [&] {
				xml.attribute("font", "monospace/regular");
				xml.attribute("name", id++);
				xml.attribute("align", "right");
				xml.attribute("text", String<40>(" Maximum time window ", align_string(wnd), " s   "));
			});
		});
	});
}


void Power::_cpu_power_info(Xml_generator &xml, Xml_node const &info, unsigned &frames)
{
	unsigned id = 0;

	info.with_optional_sub_node("package", [&](auto const &node) {
		frames ++;
		_cpu_power_info_detail(xml, node, id, " Package power info:");
	});
	info.with_optional_sub_node("dram", [&](auto const &node) {
		frames ++;
		_cpu_power_info_detail(xml, node, id, " DRAM power info:");
	});
}


void Power::_cpu_power_limit_common(Xml_generator &xml, Xml_node const &node,
                                    unsigned      &id , char     const *text)
{
	xml.node("hbox", [&] {
		xml.attribute("name", id++);

		double power = 0, window = 0;
		bool   enable = false, clamp = false;

		power  = node.attribute_value("power"       , power);
		enable = node.attribute_value("enable"      , enable);
		clamp  = node.attribute_value("clamp"       , clamp);
		window = node.attribute_value("time_window" , window);

		xml.node("label", [&] {
			xml.attribute("font", "monospace/regular");
			xml.attribute("name", id++);
			xml.attribute("align", "left");
			xml.attribute("text", text);
		});

		xml.node("label", [&] {
			xml.attribute("font", "monospace/regular");
			xml.attribute("name", id++);
			xml.attribute("align", "right");
			xml.attribute("text", String<19>(" ", align_string(power), " Watt"));
		});

		xml.node("label", [&] {
			xml.attribute("font", "monospace/regular");
			xml.attribute("name", id++);
			xml.attribute("align", "right");
			xml.attribute("text", enable ? String<10>(" true    ") : String<10>("false    "));
		});

		xml.node("label", [&] {
			xml.attribute("font", "monospace/regular");
			xml.attribute("name", id++);
			xml.attribute("align", "right");
			xml.attribute("text", clamp ? String<10>(" true    ") : String<10>("false    "));
		});

		xml.node("label", [&] {
			xml.attribute("font", "monospace/regular");
			xml.attribute("name", id++);
			xml.attribute("align", "right");
			xml.attribute("text", String<16>(" ", align_string(window), " s"));
		});
	});
}


void Power::_cpu_power_limit_dram_pp0_pp1(Xml_generator      &xml,
                                          Xml_node     const &node,
                                          unsigned           &id,
                                          char const * const  text)
{
		bool lock = false;

		lock = node.attribute_value("lock", lock);

		xml.node("hbox", [&] {
			xml.attribute("name", id++);

			xml.node("label", [&] {
				xml.attribute("name", id++);
				xml.attribute("align", "left");
				xml.attribute("text", String<32>(text, lock ? " - LOCKED" : ""));
			});
		});

		_cpu_power_limit_common(xml, node, id, " -  ");
}


void Power::_cpu_power_limit_headline(Xml_generator      &xml,
                                      unsigned           &id,
                                      char const * const  text)
{
	xml.node("hbox", [&] {
		xml.attribute("name", id++);

		xml.node("label", [&] {
			xml.attribute("font", "monospace/regular");
			xml.attribute("name", id++);
			xml.attribute("align", "left");
			xml.attribute("text", text);
		});

		xml.node("label", [&] {
			xml.attribute("font", "monospace/regular");
			xml.attribute("name", id++);
			xml.attribute("align", "right");
			xml.attribute("text", String<19>("         power"));
		});

		xml.node("label", [&] {
			xml.attribute("font", "monospace/regular");
			xml.attribute("name", id++);
			xml.attribute("align", "right");
			xml.attribute("text", "enable");
		});

		xml.node("label", [&] {
			xml.attribute("font", "monospace/regular");
			xml.attribute("name", id++);
			xml.attribute("align", "right");
			xml.attribute("text", "clamp");
		});

		xml.node("label", [&] {
			xml.attribute("font", "monospace/regular");
			xml.attribute("name", id++);
			xml.attribute("align", "right");
			xml.attribute("text", String<16>("time window  "));
		});
	});
}


void Power::_cpu_power_limit(Xml_generator &xml, Xml_node const &limit, unsigned &)
{
	unsigned id = 0;

	xml.node("vbox", [&] {
		xml.attribute("name", id++);

		limit.with_optional_sub_node("package", [&](auto const &node) {

			bool lock = false;

			lock = node.attribute_value("lock", lock);

			xml.node("hbox", [&] {
				xml.attribute("name", id++);

				xml.node("label", [&] {
					xml.attribute("name", id++);
					xml.attribute("align", "left");
					xml.attribute("text", String<32>(" Package power limit", lock ? " LOCKED" : ""));
				});
			});

			_cpu_power_limit_headline(xml, id, "");

			node.with_optional_sub_node("limit_1", [&](auto const &node) {
				_cpu_power_limit_common(xml, node, id, " - 1");
			});

			node.with_optional_sub_node("limit_2", [&](auto const &node) {
				_cpu_power_limit_common(xml, node, id, " - 2");
			});
		});

		limit.with_optional_sub_node("dram", [&](auto const &node) {
			_cpu_power_limit_dram_pp0_pp1(xml, node, id, " DRAM power limit");
		});

		limit.with_optional_sub_node("pp0", [&](auto const &node) {
			_cpu_power_limit_dram_pp0_pp1(xml, node, id, " PP0 power limit");
		});

		limit.with_optional_sub_node("pp1", [&](auto const &node) {
			_cpu_power_limit_dram_pp0_pp1(xml, node, id, " PP1 power limit");
		});
	});
}


void Power::_cpu_perf_status_detail(Xml_generator &xml, Xml_node const &node,
                                    char const * text, unsigned &id)
{
	double abs = 0, diff = 0;

	abs  = node.attribute_value("throttle_abs",  abs);
	diff = node.attribute_value("throttle_diff", diff);

	xml.node("hbox", [&] {
		xml.attribute("name", id++);

		xml.node("label", [&] {
			xml.attribute("name", id++);
			xml.attribute("align", "left");
			xml.attribute("text", text);
		});

		xml.node("label", [&] {
			xml.attribute("font", "monospace/regular");
			xml.attribute("name", id++);
			xml.attribute("align", "right");
			xml.attribute("text", String<48>("throttle current ", align_string(diff), "s"));
		});
	});

	xml.node("hbox", [&] {
		xml.attribute("name", id++);
		xml.node("label", [&] {
			xml.attribute("font", "monospace/regular");
			xml.attribute("name", id++);
			xml.attribute("align", "right");
			xml.attribute("text", String<48>("throttle absolut ", align_string(abs), "s"));
		});
	});
}


void Power::_cpu_perf_status(Xml_generator &xml, Xml_node const &status, unsigned &)
{
	unsigned id = 0;

	xml.node("vbox", [&] {
		xml.attribute("name", id++);

		status.with_optional_sub_node("package", [&](auto const &node) {
			_cpu_perf_status_detail(xml, node, " Package perf status", id);
		});

		status.with_optional_sub_node("dram", [&](auto const &node) {
			_cpu_perf_status_detail(xml, node, " DRAM perf status", id);
		});

		status.with_optional_sub_node("pp0", [&](auto const &node) {
			_cpu_perf_status_detail(xml, node, " PP0 perf status", id);
		});
	});
}


void Power::_cpu_temp(Xml_generator &xml, Xml_node const &cpu)
{
	auto const temp_c = cpu.attribute_value("temp_c", 0U);
	auto const cpuid  = cpu_id(cpu);

	xml.node("hbox", [&] {
		xml.attribute("name", cpuid);

		xml.node("label", [&] {
			xml.attribute("name", cpuid);
			xml.attribute("align", "right");
			xml.attribute("text", String<12>(" ", temp_c, " °C |"));
		});
	});
}


void Power::_cpu_freq(Xml_generator &xml, Xml_node const &cpu)
{
	auto const freq_khz = cpu.attribute_value("freq_khz", 0ULL);
	auto const cpuid    = cpu_id(cpu);

	xml.node("hbox", [&] {
		xml.attribute("name", cpuid);

		xml.node("label", [&] {
			xml.attribute("name", cpuid);
			xml.attribute("align", "right");

			auto const rest = (freq_khz % 1000) / 10;
			xml.attribute("text", String<16>(" ", freq_khz / 1000, ".",
			                                 rest < 10 ? "0" : "", rest, " MHz"));
		});
	});
}


void Power::_cpu_setting(Xml_generator &xml, Xml_node const &cpu)
{
	auto const cpuid = cpu_id(cpu);

	xml.node("hbox", [&] {
		xml.attribute("name", cpuid);
		xml.node("button", [&] () {
			xml.attribute("name", "settings");
			xml.node("label", [&] () {
				xml.attribute("text", "");
			});

			if (_setting_hovered.value == cpuid)
				xml.attribute("hovered", true);
			if (_setting_cpu.value == cpuid)
				xml.attribute("selected", true);
		});
	});
}


void Power::_settings_mode(Xml_generator &xml)
{
	xml.node("frame", [&] () {
		xml.attribute("name", "frame_mode");

		xml.node("hbox", [&] () {
			xml.attribute("name", "mode");

			auto text = String<64>(" Settings:");

			xml.node("label", [&] () {
				xml.attribute("align", "left");
				xml.attribute("text", text);
			});

#if 1
			xml.node("button", [&] () {
				xml.attribute("align", "right");
				xml.attribute("name", "normal");
				xml.node("label", [&] () {
					xml.attribute("text", "normal");
				});

				if (_hover_normal)
					xml.attribute("hovered", true);
				if (_select_normal)
					xml.attribute("selected", true);
			});
#endif

			xml.node("button", [&] () {
				xml.attribute("align", "right");
				xml.attribute("name", "advanced");
				xml.node("label", [&] () {
					xml.attribute("text", "advanced");
				});

				if (_hover_advanced)
					xml.attribute("hovered", true);
				if (_select_advanced)
					xml.attribute("selected", true);
			});
		});
	});
}


void Power::_settings_period(Xml_generator &xml)
{
	xml.node("frame", [&] () {
		xml.attribute("name", "frame_period");

		xml.node("hbox", [&] () {
			xml.attribute("name", "period");

			auto text = String<64>(" Update period in ms:");

			xml.node("label", [&] () {
				xml.attribute("align", "left");
				xml.attribute("text", text);
			});

			hub(xml, _timer_period, "period");

			xml.node("label", [&] () {
				xml.attribute("name", "b");
				xml.attribute("align", "right");
				xml.attribute("text", "");
			});

			xml.node("button", [&] () {
				xml.attribute("align", "right");
				xml.attribute("name", "apply_period");
				xml.node("label", [&] () {
					xml.attribute("text", "apply");
				});

				if (_apply_period)
					xml.attribute("hovered", true);
				if (_apply_select_per)
					xml.attribute("selected", true);
			});
		});
	});
}


void Power::_settings_amd(Xml_generator &xml, Xml_node const &node,
                          bool const re_eval)
{
	unsigned min_value = node.attribute_value("ro_limit_cur", 0u);
	unsigned max_value = node.attribute_value("ro_max_value", 0u);
	unsigned cur_value = node.attribute_value("ro_status", 0u);

	_amd_pstate.set_min_max(min_value, max_value);

	if (re_eval)
		_amd_pstate.set(cur_value);

	xml.node("frame", [&] () {
		xml.attribute("name", "frame_pstate");

		xml.node("hbox", [&] () {
			xml.attribute("name", "pstate");

			auto text = String<64>("Hardware Performance-State: ");

			xml.node("label", [&] () {
				xml.attribute("name", "left");
				xml.attribute("align", "left");
				xml.attribute("text", text);
			});

			xml.node("button", [&] () {
				xml.attribute("name", "pstate-max");
				xml.node("label", [&] () {
					xml.attribute("text", "max");
				});
				if (_pstate_max)
					xml.attribute("hovered", true);
				if (_amd_pstate.value() == _amd_pstate.min())
					xml.attribute("selected", true);
			});

			xml.node("button", [&] () {
				xml.attribute("name", "pstate-mid");
				xml.node("label", [&] () {
					xml.attribute("text", "mid");
				});
				if (_pstate_mid)
					xml.attribute("hovered", true);
				if (_amd_pstate.value() == (_amd_pstate.max() - _amd_pstate.min() + 1) / 2)
					xml.attribute("selected", true);
			});

			xml.node("button", [&] () {
				xml.attribute("name", "pstate-min");
				xml.node("label", [&] () {
					xml.attribute("text", "min");
				});
				if (_pstate_min)
					xml.attribute("hovered", true);
				if (_amd_pstate.value() == _amd_pstate.max())
					xml.attribute("selected", true);
			});

			if (_select_advanced) {
				if (_pstate_custom_sel) {
					auto text = String<64>(" range max-min [", min_value, "-",
					                       max_value, "] current=", cur_value);

					xml.node("label", [&] () {
						xml.attribute("name", "right");
						xml.attribute("align", "right");
						xml.attribute("text", text);
					});

					hub(xml, _amd_pstate, "pstate");
				}

				xml.node("button", [&] () {
					xml.attribute("name", "pstate-custom");
					xml.node("label", [&] () {
						xml.attribute("text", "custom");
					});
					if (_pstate_custom)
						xml.attribute("hovered", true);
					if (_pstate_custom_sel)
						xml.attribute("selected", true);
				});
			}
		});
	});
}


void Power::_settings_intel_epb(Xml_generator  &xml,
                                Xml_node const &node,
                                bool     const  re_read)
{
	unsigned const epb = node.attribute_value("raw", 0);

	xml.node("frame", [&] () {
		xml.attribute("name", "frame_speed_step");

		xml.node("hbox", [&] () {
			xml.attribute("name", "epb");

			auto text = String<64>(" Energy Performance Bias hint: ");

			xml.node("label", [&] () {
				xml.attribute("name", "left");
				xml.attribute("align", "left");
				xml.attribute("text", text);
			});

			if (re_read)
				_intel_epb.set(epb);

			xml.node("button", [&] () {
				xml.attribute("name", "epb-perf");
				xml.node("label", [&] () {
					xml.attribute("text", "performance");
				});
				if (_epb_perf)
					xml.attribute("hovered", true);
				if (_intel_epb.value() == EPB_PERF)
					xml.attribute("selected", true);
			});

			xml.node("button", [&] () {
				xml.attribute("name", "epb-bala");
				xml.node("label", [&] () {
					xml.attribute("text", "balanced");
				});
				if (_epb_bala)
					xml.attribute("hovered", true);
				if (_intel_epb.value() == EPB_BALANCED ||
				    _intel_epb.value() == EPB_BALANCED - 1)
					xml.attribute("selected", true);
			});

			xml.node("button", [&] () {
				xml.attribute("name", "epb-ener");
				xml.node("label", [&] () {
					xml.attribute("text", "energy");
				});
				if (_epb_ener)
					xml.attribute("hovered", true);
				if (_intel_epb.value() == EPB_POWER_SAVE)
					xml.attribute("selected", true);
			});

			if (!_select_advanced)
				return;

			bool const extra_info = _epb_custom_select;

			if (extra_info) {
				auto text = String<64>(" range [", _intel_epb.min(), "-",
				                       _intel_epb.max(), "] current=", epb);

				xml.node("label", [&] () {
					xml.attribute("name", "right");
					xml.attribute("align", "right");
					xml.attribute("text", text);
				});

				hub(xml, _intel_epb, "epb");
			}

			xml.node("button", [&] () {
				xml.attribute("align", "right");
				xml.attribute("name", "epb-custom");
				xml.node("label", [&] () {
					xml.attribute("text", "custom");
				});
				if (_epb_custom)
					xml.attribute("hovered", true);
				if (extra_info || ((_intel_epb.value() != EPB_PERF) &&
				                   (_intel_epb.value() != EPB_POWER_SAVE) &&
				                   (_intel_epb.value() != EPB_BALANCED)))
					xml.attribute("selected", true);
			});

		});
	});
}


void Power::_settings_intel_hwp(Xml_generator &xml, Xml_node const &node, bool)
{
	bool enabled = node.attribute_value("enable", false);

	xml.node("frame", [&] () {
		xml.attribute("name", "frame_hwp");

		xml.node("hbox", [&] () {
			xml.attribute("name", "hwp");

			auto text = String<72>(" Intel HWP state: ",
			                       enabled ? "on" : "off",
			                       " - Once enabled stays until reset (Intel spec)");
			xml.node("label", [&] () {
				xml.attribute("align", "left");
				xml.attribute("text", text);
			});

			if (enabled)
				return;

			xml.node("button", [&] () {
				xml.attribute("name", "hwp_on");
				xml.node("label", [&] () {
					xml.attribute("text", "on");
				});

				if (_hwp_on_hovered)
					xml.attribute("hovered", true);
				if (_hwp_on_selected)
					xml.attribute("selected", true);
			});
		});
	});

	if (enabled && !_hwp_enabled_once)
		_hwp_enabled_once = true;
}


struct Hwp_request : Genode::Register<64> {
	struct Perf_min : Bitfield< 0, 8> { };
	struct Perf_max : Bitfield< 8, 8> { };
	struct Perf_des : Bitfield<16, 8> { };
	struct Perf_epp : Bitfield<24, 8> { };

	struct Activity_wnd  : Bitfield<32,10> { };
	struct Pkg_ctrl      : Bitfield<42, 1> { };
	struct Act_wnd_valid : Bitfield<59, 1> { };
	struct Epp_valid     : Bitfield<60, 1> { };
	struct Desired_valid : Bitfield<61, 1> { };
	struct Max_valid     : Bitfield<62, 1> { };
	struct Min_valid     : Bitfield<63, 1> { };
};


void Power::_settings_intel_hwp_req(Xml_generator &xml,
                                    Xml_node const &node,
                                    unsigned const hwp_low,
                                    unsigned const hwp_high,
                                    uint64_t const hwp_req_pkg,
                                    bool     const hwp_req_pkg_valid,
                                    bool     const re_read,
                                    unsigned     & frames_count)
{
	auto const hwp_req = node.attribute_value("raw", 0ull);

	auto const hwp_min = unsigned(Hwp_request::Perf_min::get(hwp_req));
	auto const hwp_max = unsigned(Hwp_request::Perf_max::get(hwp_req));
	auto const hwp_des = unsigned(Hwp_request::Perf_des::get(hwp_req));
	auto const hwp_epp = unsigned(Hwp_request::Perf_epp::get(hwp_req));
	auto const act_wnd = unsigned(Hwp_request::Activity_wnd::get(hwp_req));

	auto const hwp_pkg_min = unsigned(Hwp_request::Perf_min::get(hwp_req_pkg));
	auto const hwp_pkg_max = unsigned(Hwp_request::Perf_max::get(hwp_req_pkg));
	auto const hwp_pkg_des = unsigned(Hwp_request::Perf_des::get(hwp_req_pkg));

	if (re_read)
	{
		_intel_hwp_min.set_min_max(hwp_low, hwp_high);
		_intel_hwp_max.set_min_max(hwp_low, hwp_high);
		_intel_hwp_des.set_min_max(hwp_low, hwp_high);

		/* read out features sometimes are not within hw range .oO */
		if (hwp_low <= hwp_min && hwp_min <= hwp_high)
			_intel_hwp_min.set(hwp_min);
		if (hwp_low <= hwp_max && hwp_max <= hwp_high)
			_intel_hwp_max.set(hwp_max);
		if (hwp_des <= hwp_high) {
			_intel_hwp_des.set(hwp_des);

			_hwp_req_auto_sel = hwp_des == 0;
		}

		_intel_hwp_epp.set(hwp_epp);

		_intel_hwp_pck_min.set_min_max(hwp_low, hwp_high);
		_intel_hwp_pck_max.set_min_max(hwp_low, hwp_high);
		_intel_hwp_pck_des.set_min_max(hwp_low, hwp_high);
	}

	if (_select_advanced) {
		frames_count ++;

		xml.node("frame", [&] () {
			xml.attribute("name", "frame_hwpreq");

			xml.node("hbox", [&] () {
				xml.attribute("name", "hwpreq");

				auto text = String<72>(" HWP CPU: [", hwp_min, "-", hwp_max, "] desired=",
				                       hwp_des, (hwp_des == 0) ? " (AUTO)" : "",
				                       (hwp_req >> 32) ? " IMPLEMENT ME:" : "");

				/* only relevant if HWP_REQ_PACKAGE is supported */
				if (Hwp_request::Pkg_ctrl::get(hwp_req))
					text = String<72>(text, "P");
				if (Hwp_request::Act_wnd_valid::get(hwp_req))
					text = String<72>(text, "A");
				if (Hwp_request::Epp_valid::get(hwp_req))
					text = String<72>(text, "E");
				if (Hwp_request::Desired_valid::get(hwp_req))
					text = String<72>(text, "D");
				if (Hwp_request::Max_valid::get(hwp_req))
					text = String<72>(text, "X");
				if (Hwp_request::Min_valid::get(hwp_req))
					text = String<72>(text, "N");

				xml.node("label", [&] () {
					xml.attribute("align", "left");
					xml.attribute("name", 1);
					xml.attribute("text", text);
				});

				if (_hwp_req_cus_sel) {
					xml.node("label", [&] () {
						xml.attribute("align", "right");
						xml.attribute("name", 2);
						xml.attribute("text", String<16>(" min:"));
					});
					hub(xml, _intel_hwp_min, "hwp_min");

					xml.node("label", [&] () {
						xml.attribute("align", "right");
						xml.attribute("name", 3);
						xml.attribute("text", String<16>(" max:"));
					});
					hub(xml, _intel_hwp_max, "hwp_max");

					xml.node("label", [&] () {
						xml.attribute("align", "right");
						xml.attribute("name", 4);
						xml.attribute("text", String<16>(" desired:"));
					});

					/* if auto on, hide button for individual values */
					if (!_hwp_req_auto_sel) {
						hub(xml, _intel_hwp_des, "hwp_des");
					}

					xml.node("button", [&] () {
						xml.attribute("name", "hwp_req-auto");
						xml.node("label", [&] () {
							xml.attribute("text", "auto");
						});
						if (_hwp_req_auto)
							xml.attribute("hovered", true);
						if (_hwp_req_auto_sel)
							xml.attribute("selected", true);
					});
				}

				xml.node("button", [&] () {
				xml.attribute("align", "right");
					xml.attribute("name", "hwp_req-custom");
					xml.node("label", [&] () {
						xml.attribute("text", "custom");
					});
					if (_hwp_req_custom)
						xml.attribute("hovered", true);

					if (_hwp_req_cus_sel)
						xml.attribute("selected", true);
				});

			});
		});

		/* just show the values if hwp req package is available XXX */
		/* re-use code from  "hwp request" XXX */
		if (hwp_req_pkg_valid) {
			frames_count ++;
			xml.node("frame", [&] () {
				xml.attribute("name", "frame_hwpreq_pck");

				xml.node("hbox", [&] () {
					xml.attribute("name", "hwpreq_pck");

					auto text = String<72>(" Package: [", hwp_pkg_min, "-", hwp_pkg_max,
					                       "] desired=", hwp_pkg_des,
					                       (hwp_pkg_des == 0) ? " (AUTO)" : "");
					xml.node("label", [&] () {
						xml.attribute("align", "left");
						xml.attribute("name", 1);
						xml.attribute("text", text);
					});

					if (_hwp_req_cus_sel) {
						xml.node("label", [&] () {
							xml.attribute("align", "right");
							xml.attribute("name", 2);
							xml.attribute("text", String<16>(" min:"));
						});
						hub(xml, _intel_hwp_pck_min, "hwp_pck_min");

						xml.node("label", [&] () {
							xml.attribute("align", "right");
							xml.attribute("name", 3);
							xml.attribute("text", String<16>(" max:"));
						});
						hub(xml, _intel_hwp_pck_max, "hwp_pck_max");

						xml.node("label", [&] () {
							xml.attribute("align", "right");
							xml.attribute("name", 4);
							xml.attribute("text", String<16>(" desired:"));
						});
						hub(xml, _intel_hwp_pck_des, "hwp_pck_des");
					}
				});
			});
		}
	}

	xml.node("frame", [&] () {
		frames_count ++;
		xml.attribute("name", "frame_hwpepp");

		xml.node("hbox", [&] () {
			xml.attribute("name", "hwpepp");

			xml.node("label", [&] () {
				xml.attribute("align", "left");
				xml.attribute("name", "a");
				xml.attribute("text", " Energy-Performance-Preference:");
			});

			xml.node("button", [&] () {
				xml.attribute("name", "hwp_epp-perf");
				xml.node("label", [&] () {
					xml.attribute("text", "performance");
				});
				if (_hwp_epp_perf)
					xml.attribute("hovered", true);
				if (_intel_hwp_epp.value() == EPP_PERF)
					xml.attribute("selected", true);
			});

			xml.node("button", [&] () {
				xml.attribute("name", "hwp_epp-bala");
				xml.node("label", [&] () {
					xml.attribute("text", "balanced");
				});
				if (_hwp_epp_bala)
					xml.attribute("hovered", true);
				if (_intel_hwp_epp.value() == EPP_BALANCED ||
				    _intel_hwp_epp.value() == EPP_BALANCED - 1)
					xml.attribute("selected", true);
			});

			xml.node("button", [&] () {
				xml.attribute("name", "hwp_epp-ener");
				xml.node("label", [&] () {
					xml.attribute("text", "energy");
				});
				if (_hwp_epp_ener)
					xml.attribute("hovered", true);
				if (_intel_hwp_epp.value() == EPP_ENERGY)
					xml.attribute("selected", true);
			});

			bool const extra_info = _epp_custom_select && _select_advanced;

			if (extra_info) {
				xml.node("vbox", [&] () {
					auto text = String<64>(" range [", _intel_hwp_epp.min(),
					                       "-", _intel_hwp_epp.max(),
					                       "] current=", hwp_epp);
					xml.node("label", [&] () {
						xml.attribute("align", "left");
						xml.attribute("name", "a");
						xml.attribute("text", text);
					});
					xml.node("label", [&] () {
						xml.attribute("align", "left");
						xml.attribute("name", "b");
						xml.attribute("text", " (EPP - Energy-Performance-Preference)");
					});
					xml.node("label", [&] () {
						xml.attribute("align", "left");
						xml.attribute("name", "c");
						xml.attribute("text", String<22>(" Activity window=", act_wnd));
					});
				});

				hub(xml, _intel_hwp_epp, "hwp_epp");
			}

			if (_select_advanced) {
				xml.node("button", [&] () {
					xml.attribute("align", "right");
					xml.attribute("name", "hwp_epp-custom");
					xml.node("label", [&] () {
						xml.attribute("text", "custom");
					});
					if (_hwp_epp_custom)
						xml.attribute("hovered", true);
					if (extra_info || ((_intel_hwp_epp.value() != EPP_PERF)     &&
					                   (_intel_hwp_epp.value() != EPP_BALANCED) &&
					                   (_intel_hwp_epp.value() != EPP_ENERGY)))
						xml.attribute("selected", true);
				});
			}

		});
	});
}


void Power::_settings_view(Xml_generator &xml, Xml_node const &cpu,
                           String<12> const &cpuid, unsigned const cpu_count,
                           bool re_eval)
{
	bool     hwp_extension = false;
	unsigned frames   = 1; /* none - apply - all apply frame */
	unsigned hwp_high = 0;
	unsigned hwp_low  = 0;
	uint64_t hwp_req_pkg = 0;
	bool     hwp_req_pkg_valid = false;

	xml.attribute("name", "settings");

	_settings_period(xml);
	frames ++;

	_settings_mode(xml);
	frames ++;

	cpu.for_each_sub_node([&](Genode::Xml_node const &node) {

		if (node.type() == "pstate") {
			frames ++;
			_settings_amd(xml, node, re_eval);
			return;
		}

		if (node.type() == "energy_perf_bias" && node.has_attribute("raw")) {
			frames ++;
			_settings_intel_epb(xml, node, re_eval);
			return;
		}

		if (node.type() == "hwp") {
			frames ++;
			_settings_intel_hwp(xml, node, re_eval);
			return;
		}

		if (node.type() == "hwp_cap") {

			hwp_extension = true;

			if (!_hwp_enabled_once)
				return;

			bool const extra_info = _select_advanced && _hwp_req_cus_sel;

			unsigned effi = node.attribute_value("effi" , 1);
			unsigned guar = node.attribute_value("guar" , 1);

			hwp_high = node.attribute_value("high" , 0);
			hwp_low  = node.attribute_value("low"  , 0);

			if (!_initial_hwp_cap) {
				re_eval = true;
				_initial_hwp_cap = true;
			}

			if (extra_info) {
				frames ++;

				xml.node("frame", [&] () {
					xml.attribute("name", "frame_hwpcap");

					xml.node("hbox", [&] () {
						xml.attribute("name", "hwpcap");

						xml.node("vbox", [&] () {
							auto text = String<72>(" Intel HWP features: [", hwp_low,
							                       "-", hwp_high, "] efficient=", effi,
							                       " guaranty=", guar, " desired=0 (AUTO)");

							xml.node("label", [&] () {
								xml.attribute("align", "left");
								xml.attribute("name", "a");
								xml.attribute("text", text);
							});
							xml.node("label", [&] () {
								xml.attribute("align", "left");
								xml.attribute("name", "b");
								xml.attribute("text", " performance & frequency range steering");
							});
						});
					});
				});
			}
			return;
		}

		if (node.type() == "hwp_request_package") {
			hwp_req_pkg_valid = true;
			hwp_req_pkg       = node.attribute_value("raw", 0ull);
		}

		if (node.type() == "hwp_request") {

			hwp_extension = true;

			if (!_hwp_enabled_once)
				return;

			_settings_intel_hwp_req(xml, node, hwp_low, hwp_high, hwp_req_pkg,
			                        hwp_req_pkg_valid, re_eval, frames);
			return;
		}
	});

	if (_hwp_on_selected && !hwp_extension) {
		xml.node("frame", [&] () {
			xml.attribute("name", "frame_missing_hwp");

			xml.node("hbox", [&] () {
				xml.attribute("name", "hwp_extension");

				auto text = String<72>(" Intel HWP features available but HWP is off (not applied yet?)");

				xml.node("label", [&] () {
					xml.attribute("align", "left");
					xml.attribute("name", "a");
					xml.attribute("text", text);
				});
			});
		});
	}

	cpu.with_optional_sub_node("energy", [&](Genode::Xml_node const &energy) {
		frames ++;
		xml.node("frame", [&] () {
			xml.attribute("name", "rafl");

			xml.node("hbox", [&] {
				xml.attribute("name", "energy");

				_cpu_energy(xml, energy, frames);
			});
		});
	});

	if (_select_rapl_detail) {
		cpu.with_optional_sub_node("power_info", [&](Genode::Xml_node const &info) {
			frames ++;
			xml.node("frame", [&] () {
				xml.attribute("name", "info");

				xml.node("hbox", [&] {
					xml.attribute("name", "info");

					_cpu_power_info(xml, info, frames);
				});
			});
		});

		cpu.with_optional_sub_node("power_limit", [&](Genode::Xml_node const &info) {
			frames ++;
			xml.node("frame", [&] () {
				xml.attribute("name", "limit");

				xml.node("hbox", [&] {
					xml.attribute("name", "limit");

					_cpu_power_limit(xml, info, frames);
				});
			});
		});
	}

/*
	<policy pp0="0x10" pp1="0x10"/>

	cpu.with_optional_sub_node("policy", [&](Genode::Xml_node const &info) {
		frames ++;
		xml.node("frame", [&] () {
			xml.attribute("name", "policy");

			xml.node("hbox", [&] {
				xml.attribute("name", "policy");

			});
		});
	});
*/

	cpu.with_optional_sub_node("perf_status", [&](Genode::Xml_node const &info) {
		frames ++;
		xml.node("frame", [&] () {
			xml.attribute("name", "perf");

			xml.node("hbox", [&] {
				xml.attribute("name", "perf");

				_cpu_perf_status(xml, info, frames);
			});
		});
	});

	for (unsigned i = 0; i < 1 + ((cpu_count > frames) ? cpu_count - frames : 0); i++) {
		xml.node("frame", [&] () {
			xml.attribute("style", "invisible");
			xml.attribute("name", String<15>("frame_space_", i));

			xml.node("hbox", [&] () {
				xml.attribute("name", "space");

				xml.node("label", [&] () {
					xml.attribute("align", "left");
					xml.attribute("text", "");
				});
			});
		});
	}

	xml.node("hbox", [&] () {
		xml.node("label", [&] () {
			xml.attribute("text", "Apply to:");
		});

		xml.node("button", [&] () {
			xml.attribute("name", "none");
			xml.node("label", [&] () {
				xml.attribute("text", "none");
			});

			if (_none_hovered)
				xml.attribute("hovered", true);
			if (!_apply_select && !_apply_all_select)
				xml.attribute("selected", true);
		});

		if (_select_advanced) {
			xml.node("button", [&] () {
				xml.attribute("name", "apply");
				xml.node("label", [&] () {
					xml.attribute("text", cpuid);
				});

				if (_apply_hovered)
					xml.attribute("hovered", true);
				if (_apply_select)
					xml.attribute("selected", true);
			});
		}

		xml.node("button", [&] () {
			xml.attribute("name", "applyall");
			xml.node("label", [&] () {
				xml.attribute("text", "all CPUs");
			});

			if (_apply_all_hovered)
				xml.attribute("hovered", true);
			if (_apply_all_select)
				xml.attribute("selected", true);
		});
	});
}

void Component::construct(Genode::Env &env) { static Power state(env); }
