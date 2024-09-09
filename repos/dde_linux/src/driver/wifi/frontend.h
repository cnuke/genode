 /*
 * \author Josef Soentgen
 * \date   2018-07-31
 *
 * This wifi driver front end uses the CTRL interface of the wpa_supplicant
 * via a Genode specific backend that uses two distinct memory buffers for
 * communication, one for the command results and one for events.
 */

/*
 * Copyright (C) 2018-2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _WIFI_FRONTEND_H_
#define _WIFI_FRONTEND_H_

/* Genode includes */
#include <libc/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/registry.h>
#include <base/sleep.h>
#include <os/reporter.h>
#include <timer_session/connection.h>
#include <util/attempt.h>
#include <util/interface.h>
#include <util/xml_node.h>

/* rep includes */
#include <wifi/ctrl.h>
#include <wifi/rfkill.h>
using Ctrl_msg_buffer = Msg_buffer;

/* local includes */
#include <util.h>

/* declare manually as it is a internal hack^Winterface */
extern void wifi_kick_socketcall();


namespace Wifi {
	struct Frontend;
}


/* keep ordered! */
static struct Recv_msg_table {
	char const *string;
	size_t len;
} recv_table[] = {
	{ "OK",                            2 },
	{ "FAIL",                          4 },
	{ "CTRL-EVENT-SCAN-RESULTS",      23 },
	{ "CTRL-EVENT-CONNECTED",         20 },
	{ "CTRL-EVENT-DISCONNECTED",      23 },
	{ "SME: Trying to authenticate",  27 },
	{ "CTRL-EVENT-NETWORK-NOT-FOUND", 28 },
};

enum Rmi {
	OK = 0,
	FAIL,
	SCAN_RESULTS,
	CONNECTED,
	DISCONNECTED,
	SME_AUTH,
	NOT_FOUND,
};


static inline bool check_recv_msg(char const *msg,
                                  Recv_msg_table const &entry) {
	return Genode::strcmp(entry.string, msg, entry.len) == 0; }


static bool cmd_successful(char const *msg) {
	return check_recv_msg(msg, recv_table[OK]); }


static bool cmd_fail(char const *msg) {
	return check_recv_msg(msg, recv_table[FAIL]); }


static bool results_available(char const *msg) {
	return check_recv_msg(msg, recv_table[SCAN_RESULTS]); }


static bool connecting_to_network(char const *msg) {
	return check_recv_msg(msg, recv_table[SME_AUTH]); }


static bool network_not_found(char const *msg) {
	return check_recv_msg(msg, recv_table[NOT_FOUND]); }


static bool scan_results(char const *msg) {
	return Genode::strcmp("bssid", msg, 5) == 0; }


using Cmd = Genode::String<sizeof(Msg_buffer::send)>;
static void ctrl_cmd(Ctrl_msg_buffer &msg, Cmd const &cmd)
{
	Genode::memset(msg.send, 0, sizeof(msg.send));
	Genode::memcpy(msg.send, cmd.string(), cmd.length());
	++msg.send_id;

	wpa_ctrl_set_fd();

	/*
	 * We might have to pull the socketcall task out of poll_all()
	 * because otherwise we might be late and wpa_supplicant has
	 * already removed all scan results due to BSS age settings.
	 */
	wifi_kick_socketcall();
}



/*
 * Central network data structure
 */
struct Accesspoint : Genode::Interface
{
	using Bssid = Genode::String<17+1>;
	using Freq  = Genode::String< 4+1>;
	using Prot  = Genode::String< 7+1>;
	using Ssid  = Genode::String<32+1>;
	using Pass  = Genode::String<63+1>;

	/*
	 * Accesspoint information fields used by the front end
	 */
	Bssid    bssid  { };
	Freq     freq   { };
	Prot     prot   { };
	Ssid     ssid   { };
	Pass     pass   { };
	unsigned signal { 0 };

	/*
	 * CTRL interface fields
	 *
	 * The 'enabled' field is set to true if ENABLE_NETWORK
	 * was successfully executed. The network itself might
	 * get disabled by wpa_supplicant itself in case it cannot
	 * connect to the network, which will _not_ be reflected
	 * here.
	 */
	int  id      { -1 };
	bool enabled { false };

	/*
	 * Internal configuration fields
	 */
	bool auto_connect  { false };
	bool update        { false };
	bool stale         { false };
	bool explicit_scan { false };

	/**
	 * Default constructor
	 */
	Accesspoint() { }

	/**
	 * Constructor that initializes information fields
	 */
	Accesspoint(char const *bssid, char const *freq,
	            char const *prot,  char const *ssid, unsigned signal)
	: bssid(bssid), freq(freq), prot(prot), ssid(ssid), signal(signal)
	{ }

	void print(Genode::Output &out) const
	{
		Genode::print(out, "Accesspoint:",
		                   " SSID: '", ssid, "'",
		                   " BSSID: '", bssid, "'",
		                   " protection: ", prot,
		                   " id: ", id,
		                   " quality: ", signal,
		                   " enabled: ", enabled,
		                   " update: ", update,
		                   " auto_connect: ", auto_connect,
		                   " stale: ", stale,
		                   " explicit_scan: ", explicit_scan);
	}

	void invalidate() { ssid = Ssid(); bssid = Bssid(); }

	bool ssid_valid()  const { return ssid.length() > 1 && ssid.length() <= 32 + 1; }
	bool bssid_valid() const { return bssid.length() == 17 + 1; }
	bool wpa()         const { return prot != "NONE"; }
	bool wpa3()        const { return prot == "WPA3"; }
	bool stored()      const { return id != -1; }

	bool pass_valid()  const { return pass.length() > 8 && pass.length() <= 63 + 1; }
};


template <typename FUNC>
static void for_each_line(char const *msg, FUNC const &func)
{
	char line_buffer[1024];
	size_t cur = 0;

	while (msg[cur] != 0) {
		size_t until = Util::next_char(msg, cur, '\n');
		Genode::memcpy(line_buffer, &msg[cur], until);
		line_buffer[until] = 0;
		cur += until + 1;

		func(line_buffer);
	}
}


template <typename FUNC>
static void for_each_result_line(char const *msg, FUNC const &func)
{
	char line_buffer[1024];
	size_t cur = 0;

	/* skip headline */
	size_t until = Util::next_char(msg, cur, '\n');
	cur += until + 1;

	while (msg[cur] != 0) {
		until = Util::next_char(msg, cur, '\n');
		Genode::memcpy(line_buffer, &msg[cur], until);
		line_buffer[until] = 0;
		cur += until + 1;

		char const *s[5] = { };

		for (size_t c = 0, i = 0; i < 5; i++) {
			size_t pos = Util::next_char(line_buffer, c, '\t');
			line_buffer[c+pos] = 0;
			s[i] = (char const*)&line_buffer[c];
			c += pos + 1;
		}

		bool const is_wpa1 = Util::string_contains((char const*)s[3], "WPA");
		bool const is_wpa2 = Util::string_contains((char const*)s[3], "WPA2");
		bool const is_wpa3 = Util::string_contains((char const*)s[3], "SAE");

		unsigned signal = Util::approximate_quality(s[2]);

		char const *prot = is_wpa1 ? "WPA" : "NONE";
		            prot = is_wpa2 ? "WPA2" : prot;
		            prot = is_wpa3 ? "WPA3" : prot;

		Accesspoint ap(s[0], s[1], prot, s[4], signal);

		func(ap);
	}
}


struct Action : Genode::Fifo<Action>::Element
{
	enum class Type    : unsigned { COMMAND, QUERY };
	enum class Command : unsigned {
		INVALID, ADD, DISABLE, ENABLE, EXPLICIT_SCAN,
		REMOVE, SCAN, SCAN_RESULTS, SET, UPDATE, };
	enum class Query   : unsigned {
		INVALID, BSS, RSSI, STATUS, };

	Type    const type;
	Command const command;
	Query   const query;

	bool successful;

	Action(Command cmd)
	:
		type       { Type::COMMAND },
		command    { cmd },
		query      { Query::INVALID },
		successful { true }
	{ }

	Action(Query query)
	:
		type       { Type::QUERY },
		command    { Command::INVALID },
		query      { query },
		successful { true }
	{ }

	bool valid_command() const {
		return type == Type::COMMAND && command != Command::INVALID; }
	bool valid_query() const {
		return type == Type::QUERY && query != Query::INVALID; }

	virtual void execute() { }

	virtual void check(char const *) { }

	virtual void response(char const *, Accesspoint &) { }

	virtual bool complete() const = 0;

	virtual void print(Genode::Output &) const = 0;
};


/*
 * Action for adding a new network
 *
 * In case the 'auto_connect' option is set for the network it
 * will also be enabled to active auto-joining.
 */
struct Add_network_cmd : Action
{
	enum class State : unsigned {
		INIT, ADD_NETWORK, FILL_NETWORK_SSID, FILL_NETWORK_BSSID,
		FILL_NETWORK_KEY_MGMT, SET_NETWORK_PMF, FILL_NETWORK_PSK,
		ENABLE_NETWORK, COMPLETE
	};

	Ctrl_msg_buffer &_msg;
	Accesspoint      _accesspoint;
	State            _state;

	Add_network_cmd(Ctrl_msg_buffer &msg, Accesspoint const &ap)
	:
		Action       { Command::ADD },
		_msg         { msg },
		_accesspoint { ap },
		_state       { State::INIT }
	{ }

	void print(Genode::Output &out) const override
	{
		Genode::print(out, "Add_network_cmd: ", (unsigned)_state);
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("ADD_NETWORK"));
			_state = State::ADD_NETWORK;
			break;
		case State::ADD_NETWORK:
			ctrl_cmd(_msg, Cmd("SET_NETWORK ", _accesspoint.id,
			                       " ssid \"", _accesspoint.ssid, "\""));
			_state = State::FILL_NETWORK_SSID;
			break;
		case State::FILL_NETWORK_SSID:
		{
			bool const valid = _accesspoint.bssid_valid();
			char const *bssid = valid ? _accesspoint.bssid.string() : "";

			ctrl_cmd(_msg, Cmd("SET_NETWORK ", _accesspoint.id,
			                       " bssid ", bssid));
			_state = State::FILL_NETWORK_BSSID;
			break;
		}
		case State::FILL_NETWORK_BSSID:
			if (_accesspoint.wpa3()) {
				ctrl_cmd(_msg, Cmd("SET_NETWORK ", _accesspoint.id,
				                       " key_mgmt SAE"));
				_state = State::FILL_NETWORK_KEY_MGMT;
			} else {
				if (_accesspoint.wpa())
					ctrl_cmd(_msg, Cmd("SET_NETWORK ", _accesspoint.id,
					                       " psk \"", _accesspoint.pass, "\""));
				else
					ctrl_cmd(_msg, Cmd("SET_NETWORK ", _accesspoint.id,
					                       " key_mgmt NONE"));
				_state = State::FILL_NETWORK_PSK;
			}
			break;
		case State::FILL_NETWORK_KEY_MGMT:
			ctrl_cmd(_msg, Cmd("SET_NETWORK ", _accesspoint.id,
			         " ieee80211w 2"));
			_state = State::SET_NETWORK_PMF;
			break;
		case State::SET_NETWORK_PMF:
			ctrl_cmd(_msg, Cmd("SET_NETWORK ", _accesspoint.id,
			                       " psk \"", _accesspoint.pass, "\""));
			_state = State::FILL_NETWORK_PSK;
			break;
		case State::FILL_NETWORK_PSK:
			if (_accesspoint.auto_connect) {
				ctrl_cmd(_msg, Cmd("ENABLE_NETWORK ", _accesspoint.id));
				_state = State::ENABLE_NETWORK;
			} else
				_state = State::COMPLETE;
			break;
		case State::ENABLE_NETWORK:
			_state = State::COMPLETE;
			break;
		case State::COMPLETE:
			break;
		}
	}

	void check(char const *msg) override
	{
		using namespace Genode;

		bool complete = false;

		/*
		 * Handle response by expected failure handling
		 * and use fallthrough switch cases to reduce code.
		 */
		switch (_state) {
		case State::INIT: break;

		case State::ADD_NETWORK:
			if (cmd_fail(msg)) {
				error("ADD_NETWORK(", (unsigned)_state, ") failed: ", msg);
				Action::successful = false;
				complete = true;
			}
			break;

		case State::FILL_NETWORK_SSID:     [[fallthrough]];
		case State::FILL_NETWORK_BSSID:    [[fallthrough]];
		case State::FILL_NETWORK_KEY_MGMT: [[fallthrough]];
		case State::SET_NETWORK_PMF:       [[fallthrough]];
		case State::FILL_NETWORK_PSK:      [[fallthrough]];
		case State::ENABLE_NETWORK:
			if (!cmd_successful(msg)) {
				error("ADD_NETWORK(", (unsigned)_state, ") failed: ", msg);
				Action::successful = false;
				complete = true;
			}
			break;
		case State::COMPLETE: break;
		}

		if (complete) {
			_state = State::COMPLETE;
			return;
		}

		switch (_state) {
		case State::INIT: break;
		case State::ADD_NETWORK:
		{
			long id = -1;
			Genode::ascii_to(msg, id);
			_accesspoint.id = static_cast<int>(id);
			break;
		}
		case State::FILL_NETWORK_SSID:     break;
		case State::FILL_NETWORK_BSSID:    break;
		case State::FILL_NETWORK_KEY_MGMT: break;
		case State::SET_NETWORK_PMF:       break;
		case State::FILL_NETWORK_PSK:      break;
		case State::ENABLE_NETWORK:        break;
		case State::COMPLETE:              break;
		}
	}

	Accesspoint const &accesspoint() const
	{
		return _accesspoint;
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for removing a network
 */
struct Remove_network_cmd : Action
{
	enum class State : unsigned {
		INIT, REMOVE_NETWORK, COMPLETE
	};

	Ctrl_msg_buffer &_msg;
	int              _id;
	State            _state;

	Remove_network_cmd(Ctrl_msg_buffer &msg, int id)
	:
		Action      { Command::REMOVE },
		_msg        { msg },
		_id         { id },
		_state      { State::INIT }
	{ }

	void print(Genode::Output &out) const override
	{
		Genode::print(out, "Remove_network_cmd: ", (unsigned)_state);
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("REMOVE_NETWORK ", _id));
			_state = State::REMOVE_NETWORK;
			break;
		case State::REMOVE_NETWORK:
			_state = State::COMPLETE;
			break;
		case State::COMPLETE:
			break;
		}
	}

	void check(char const *msg) override
	{
		using namespace Genode;

		bool complete = false;

		switch (_state) {
		case State::INIT: break;
		case State::REMOVE_NETWORK:
			if (cmd_fail(msg)) {
				error("could not remove network: ", msg);
				Action::successful = false;
				complete = true;
			}
			break;
		case State::COMPLETE: break;
		}

		if (complete)
			_state = State::COMPLETE;
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for enabling a network
 */
struct Enable_network_cmd : Action
{
	enum class State : unsigned {
		INIT, ENABLE_NETWORK, COMPLETE
	};
	Ctrl_msg_buffer &_msg;
	int              _id;
	State            _state;

	Enable_network_cmd(Ctrl_msg_buffer &msg, int id)
	:
		Action      { Command::ENABLE },
		_msg        { msg },
		_id         { id },
		_state      { State::INIT }
	{ }

	void print(Genode::Output &out) const override
	{
		Genode::print(out, "Enable_network_cmd: ", (unsigned)_state);
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("ENABLE_NETWORK ", _id));
			_state = State::ENABLE_NETWORK;
			break;
		case State::ENABLE_NETWORK:
			_state = State::COMPLETE;
			break;
		case State::COMPLETE:
			break;
		}
	}

	void check(char const *msg) override
	{
		using namespace Genode;

		bool complete = false;

		switch (_state) {
		case State::INIT: break;
		case State::ENABLE_NETWORK:
			if (cmd_fail(msg)) {
				error("could not enable network: ", msg);
				Action::successful = false;
				complete = true;
			}
			break;
		case State::COMPLETE: break;
		}

		if (complete)
			_state = State::COMPLETE;
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for disabling a network
 */
struct Disable_network_cmd : Action
{
	enum class State : unsigned {
		INIT, DISABLE_NETWORK, COMPLETE
	};
	Ctrl_msg_buffer &_msg;
	int              _id;
	State            _state;

	Disable_network_cmd(Ctrl_msg_buffer &msg, int id)
	:
		Action      { Command::DISABLE },
		_msg        { msg },
		_id         { id },
		_state      { State::INIT }
	{ }

	void print(Genode::Output &out) const override
	{
		Genode::print(out, "Disable_network_cmd: ", (unsigned)_state);
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("DISABLE_NETWORK ", _id));
			_state = State::DISABLE_NETWORK;
			break;
		case State::DISABLE_NETWORK:
			_state = State::COMPLETE;
			break;
		case State::COMPLETE:
			break;
		}
	}

	void check(char const *msg) override
	{
		using namespace Genode;

		bool complete = false;

		switch (_state) {
		case State::INIT: break;
		case State::DISABLE_NETWORK:
			if (cmd_fail(msg)) {
				error("could not disable network: ", msg);
				Action::successful = false;
				complete = true;
			}
			break;
		case State::COMPLETE: break;
		}

		if (complete)
			_state = State::COMPLETE;
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for updating a network
 *
 * For now only the PSK is updated and depending on the
 * auto_connect configuration the network will also be
 * enabled to allow for auto-join after the alteration.
 */
struct Update_network_cmd : Action
{
	enum class State : unsigned {
		INIT, UPDATE_NETWORK_PSK, DISABLE_NETWORK, ENABLE_NETWORK,
		COMPLETE
	};
	Ctrl_msg_buffer &_msg;
	Accesspoint      _accesspoint;
	State            _state;

	Update_network_cmd(Ctrl_msg_buffer &msg, Accesspoint const &ap)
	:
		Action       { Command::UPDATE },
		_msg         { msg },
		_accesspoint { ap },
		_state       { State::INIT }
	{ }

	void print(Genode::Output &out) const override
	{
		Genode::print(out, "Update_network_cmd: ", (unsigned)_state);
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("SET_NETWORK ", _accesspoint.id,
			                       " psk \"", _accesspoint.pass, "\""));
			_state = State::UPDATE_NETWORK_PSK;
			break;
		case State::UPDATE_NETWORK_PSK:
			ctrl_cmd(_msg, Cmd("DISABLE_NETWORK ", _accesspoint.id));
			_state = State::DISABLE_NETWORK;
			break;
		case State::DISABLE_NETWORK:
			if (_accesspoint.auto_connect) {
				ctrl_cmd(_msg, Cmd("ENABLE_NETWORK ", _accesspoint.id));
				_state = State::ENABLE_NETWORK;
			} else {
				_state = State::COMPLETE;
			}
			break;
		case State::ENABLE_NETWORK:
			_state  = State::COMPLETE;
		case State::COMPLETE:
			break;
		}
	}

	void check(char const *msg) override
	{
		using namespace Genode;

		bool complete = false;

		switch (_state) {
		case State::INIT: break;
		case State::UPDATE_NETWORK_PSK: [[fallthrough]];
		case State::ENABLE_NETWORK:     [[fallthrough]];
		case State::DISABLE_NETWORK:
			if (!cmd_successful(msg)) {
				error("UPDATE_NETWORK(", (unsigned)_state, ") failed: ", msg);
				Action::successful = false;
				complete = true;
			}
			break;
		case State::COMPLETE: break;
		}

		if (complete)
			_state = State::COMPLETE;
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for initiating a scan request
 */
struct Scan_cmd : Action
{
	enum class State : unsigned {
		INIT, SCAN, COMPLETE
	};
	Ctrl_msg_buffer &_msg;
	State            _state;

	Scan_cmd(Ctrl_msg_buffer &msg)
	:
		Action      { Command::SCAN },
		_msg        { msg },
		_state      { State::INIT }
	{ }

	void print(Genode::Output &out) const override
	{
		Genode::print(out, "Scan_cmd: ", (unsigned)_state);
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("SCAN"));
			_state = State::SCAN;
			break;
		case State::SCAN:
			_state = State::COMPLETE;
			break;
		case State::COMPLETE:
			break;
		}
	}

	void check(char const *msg) override
	{
		using namespace Genode;

		bool complete = false;

		switch (_state) {
		case State::INIT: break;
		case State::SCAN:
			if (!cmd_successful(msg)) {
				/* ignore busy fails silently */
				bool const scan_busy = strcmp(msg, "FAIL-BUSY");
				if (!scan_busy) {
					error("could not initiate scan: ", msg);
					Action::successful = false;
					complete = true;
				}
			}
			break;
		case State::COMPLETE: break;
		}

		if (complete)
			_state = State::COMPLETE;
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for initiating a explicit scan request
 */
struct Explicit_scan_cmd : Action
{
	enum class State : unsigned {
		INIT, FILL_SSID, SCAN, COMPLETE
	};
	Ctrl_msg_buffer &_msg;
	State            _state;

	/*
	 * The number of explicit networks is limited by the
	 * message buffer that is 4096 bytes larger. Thus its
	 * possible to store around 58 explicit SSID (64 + 6)
	 * request, which should be plenty - limit the buffer
	 * to that amount.
	 */
	char _ssid_buffer[4060] { };

	Explicit_scan_cmd(Ctrl_msg_buffer &msg)
	:
		Action      { Command::EXPLICIT_SCAN },
		_msg        { msg },
		_state      { State::INIT }
	{ }

	void print(Genode::Output &out) const override
	{
		Genode::print(out, "Explicit_scan_cmd: ", (unsigned)_state);
	}

	void with_ssid_buffer(auto const fn)
	{
		fn(_ssid_buffer, sizeof(_ssid_buffer));

		_state = State::FILL_SSID;
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			break;
		case State::FILL_SSID:
			ctrl_cmd(_msg, Cmd("SCAN", (char const *)_ssid_buffer));
			_state = State::SCAN;
			break;
		case State::SCAN:
			_state = State::COMPLETE;
			break;
		case State::COMPLETE:
			break;
		}
	}

	void check(char const *msg) override
	{
		using namespace Genode;

		bool complete = false;

		switch (_state) {
		case State::INIT:      break;
		case State::FILL_SSID: break;
		case State::SCAN:
			if (!cmd_successful(msg)) {
				/* ignore busy fails silently */
				bool const scan_busy = strcmp(msg, "FAIL-BUSY");
				if (!scan_busy) {
					error("could not initiate scan: ", msg);
					Action::successful = false;
					complete = true;
				}
			}
			break;
		case State::COMPLETE: break;
		}

		if (complete)
			_state = State::COMPLETE;
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for initiating a scan results request
 */
struct Scan_results_cmd : Action
{
	enum class State : unsigned {
		INIT, SCAN_RESULTS, COMPLETE
	};
	Ctrl_msg_buffer &_msg;
	State            _state;

	Genode::Expanding_reporter &_reporter;

	void _generate_report(char const *msg)
	{
		unsigned count_lines = 0;
		for_each_line(msg, [&] (char const*) { count_lines++; });

		if (!count_lines)
			return;

		try {
			_reporter.generate([&] (Genode::Xml_generator &xml) {

				for_each_result_line(msg, [&] (Accesspoint const &ap) {

					/* ignore potentially empty ssids */
					if (ap.ssid == "")
						return;

					xml.node("accesspoint", [&]() {
						xml.attribute("ssid",    ap.ssid);
						xml.attribute("bssid",   ap.bssid);
						xml.attribute("freq",    ap.freq);
						xml.attribute("quality", ap.signal);
						if (ap.wpa()) { xml.attribute("protection", ap.prot); }
					});
				});
			});

		} catch (...) { /* silently omit report */ }
	}

	Scan_results_cmd(Ctrl_msg_buffer &msg,
	                 Genode::Expanding_reporter &reporter)
	:
		Action      { Command::SCAN_RESULTS },
		_msg        { msg },
		_state      { State::INIT },
		_reporter   { reporter }
	{ }

	void print(Genode::Output &out) const override
	{
		Genode::print(out, "Scan_results_cmd: ", (unsigned)_state);
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("SCAN_RESULTS"));
			_state = State::SCAN_RESULTS;
			break;
		case State::SCAN_RESULTS:
			_state = State::COMPLETE;
			break;
		case State::COMPLETE:
			break;
		}
	}

	void check(char const *msg) override
	{
		using namespace Genode;

		bool complete = false;

		switch (_state) {
		case State::INIT: break;
		case State::SCAN_RESULTS:
			if (scan_results(msg))
				_generate_report(msg);
			break;
		case State::COMPLETE: break;
		}

		if (complete)
			_state = State::COMPLETE;
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for setting a configuration variable
 */
struct Set_cmd : Action
{
	using Key   = Genode::String<64>;
	using Value = Genode::String<128>;

	enum class State : unsigned {
		INIT, SET, COMPLETE
	};
	Ctrl_msg_buffer &_msg;
	State            _state;

	Key   _key;
	Value _value;

	Set_cmd(Ctrl_msg_buffer &msg, Key key, Value value)
	:
		Action      { Command::SET },
		_msg        { msg },
		_state      { State::INIT },
		_key        { key },
		_value      { value }
	{ }

	void print(Genode::Output &out) const override
	{
		Genode::print(out, "Set_cmd: ", (unsigned)_state);
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("SET ", _key, " ", _value));
			_state = State::SET;
			break;
		case State::SET:
			_state = State::COMPLETE;
			break;
		case State::COMPLETE:
			break;
		}
	}

	void check(char const *msg) override
	{
		using namespace Genode;

		bool complete = false;

		switch (_state) {
		case State::INIT: break;
		case State::SET:
			if (!cmd_successful(msg)) {
				error("could not set '", _key, "' to '", _value, "'");
				Action::successful = false;
				complete = true;
			}
			break;
		case State::COMPLETE: break;
		}

		if (complete)
			_state = State::COMPLETE;
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for querying BSS information
 */
struct Bss_query : Action
{
	enum class State : unsigned {
		INIT, BSS, COMPLETE
	};
	Ctrl_msg_buffer    &_msg;
	Accesspoint::Bssid  _bssid;
	State               _state;

	Bss_query(Ctrl_msg_buffer &msg, Accesspoint::Bssid bssid)
	:
		Action      { Query::BSS },
		_msg        { msg },
		_bssid      { bssid },
		_state      { State::INIT }
	{ }

	void print(Genode::Output &out) const override
	{
		Genode::print(out, "Bss_query: ", (unsigned)_state);
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("BSS ", _bssid));
			_state = State::BSS;
			break;
		case State::BSS:      break;
		case State::COMPLETE: break;
		}
	}

	void response(char const *msg, Accesspoint &ap) override
	{
		if (_state != State::BSS)
			return;

		_state = State::COMPLETE;

		/*
		 * It might happen that the supplicant already flushed
		 * its internal BSS information and cannot help us out.
		 * Since we already sent out a rudimentary report, just
		 * stop here.
		 */
		if (0 == msg[0])
			return;

		auto fill_ap = [&] (char const *line) {
			if (Genode::strcmp(line, "ssid=", 5) == 0) {
				ap.ssid = Accesspoint::Ssid(line+5);
			} else

			if (Genode::strcmp(line, "bssid=", 6) == 0) {
				ap.bssid = Accesspoint::Bssid(line+6);
			} else

			if (Genode::strcmp(line, "freq=", 5) == 0) {
				ap.freq = Accesspoint::Freq(line+5);
			}
		};
		for_each_line(msg, fill_ap);
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for querying RSSI information
 */
struct Rssi_query : Action
{
	enum class State : unsigned {
		INIT, RSSI, COMPLETE
	};
	Ctrl_msg_buffer &_msg;
	State            _state;

	Rssi_query(Ctrl_msg_buffer &msg)
	:
		Action      { Query::RSSI },
		_msg        { msg },
		_state      { State::INIT }
	{ }

	void print(Genode::Output &out) const override
	{
		Genode::print(out, "Rssi_query: ", (unsigned)_state);
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("SIGNAL_POLL"));
			_state = State::RSSI;
			break;
		case State::RSSI:     break;
		case State::COMPLETE: break;
		}
	}

	void response(char const *msg, Accesspoint &ap) override
	{
		if (_state != State::RSSI)
			return;

		_state = State::COMPLETE;

		using Rssi = Genode::String<5>;
		Rssi rssi { };
		auto get_rssi = [&] (char const *line) {
			if (Genode::strcmp(line, "RSSI=", 5) != 0)
				return;

			rssi = Rssi(line + 5);
		};
		for_each_line(msg, get_rssi);

		/*
		 * Use the same simplified approximation for denoting
		 * the quality to be in line with the scan results.
		 */
		ap.signal = Util::approximate_quality(rssi.valid() ? rssi.string()
		                                                   : "-100");
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Action for querying the current connection status
 */
struct Status_query : Action
{
	enum class State : unsigned {
		INIT, STATUS, COMPLETE
	};
	Ctrl_msg_buffer &_msg;
	State            _state;

	Status_query(Ctrl_msg_buffer &msg)
	:
		Action      { Query::STATUS },
		_msg        { msg },
		_state      { State::INIT }
	{ }

	void print(Genode::Output &out) const override
	{
		Genode::print(out, "Status_query: ", (unsigned)_state);
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("STATUS"));
			_state = State::STATUS;
			break;
		case State::STATUS:   break;
		case State::COMPLETE: break;
		}
	}

	void response(char const *msg, Accesspoint &ap) override
	{
		if (_state != State::STATUS)
			return;

		_state = State::COMPLETE;

		/*
		 * It might happen that the supplicant already flushed
		 * its internal BSS information and cannot help us out.
		 * Since we already sent out a rudimentary report, just
		 * stop here.
		 */
		if (0 == msg[0])
			return;

		auto fill_ap = [&] (char const *line) {
			if (Genode::strcmp(line, "ssid=", 5) == 0) {
				ap.ssid = Accesspoint::Ssid(line+5);
			} else

			if (Genode::strcmp(line, "bssid=", 6) == 0) {
				ap.bssid = Accesspoint::Bssid(line+6);
			} else

			if (Genode::strcmp(line, "freq=", 5) == 0) {
				ap.freq = Accesspoint::Freq(line+5);
			}
		};
		for_each_line(msg, fill_ap);
	}

	bool complete() const override {
		return _state == State::COMPLETE; }
};


/*
 * Wifi driver front end
 */
struct Wifi::Frontend : Wifi::Rfkill_notification_handler
{
	Frontend(const Frontend&) = delete;
	Frontend& operator=(const Frontend&) = delete;

	/* accesspoint */

	Genode::Heap _ap_allocator;

	using Accesspoint_r = Genode::Registered<Accesspoint>;

	Genode::Registry<Accesspoint_r> _aps { };

	Accesspoint *_lookup_ap_by_ssid(Accesspoint::Ssid const &ssid)
	{
		Accesspoint *p = nullptr;
		_aps.for_each([&] (Accesspoint &ap) {
			if (ap.ssid_valid() && ap.ssid == ssid) { p = &ap; }
		});
		return p;
	}

	void _with_accesspoint(Accesspoint::Ssid const &ssid,
	                       auto const found_fn,
	                       auto const err_fn)
	{
		Accesspoint *p = nullptr;
		_aps.for_each([&] (Accesspoint &ap) {
			if (ap.ssid_valid() && ap.ssid == ssid)
				p = &ap;
		});

		if (!p)
			try {
				p = new (&_ap_allocator) Accesspoint_r(_aps);
			} catch (...) {
				err_fn();
				return;
			}

		found_fn(*p);
	}

	void _free_ap(Accesspoint &ap)
	{
		Genode::destroy(&_ap_allocator, &ap);
	}

	template <typename FUNC>
	void _for_each_ap(FUNC const &func)
	{
		_aps.for_each([&] (Accesspoint &ap) {
			func(ap);
		});
	}

	unsigned _count_to_be_enabled()
	{
		unsigned count = 0;
		auto enable = [&](Accesspoint const &ap) {
			count += ap.auto_connect;
		};
		_for_each_ap(enable);
		return count;
	}

	unsigned _count_enabled()
	{
		unsigned count = 0;
		auto enabled = [&](Accesspoint const &ap) {
			count += ap.enabled;
		};
		_for_each_ap(enabled);
		return count;
	}

	/*
	 * Action queue handling
	 */

	Genode::Heap _action_alloctor;
	Genode::Fifo<Action> _actions { };

	Action *_pending_action { nullptr };

	void _with_pending_action(auto const &fn)
	{
		if (!_pending_action)
			_actions.dequeue([&] (Action &action) {
				_pending_action = &action; });

		bool const complete = _pending_action ? fn(*_pending_action)
		                                      : false;
		if (complete) {
			Genode::destroy(_action_alloctor, _pending_action);
			_pending_action = nullptr;
		}
	}

	void _dispatch_action_if_needed()
	{
		if (_pending_action)
			return;

		/*
		 * Grab the next action and call execute()
		 * to poke the CTRL interface.
		 */

		_actions.dequeue([&] (Action &action) {
			_pending_action = &action;
			_pending_action->execute();
		});
	}

	Msg_buffer &_msg;

	Genode::Blockade _notify_blockade { };

	void _notify_lock_lock()   { _notify_blockade.block(); }
	void _notify_lock_unlock() { _notify_blockade.wakeup(); }

	bool _rfkilled { false };

	Genode::Signal_handler<Wifi::Frontend> _rfkill_handler;

	void _handle_rfkill()
	{
		_rfkilled = Wifi::rfkill_blocked();

		/* re-enable scan timer */
		if (!_rfkilled)
			_try_arming_any_timer();
	}

	/* config */

	Genode::Attached_rom_dataspace         _config_rom;
	Genode::Signal_handler<Wifi::Frontend> _config_sigh;

	bool _single_autoconnect { false };

	struct Config
	{
		enum {
			DEFAULT_CONNECTED_SCAN_INTERVAL  = 30,
			DEFAULT_SCAN_INTERVAL            =  5,
			DEFAULT_UPDATE_QUAILITY_INTERVAL = 30,

			DEFAULT_VERBOSE       = false,
			DEFAULT_RFKILL        = false,
		};

		unsigned connected_scan_interval { DEFAULT_CONNECTED_SCAN_INTERVAL };
		unsigned scan_interval           { DEFAULT_SCAN_INTERVAL };
		unsigned update_quality_interval { DEFAULT_UPDATE_QUAILITY_INTERVAL };

		bool verbose { DEFAULT_VERBOSE };
		bool rfkill  { DEFAULT_RFKILL };

		bool intervals_changed(Config const &cfg) const
		{
			return connected_scan_interval != cfg.connected_scan_interval
			    || scan_interval           != cfg.scan_interval
			    || update_quality_interval != cfg.update_quality_interval;
		}

		bool rfkill_changed(Config const &cfg) const
		{
			return rfkill != cfg.rfkill;
		}

		static Config from_xml(Genode::Xml_node const &node)
		{
			bool const verbose       = node.attribute_value("verbose",
			                                                (bool)DEFAULT_VERBOSE);
			bool const rfkill        = node.attribute_value("rfkill",
			                                                (bool)DEFAULT_RFKILL);

			unsigned const connected_scan_interval =
				Util::check_time(node.attribute_value("connected_scan_interval",
				                 (unsigned)DEFAULT_CONNECTED_SCAN_INTERVAL),
				                 10, 15*60);

			unsigned const scan_interval =
				Util::check_time(node.attribute_value("scan_interval",
				                 (unsigned)DEFAULT_SCAN_INTERVAL),
				                 5, 15*60);

			unsigned const update_quality_interval =
				Util::check_time(node.attribute_value("update_quality_interval",
				                 (unsigned)DEFAULT_UPDATE_QUAILITY_INTERVAL),
				                 10, 15*60);

			Config new_config {
				.connected_scan_interval = connected_scan_interval,
				.scan_interval           = scan_interval,
				.update_quality_interval = update_quality_interval,
				.verbose                 = verbose,
				.rfkill                  = rfkill
			};
			return new_config;
		}
	};

	Config _config { };

	void _config_update(bool initial_config)
	{
		_config_rom.update();

		if (!_config_rom.valid()) { return; }

		Genode::Xml_node config_node = _config_rom.xml();

		Config const old_config = _config;

		_config = Config::from_xml(config_node);

		/*
		 * Arm again if intervals changed, implicitly discards
		 * an already scheduled timer.
		 */
		if (_config.intervals_changed(old_config) || initial_config)
			_try_arming_any_timer();

		if (_config.rfkill_changed(old_config) || initial_config) {
			Wifi::set_rfkill(_config.rfkill);

			/*
			 * In case we get blocked set rfkilled immediately to prevent
			 * any further scanning operation. The actual value will be set
			 * by the singal handler but is not expected to be any different
			 * as the rfkill call is not supposed to fail.
			 */
			if (_config.rfkill && !_rfkilled) {
				_rfkilled = true;

				Genode::Reporter::Xml_generator xml(*_state_reporter, [&] () {
					xml.node("accesspoint", [&] () {
						xml.attribute("state", "disconnected");
						xml.attribute("rfkilled", _rfkilled);
					});
				});

				_connected_ap.invalidate();
				_connecting = false;
			}
		}

		bool single_autoconnect = false;

		/* update AP list */
		auto parse = [&] ( Genode::Xml_node node) {

			Accesspoint ap;
			ap.ssid  = node.attribute_value("ssid",  Accesspoint::Ssid());
			ap.bssid = node.attribute_value("bssid", Accesspoint::Bssid());

			if (!ap.ssid_valid()) {
				Genode::warning("ignoring accesspoint with invalid ssid");
				return;
			}

			ap.pass          = node.attribute_value("passphrase", Accesspoint::Pass(""));
			ap.prot          = node.attribute_value("protection", Accesspoint::Prot("NONE"));
			ap.auto_connect  = node.attribute_value("auto_connect", true);
			ap.explicit_scan = node.attribute_value("explicit_scan", false);

			if (ap.wpa() && !ap.pass_valid()) {
					Genode::warning("ignoring accesspoint '", ap.ssid,
					                "' with invalid psk");
					return;
			}

			_with_accesspoint(ap.ssid, [&] (Accesspoint &p) {

				p.update = ((ap.bssid_valid() && ap.bssid != p.bssid)
				         || ap.pass  != p.pass
				         || ap.prot  != p.prot
				         || ap.auto_connect != p.auto_connect);

				if (ap.bssid_valid()) { p.bssid = ap.bssid; }

				p.ssid          = ap.ssid;
				p.prot          = ap.prot;
				p.pass          = ap.pass;
				p.auto_connect  = ap.auto_connect;
				p.explicit_scan = ap.explicit_scan;

				single_autoconnect |= (p.update || p.auto_connect) && !_connected_ap.ssid_valid();
			},
			[&] { Genode::error("could not add accesspoint"); });
		};
		config_node.for_each_sub_node("network", parse);

		/*
		 * To accomodate a management component that only deals
		 * with on network, e.g. the sculpt_manager, generate a
		 * fake connecting event. Either a connected or disconnected
		 * event will bring us to square one.
		 */
		if (!initial_config && _count_to_be_enabled() == 1 && single_autoconnect && !_rfkilled) {

			auto lookup = [&] (Accesspoint const &ap) {
				if (!ap.auto_connect) { return; }

				if (_config.verbose) { Genode::log("Single autoconnect event for '", ap.ssid, "'"); }

				try {
					Genode::Reporter::Xml_generator xml(*_state_reporter, [&] () {
						xml.node("accesspoint", [&] () {
							xml.attribute("ssid",  ap.ssid);
							xml.attribute("state", "connecting");
						});
					});

					_single_autoconnect = true;

				} catch (...) { }
			};
			_for_each_ap(lookup);
		}

		/*
		 * Marking removes stale APs first and triggers adding of
		 * new ones afterwards.
		 */
		_mark_stale_aps(config_node);

		_dispatch_action_if_needed();
	}

	void _handle_config_update() { _config_update(false); }

	/* state */

	Accesspoint _connected_ap { };

	/* scan */

	enum class Timer_type : uint8_t { CONNECTED_SCAN, SCAN, SIGNAL_POLL };

	unsigned _seconds_from_type(Timer_type const type)
	{
		switch (type) {
		case Timer_type::CONNECTED_SCAN: return _config.connected_scan_interval;
		case Timer_type::SCAN:           return _config.scan_interval;
		case Timer_type::SIGNAL_POLL:    return _config.update_quality_interval;
		}
		/* never reached */
		return 0;
	}

	static char const *_name_from_type(Timer_type const type)
	{
		switch (type) {
		case Timer_type::CONNECTED_SCAN: return "connected-scan";
		case Timer_type::SCAN:           return "scan";
		case Timer_type::SIGNAL_POLL:    return "signal-poll";
		}
		/* never reached */
		return nullptr;
	}

	Timer::Connection _timer;

	Timer::One_shot_timeout<Wifi::Frontend> _scan_timeout {
		_timer, *this, &Wifi::Frontend::_handle_scan_timeout };
	Timer::One_shot_timeout<Wifi::Frontend> _quality_timeout {
		_timer, *this, &Wifi::Frontend::_handle_quality_timeout };

	bool _arm_timer(Timer_type const type)
	{
		unsigned const sec = _seconds_from_type(type);
		if (!sec) { return false; }

		Genode::Microseconds const us { sec * 1000'000u };

		if (_config.verbose)
			Genode::log("Arm timer for ", _name_from_type(type), ": ", us);

		switch (type) {
		case Timer_type::CONNECTED_SCAN: _scan_timeout.schedule(us); break;
		case Timer_type::SCAN:           _scan_timeout.schedule(us); break;
		case Timer_type::SIGNAL_POLL:    _quality_timeout.schedule(us); break;
		}
		return true;
	}

	bool _arm_scan_timer()
	{
		Timer_type const type = _connected_ap.bssid_valid()
		                      ? Timer_type::CONNECTED_SCAN
		                      : Timer_type::SCAN;
		return _arm_timer(type);
	}

	bool _arm_poll_timer()
	{
		if (!_connected_ap.bssid_valid())
			return false;

		return _arm_timer(Timer_type::SIGNAL_POLL);
	}

	void _try_arming_any_timer()
	{
		_arm_scan_timer();
		(void)_arm_poll_timer();
	}

	void _handle_scan_timeout(Genode::Duration)
	{
		if (_rfkilled || _connecting) {
			if (_config.verbose)
				Genode::log("Scanning: suspend due to RFKILL or connection"
				            " attempt");
			return;
		}

		if (!_arm_scan_timer()) {
			if (_config.verbose)
				Genode::log("Timer: scanning disabled");
			return;
		}

		bool explicit_scan = false;
		_for_each_ap([&] (Accesspoint const &ap) {
			explicit_scan |= ap.explicit_scan; });

		if (explicit_scan) try {
			Explicit_scan_cmd &scan_cmd = *new (_action_alloctor) Explicit_scan_cmd(_msg);
			scan_cmd.with_ssid_buffer([&] (char *ssid_buffer,
			                               size_t const ssid_buffer_length) {

				size_t buffer_pos = 0;
				auto explicit_ssids = [&] (Accesspoint const &ap) {

					enum { SSID_ARG_LEN = 6 + 64, /* " ssid " + "a5a5a5a5..." */ };
					if (buffer_pos + SSID_ARG_LEN >= ssid_buffer_length)
						return;

					if (!ap.explicit_scan)
						return;

					char ssid_hex[64+1] = { };
					char const *ssid = ap.ssid.string();

					for (size_t i = 0; i < ap.ssid.length() - 1; i++)
						Util::byte2hex((ssid_hex + i * 2), ssid[i]);

					Genode::String<SSID_ARG_LEN + 1> tmp(" ssid ", (char const*)ssid_hex);
					size_t const tmp_len = tmp.length() - 1;

					Genode::memcpy((ssid_buffer + buffer_pos), tmp.string(), tmp_len);
					buffer_pos += tmp_len;
				};
				_for_each_ap(explicit_ssids);
			});
			_actions.enqueue(scan_cmd);

			if (_config.verbose)
				Genode::log("Queue explicit scan request");
		} catch (...) { Genode::warning("could not queue explicit scan query"); }

		else try {
			Action &act = *new (_action_alloctor) Scan_cmd(_msg);
			_actions.enqueue(act);

			if (_config.verbose)
				Genode::log("Queue scan request");
		} catch (...) { Genode::warning("could not queue scan request"); }

		_dispatch_action_if_needed();
	}

	void _handle_quality_timeout(Genode::Duration)
	{
		if (_rfkilled || _connecting) {
			if (_config.verbose)
				Genode::log("Quality polling: suspend due to RFKILL or connection"
				            " attempt");
			return;
		}

		if (!_arm_poll_timer()) {
			if (_config.verbose)
				Genode::log("Timer: signal-strength polling disabled");
			return;
		}

		try {
			Action &act = *new (_action_alloctor) Rssi_query(_msg);
			_actions.enqueue(act);

			if (_config.verbose)
				Genode::log("Queue RSSI query");
		} catch (...) { Genode::warning("could not queue RSSI query"); }

		_dispatch_action_if_needed();
	}

	/* network commands */

	void _mark_stale_aps(Genode::Xml_node const &config)
	{
		auto mark_stale = [&] (Accesspoint &ap) {
			ap.stale = true;

			config.for_each_sub_node("network", [&] ( Genode::Xml_node node) {
				Accesspoint::Ssid ssid = node.attribute_value("ssid",  Accesspoint::Ssid(""));

				if (ap.ssid == ssid) { ap.stale = false; }
			});
		};
		_for_each_ap(mark_stale);

		_remove_stale_aps();
	}

	void _remove_stale_aps()
	{
		bool remove_any = false;

		_aps.for_each([&] (Accesspoint &ap) {
			if (!ap.ssid_valid() || !ap.stale || !ap.stored())
				return;

			try {
				Action &act = *new (_action_alloctor) Remove_network_cmd(_msg, ap.id);
				_actions.enqueue(act);

				if (_config.verbose)
					Genode::log("Queue network removal: '", ap.ssid, "'");

				Genode::destroy(_ap_allocator, &ap);

				remove_any = true;
			} catch (...) {
				Genode::warning("could not queue stale network removal [", ap.id, "]: '", ap.ssid, "'");
				return;
			}
		});

		if (!remove_any)
			_add_new_aps();
	}

	void _update_aps()
	{
		bool update_any = false;
		_aps.for_each([&] (Accesspoint &ap) {
			if (!ap.stored() || !ap.update)
				return;

			try {
				Action &act = *new (_action_alloctor) Update_network_cmd(_msg, ap);
				_actions.enqueue(act);

				if (_config.verbose)
					Genode::log("Queue update network: '", ap.ssid, "'");

				update_any = true;
			} catch (...) {
				Genode::warning("could not queue update network [", ap.id, "]: '", ap.ssid, "'");
				return;
			}

		});
	}

	void _add_new_aps()
	{
		bool add_any = false;
		_aps.for_each([&] (Accesspoint &ap) {
			if (!ap.ssid_valid() || ap.stored())
				return;

			try {
				Action &act = *new (_action_alloctor) Add_network_cmd(_msg, ap);
				_actions.enqueue(act);

				if (_config.verbose)
					Genode::log("Queue add network: '", ap.ssid, "'");

				add_any = true;
			} catch (...) {
				Genode::warning("could not queue add network [", ap.id, "]: '", ap.ssid, "'");
				return;
			}

		});

		if (!add_any)
			_update_aps();
	}

	/* connection state */

	Genode::Constructible<Genode::Reporter> _state_reporter { };

	bool _connecting = false;

	enum class Bssid_offset : unsigned {
		/* by the power of wc -c, I have the start pos... */
		CONNECT = 37, CONNECTING = 33, DISCONNECT = 30, };

	Accesspoint::Bssid const _extract_bssid(char const *msg, Bssid_offset offset)
	{
		char bssid[32] = { };

		size_t const len   = 17;
		size_t const start = (size_t)offset;

		Genode::memcpy(bssid, msg + start, len);
		return Accesspoint::Bssid((char const*)bssid);
	}

	bool _auth_failure(char const *msg)
	{
		enum { REASON_OFFSET = 55, };
		unsigned reason = 0;
		Genode::ascii_to((msg + REASON_OFFSET), reason);
		switch (reason) {
		case  2: /* prev auth no longer valid */
		case 15: /* 4-way handshake timeout/failed */
			return true;
		default:
			return false;
		}
	}

	Genode::Constructible<Genode::Expanding_reporter> _ap_reporter { };

	/*
	 * CTRL interface event handling
	 */

	bool _connected_event    { false };
	bool _disconnected_event { false };
	bool _disconnected_fail  { false };
	bool _was_connected      { false };

	enum { MAX_REAUTH_ATTEMPTS = 1 };
	unsigned _reauth_attempts { 0 };

	enum { MAX_ATTEMPTS = 3, };
	unsigned _scan_attempts { 0 };

	Genode::Signal_handler<Wifi::Frontend> _events_handler;

	unsigned _last_event_id { 0 };

	void _handle_events()
	{
		char const *msg = reinterpret_cast<char const*>(_msg.event);
		unsigned const event_id = _msg.event_id;

		/* return early */
		if (_last_event_id == event_id) {
			_notify_lock_unlock();
			return;
		}

		if (results_available(msg)) {

			/*
			 * We might have to pull the socketcall task out of poll_all()
			 * because otherwise we might be late and wpa_supplicant has
			 * already removed all scan results due to BSS age settings.
			 */
			wifi_kick_socketcall();

			try {
				Action &act = *new (_action_alloctor) Scan_results_cmd(_msg, *_ap_reporter);
				_actions.enqueue(act);

				if (_config.verbose)
					Genode::log("Queue scan results");
			} catch (...) { Genode::warning("could not queue scan results"); }

		} else

		if (connecting_to_network(msg)) {
			if (!_single_autoconnect) {
				Accesspoint::Bssid const &bssid =
					_extract_bssid(msg, Bssid_offset::CONNECTING);
				_connecting = true;

				Genode::Reporter::Xml_generator xml(*_state_reporter, [&] () {
					xml.node("accesspoint", [&] () {
						xml.attribute("bssid", bssid);
						xml.attribute("state", "connecting");
					});
				});
			}
		} else

		if (network_not_found(msg)) {

			if (_single_autoconnect && ++_scan_attempts >= MAX_ATTEMPTS) {
				_scan_attempts = 0;
				_single_autoconnect = false;

				Genode::Reporter::Xml_generator xml(*_state_reporter, [&] () {
					xml.node("accesspoint", [&] () {
						xml.attribute("state", "disconnected");
						xml.attribute("rfkilled", _rfkilled);
						xml.attribute("not_found", true);
					});
				});
			}

		} else

		{
			_connected_event    = false;
			_disconnected_event = false;
			_disconnected_fail  = false;

			bool const connected    = check_recv_msg(msg, recv_table[Rmi::CONNECTED]);
			bool const disconnected = check_recv_msg(msg, recv_table[Rmi::DISCONNECTED]);
			bool const auth_failed  = disconnected && _auth_failure(msg);

			Accesspoint::Bssid const &bssid =
				_extract_bssid(msg, connected ? Bssid_offset::CONNECT
				                              : Bssid_offset::DISCONNECT);

			/* simplistic heuristic to ignore re-authentication requests */
			if (_connected_ap.bssid_valid() && auth_failed) {
				if (_reauth_attempts < MAX_REAUTH_ATTEMPTS) {
					Genode::log("ignore deauth from: ", _connected_ap.bssid);
					_reauth_attempts++;
					return;
				}
			}
			_reauth_attempts = 0;

			Accesspoint::Ssid const ssid = _connected_ap.ssid;

			/*
			 * Always reset the "global" connection state first
			 */
			_connected_ap.invalidate();
			if (connected) { _connected_ap.bssid = bssid; }
			if (connected || disconnected) { _connecting = false; }

			/*
			 * Save local connection state here for later re-use when
			 * the BSS information are handled.
			 */
			_connected_event    = connected;
			_disconnected_event = disconnected;
			_disconnected_fail  = auth_failed;

			if (_disconnected_fail) {
				/*
				 * Being able to remove a failed network from the internal
				 * state of the supplicant relies on a sucessful BSS request.
				 * In case that failes the supplicant will try to join the
				 * network again and again...
				 */
				_for_each_ap([&] (Accesspoint const &lap) {
					if (lap.ssid != ssid)
						return;

					Accesspoint const &ap = lap;

					try {
						Action &act = *new (_action_alloctor) Disable_network_cmd(_msg, ap.id);
						_actions.enqueue(act);

						if (_config.verbose)
							Genode::log("Queue disable network: [", ap.id, "]: '", ap.ssid, "'");
					} catch (...) { Genode::warning("could not queue disable network [", ap.id, "]: '", ap.ssid, "'"); }
				});
			} else

			if (_connected_event) {

				try {
					Action &act = *new (_action_alloctor) Status_query(_msg);
					_actions.enqueue(act);

					if (_config.verbose)
						Genode::log("Queue status query");
				} catch (...) { Genode::warning("could not queue status query"); }

				_for_each_ap([&] (Accesspoint const &lap) {
					if (lap.ssid != ssid)
						return;

					_connected_ap = lap;
				});

				_arm_poll_timer();
			}

			/*
			 * Generate the first rudimentary report whose missing information
			 * are (potentially) filled in later (see above).
			 */
			Genode::Reporter::Xml_generator xml(*_state_reporter, [&] () {
				xml.node("accesspoint", [&] () {
					xml.attribute("bssid", bssid);
					xml.attribute("state", connected ? "connected"
					                                 : "disconnected");
					if (disconnected) {
						xml.attribute("rfkilled", _rfkilled);
						if (auth_failed) {
							xml.attribute("auth_failure", auth_failed);
						}
					}
				});
			});

			/* reset */
			_single_autoconnect = false;
		}

		_notify_lock_unlock();

		_dispatch_action_if_needed();
	}

	/*
	 * CTRL interface command handling
	 */

	Genode::Signal_handler<Wifi::Frontend> _cmd_handler;

	unsigned _last_recv_id { 0 };

	void _handle_cmds()
	{
		char const *msg = reinterpret_cast<char const*>(_msg.recv);
		unsigned const recv_id = _msg.recv_id;


		/* return early */
		if (_last_recv_id == recv_id) {
			_notify_lock_unlock();
			return;
		}

		_last_recv_id = recv_id;

		_with_pending_action([&] (Action &action) {

			switch (action.type) {

			case Action::Type::COMMAND:
				action.check(msg);
				break;

			case Action::Type::QUERY:
				action.response(msg, _connected_ap);
				{
					Genode::Reporter::Xml_generator xml(*_state_reporter, [&] () {
						xml.node("accesspoint", [&] () {
							xml.attribute("ssid",  _connected_ap.ssid);
							xml.attribute("bssid", _connected_ap.bssid);
							xml.attribute("freq",  _connected_ap.freq);

							xml.attribute("state", _connected_event ? "connected"
							                                        : "disconnected");
							if (!_connected_event) {
								xml.attribute("rfkilled", _rfkilled);
								xml.attribute("auth_failure", _disconnected_fail);
							}

							/*
							 * Only add the attribute when we have something
							 * to report so that a consumer of the state report
							 * may take appropriate actions.
							 */
							if (_connected_ap.signal)
								xml.attribute("quality", _connected_ap.signal);
						});
					});
				}
				break;
			}

			/*
			 * We always switch to the next state after checking and
			 * handling the response from the CTRL interface.
			 */
			action.execute();

			bool const complete = action.complete();
			if (complete)
				switch (action.command) {
				case Action::Command::ADD:
				{
					Add_network_cmd const &add_cmd =
						*dynamic_cast<Add_network_cmd*>(&action);

					bool handled = false;
					Accesspoint const &added_ap = add_cmd.accesspoint();
					_for_each_ap([&] (Accesspoint &ap) {
						if (ap.ssid != added_ap.ssid)
							return;

						if (ap.stored()) {
							Genode::error("accesspoint for SSID '", ap.ssid, "' already stored ", ap.id);
							return;
						}

						ap.id = added_ap.id;
						handled = true;
					});

					/*
					 * We have to guard against having the accesspoint removed via a config
					 * update while we are still adding it to the supplicant by removing the
					 * network directly afterwards.
					 */
					if (!handled) try {
						Action &act = *new (_action_alloctor) Disable_network_cmd(_msg, added_ap.id);
						_actions.enqueue(act);

						if (_config.verbose)
							Genode::log("Queue disable network: [", added_ap.id, "]: '",
							            added_ap.ssid, "'");
					} catch (...) {
						Genode::warning("could not queue disable network [",
						                 added_ap.id, "]: '", added_ap.ssid, "'");
					}

					break;
				}
				default: /* ignore the rest */
					break;
				}

			return complete;
		});

		_notify_lock_unlock();

		_dispatch_action_if_needed();
	}

	/**
	 * Constructor
	 */
	Frontend(Genode::Env &env, Msg_buffer &msg_buffer)
	:
		_ap_allocator(env.ram(), env.rm()),
		_action_alloctor(env.ram(), env.rm()),
		_msg(msg_buffer),
		_rfkill_handler(env.ep(), *this, &Wifi::Frontend::_handle_rfkill),
		_config_rom(env, "wifi_config"),
		_config_sigh(env.ep(), *this, &Wifi::Frontend::_handle_config_update),
		_timer(env),
		_events_handler(env.ep(), *this, &Wifi::Frontend::_handle_events),
		_cmd_handler(env.ep(),    *this, &Wifi::Frontend::_handle_cmds)
	{
		_config_rom.sigh(_config_sigh);

		/* set/initialize as unblocked */
		_notify_blockade.wakeup();

		/*
		 * Both Report sessions are mandatory, let the driver fail in
		 * case they cannot be created.
		 */
		{
			_ap_reporter.construct(env, "accesspoints", "accesspoints");
			_ap_reporter->generate([&] (Genode::Xml_generator &) { });
		}

		{
			_state_reporter.construct(env, "state");
			_state_reporter->enabled(true);

			Genode::Reporter::Xml_generator xml(*_state_reporter, [&] () {
				xml.node("accesspoint", [&] () {
					xml.attribute("state", "disconnected");
					xml.attribute("rfkilled", _rfkilled);
				}); });
		}

		/* read in list of APs */
		_config_update(true);

		/* get initial RFKILL state */
		_handle_rfkill();

		/* kick-off initial scanning */
		_handle_scan_timeout(Genode::Duration(Genode::Microseconds(0)));
	}

	/**
	 * Trigger RFKILL notification
	 *
	 * Used by the wifi driver to notify front end.
	 */
	void rfkill_notify() override {
		_rfkill_handler.local_submit(); }

	/**
	 * Get result signal capability
	 *
	 * Used by the wpa_supplicant to notify front end after processing
	 * a command.
	 */
	Genode::Signal_context_capability result_sigh() {
		return _cmd_handler; }

	/**
	 * Get event signal capability
	 *
	 * Used by the wpa_supplicant to notify front whenever a event
	 * was triggered.
	 */
	Genode::Signal_context_capability event_sigh() {
		return _events_handler; }

	/**
	 * Block until events were handled by the front end
	 *
	 * Used by the wpa_supplicant to wait for the front end.
	 */
	void block_for_processing() { _notify_lock_lock(); }
};

#endif /* _WIFI_FRONTEND_H_ */
