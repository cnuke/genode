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
#include <util/list_model.h>
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

enum Recv_msg_index : unsigned char {
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
	return check_recv_msg(msg, recv_table[Recv_msg_index::OK]); }


static bool cmd_fail(char const *msg) {
	return check_recv_msg(msg, recv_table[Recv_msg_index::FAIL]); }


static bool results_available(char const *msg) {
	return check_recv_msg(msg, recv_table[Recv_msg_index::SCAN_RESULTS]); }


static bool connecting_to_network(char const *msg) {
	return check_recv_msg(msg, recv_table[Recv_msg_index::SME_AUTH]); }


static bool network_not_found(char const *msg) {
	return check_recv_msg(msg, recv_table[Recv_msg_index::NOT_FOUND]); }


static bool disconnected_from_network(char const *msg) {
	return check_recv_msg(msg, recv_table[Recv_msg_index::DISCONNECTED]); }


static bool connected_to_network(char const *msg) {
	return check_recv_msg(msg, recv_table[Recv_msg_index::CONNECTED]); }


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
 * The Accesspoint object contains all information to join
 * a wireless network.
 */
struct Accesspoint : Genode::Interface
{
	using Bssid = Genode::String<17+1>;
	using Freq  = Genode::String< 4+1>;
	using Prot  = Genode::String< 7+1>;
	using Ssid  = Genode::String<32+1>;
	using Pass  = Genode::String<63+1>;

	static bool valid(Ssid const &ssid) {
		return ssid.length() > 1 && ssid.length() <= 32 + 1; }

	static bool valid(Pass const &pass) {
		return pass.length() > 8 && pass.length() <= 63 + 1; }

	static bool valid(Bssid const &bssid) {
		return bssid.length() == 17 + 1; }

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
	 */
	int id { -1 };

	/*
	 * Internal configuration fields
	 */
	bool auto_connect  { false };
	bool explicit_scan { false };

	/**
	 * Default constructor
	 */
	Accesspoint() { }

	Accesspoint(Bssid const &bssid, Ssid const &ssid)
	: bssid { bssid }, ssid { ssid } { }

	/**
	 * Constructor that initializes information fields
	 */
	Accesspoint(char const *bssid, char const *freq,
	            char const *prot,  char const *ssid, unsigned signal)
	: bssid(bssid), freq(freq), prot(prot), ssid(ssid), signal(signal)
	{ }

	void print(Genode::Output &out) const
	{
		Genode::print(out, "SSID: '",         ssid, "'",  " "
		                   "BSSID: '",        bssid, "'", " "
		                   "protection: ",    prot, " "
		                   "id: ",            id, " "
		                   "quality: ",       signal, " "
		                   "auto_connect: ",  auto_connect, " "
		                   "explicit_scan: ", explicit_scan);
	}

	bool valid() const { return Accesspoint::valid(bssid); }

	void invalidate() { ssid = Ssid(); bssid = Bssid(); }

	bool wpa()         const { return prot != "NONE"; }
	bool wpa3()        const { return prot == "WPA3"; }

	bool stored()      const { return id != -1; }

	bool updated_from(Accesspoint const &other)
	{
		bool const update = ((Accesspoint::valid(other.bssid) && other.bssid != bssid)
		                 || pass          != other.pass
		                 || prot          != other.prot
		                 || explicit_scan != other.explicit_scan
		                 || auto_connect  != other.auto_connect);
		if (!update)
			return false;

		if (Accesspoint::valid(other.bssid))
			bssid = other.bssid;

		pass          = other.prot;
		prot          = other.prot;
		auto_connect  = other.auto_connect;
		explicit_scan = other.explicit_scan;
		return true;
	}

	static Accesspoint from_xml(Genode::Xml_node const &node)
	{
		Accesspoint ap { };

		ap.ssid  = node.attribute_value("ssid",  Accesspoint::Ssid());
		ap.bssid = node.attribute_value("bssid", Accesspoint::Bssid());

		ap.pass          = node.attribute_value("passphrase", Accesspoint::Pass(""));
		ap.prot          = node.attribute_value("protection", Accesspoint::Prot("NONE"));
		ap.auto_connect  = node.attribute_value("auto_connect", true);
		ap.explicit_scan = node.attribute_value("explicit_scan", false);

		return ap;
	}
};


struct Network : Genode::List_model<Network>::Element
{

	Accesspoint _accesspoint { };

	Network(Accesspoint const &ap) : _accesspoint { ap } { }

	virtual ~Network() { }

	void with_accesspoint(auto const &fn)
	{
		fn(_accesspoint);
	}

	void with_accesspoint(auto const &fn) const
	{
		fn(_accesspoint);
	}

	/**************************
	 ** List_model interface **
	 **************************/

	static bool type_matches(Genode::Xml_node const &node)
	{
		return node.has_type("network");
	}

	bool matches(Genode::Xml_node const &node)
	{
		return _accesspoint.ssid == node.attribute_value("ssid", Accesspoint::Ssid());
	}
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
		LOG_LEVEL, REMOVE, SCAN, SCAN_RESULTS, SET, UPDATE, };
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
		Genode::print(out, "Add_network_cmd[", (unsigned)_state,
		                   "] '", _accesspoint.ssid, "'");
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
			bool const valid = Accesspoint::valid(_accesspoint.bssid);
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
		Genode::print(out, "Remove_network_cmd[", (unsigned)_state, "] id: ", _id);
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
		Genode::print(out, "Update_network_cmd[", (unsigned)_state,
		                   "] id: ", _accesspoint.id);
	}

	void execute() override
	{
		// XXX change to disable -> psk ?-> enable
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
		Genode::print(out, "Scan_cmd[", (unsigned)_state, "]");
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
		Genode::print(out, "Explicit_scan_cmd[", (unsigned)_state, "]");
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
		Genode::print(out, "Scan_results_cmd[", (unsigned)_state, "]");
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
		Genode::print(out, "Set_cmd[", (unsigned)_state, "] key: '",
		                   _key, "' value: '", _value, "'");
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("SET ", _key, " \"", _value, "\""));
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
				error("could not set '", _key, "' to '", _value, "': '", msg, "'");
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
 * Action for setting a configuration variable
 */
struct Log_level_cmd : Action
{
	using Level = Genode::String<16>;

	enum class State : unsigned {
		INIT, LOG_LEVEL, COMPLETE
	};
	Ctrl_msg_buffer &_msg;
	State            _state;

	Level _level;

	Log_level_cmd(Ctrl_msg_buffer &msg, Level const &level)
	:
		Action      { Command::LOG_LEVEL },
		_msg        { msg },
		_state      { State::INIT },
		_level      { level }
	{ }

	void print(Genode::Output &out) const override
	{
		Genode::print(out, "Log_level_cmd[", (unsigned)_state, "] '", _level, "'");
	}

	void execute() override
	{
		switch (_state) {
		case State::INIT:
			ctrl_cmd(_msg, Cmd("LOG_LEVEL ", _level));
			_state = State::LOG_LEVEL;
			break;
		case State::LOG_LEVEL:
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
		case State::LOG_LEVEL:
			if (!cmd_successful(msg)) {
				error("could not set LOG_LEVEL to ", _level);
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
		Genode::print(out, "Bss_query[", (unsigned)_state, "] ", _bssid);
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
		Genode::print(out, "Rssi_query[", (unsigned)_state, "]");
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
		Genode::print(out, "Status_query[", (unsigned)_state, "]");
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

	/* Network handling */

	Genode::Heap                _network_allocator;
	Genode::List_model<Network> _network_list { };

	/*
	 * Action queue handling
	 */

	Genode::Heap _action_alloc;
	Genode::Fifo<Action> _actions { };

	Action *_pending_action { nullptr };

	void _queue_action(Action &action, bool verbose = false)
	{
		_actions.enqueue(action);
		if (verbose)
			Genode::log("Queue ", action);
	}

	void _with_pending_action(auto const &fn)
	{
		if (!_pending_action)
			_actions.dequeue([&] (Action &action) {
				_pending_action = &action; });

		bool const complete = _pending_action ? fn(*_pending_action)
		                                      : false;
		if (complete) {
			Genode::destroy(_action_alloc, _pending_action);
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

	Accesspoint _connecting  { };

	Genode::Constructible<Genode::Expanding_reporter> _ap_reporter { };

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

		bool intervals_changed(Config const &cfg) const
		{
			return connected_scan_interval != cfg.connected_scan_interval
			    || scan_interval           != cfg.scan_interval
			    || update_quality_interval != cfg.update_quality_interval;
		}

		bool verbose { DEFAULT_VERBOSE };
		bool rfkill  { DEFAULT_RFKILL };

		bool rfkill_changed(Config const &cfg) const {
			return rfkill != cfg.rfkill; }

		/* see wpa_debug.h - EXCESSIVE, MSGDUMP, DEBUG, INFO, WARNING, ERROR */
		using Log_level = Log_level_cmd::Level;
		Log_level log_level { "" };

		bool log_level_changed(Config const &cfg) const {
			return log_level != cfg.log_level; }

		bool log_level_set() const {
			return log_level.length() > 1; }

		using Bgscan = Genode::String<16>;
		Bgscan bgscan { "" };

		bool bgscan_changed(Config const &cfg) const {
			return bgscan != cfg.bgscan; }

		bool bgscan_set() const {
			return bgscan.length() > 1; }

		static Config from_xml(Genode::Xml_node const &node)
		{
			bool const verbose       = node.attribute_value("verbose",
			                                                (bool)DEFAULT_VERBOSE);
			bool const rfkill        = node.attribute_value("rfkill",
			                                                (bool)DEFAULT_RFKILL);
			Log_level const log_level =
				node.attribute_value("log_level", Log_level("ERROR"));

			Bgscan const bgscan =
				node.attribute_value("bgscan", Bgscan("simple:30:-70:600"));

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
				.rfkill                  = rfkill,
				.log_level               = log_level,
				.bgscan                  = bgscan
			};
			return new_config;
		}
	};

	Config _config { };

	void _config_update(bool initial_config)
	{
		_config_rom.update();

		if (!_config_rom.valid())
			return;

		Genode::Xml_node const config_node = _config_rom.xml();

		Config const old_config = _config;

		_config = Config::from_xml(config_node);

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

				_connected_ap.invalidate();
				_connecting = Accesspoint();
			}
		}

		if (_config.log_level_changed(old_config) || initial_config)
			if (_config.log_level_set())
				_queue_action(*new (_action_alloc)
					Log_level_cmd(_msg, _config.log_level), _config.verbose);

		if (_config.bgscan_changed(old_config) || initial_config)
			if (_config.bgscan_set())
					_queue_action(*new (_action_alloc)
						Set_cmd(_msg, Set_cmd::Key("bgscan"),
						              Set_cmd::Value(_config.bgscan)),
						_config.verbose);

		_network_list.update_from_xml(config_node,

			[&] (Genode::Xml_node const &node) -> Network & {

				Accesspoint const ap = Accesspoint::from_xml(node);

				bool const ssid_invalid = !Accesspoint::valid(ap.ssid);
				if (ssid_invalid)
					Genode::warning("accesspoint has invalid ssid: '", ap.ssid, "'");

				bool const pass_invalid = ap.wpa() && !Accesspoint::valid(ap.pass);
				if (pass_invalid)
					Genode::warning("accesspoint '", ap.ssid, "' has invalid psk");

				if (!ssid_invalid || !pass_invalid)
					_queue_action(*new (_action_alloc)
						Add_network_cmd(_msg, ap), _config.verbose);

				return *new (_network_allocator) Network(ap);
			},
			[&] (Network &network) {

				network.with_accesspoint([&] (Accesspoint &ap) {

					if (!Accesspoint::valid(ap.ssid) || !ap.stored())
						return;

					_queue_action(*new (_action_alloc)
						Remove_network_cmd(_msg, ap.id), _config.verbose);
				});

				Genode::destroy(_network_allocator, &network);
			},
			[&] (Network &network, Genode::Xml_node const &node) {
				Accesspoint const updated_ap = Accesspoint::from_xml(node);

				network.with_accesspoint([&] (Accesspoint &ap) {

					if (!ap.updated_from(updated_ap))
						return;

					if (!ap.stored())
						return;

					_queue_action(*new (_action_alloc)
						Update_network_cmd(_msg, ap), _config.verbose);
				});
			});

		_dispatch_action_if_needed();
	}

	void _handle_config_update() { _config_update(false); }

	/* state */

	bool _single_autoconnect() const
	{
		unsigned count = 0;
		_network_list.for_each([&] (Network const &network) {
			network.with_accesspoint([&] (Accesspoint const &ap) {
				count += ap.auto_connect; }); });
		return count == 1;
	}

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
		Timer_type const type = Accesspoint::valid(_connected_ap.bssid)
		                      ? Timer_type::CONNECTED_SCAN
		                      : Timer_type::SCAN;
		return _arm_timer(type);
	}

	bool _arm_poll_timer()
	{
		if (!Accesspoint::valid(_connected_ap.bssid))
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
		if (_rfkilled) {
			if (_config.verbose)
				Genode::log("Scanning: suspend due to RFKILL");
			return;
		}

		if (!_arm_scan_timer()) {
			if (_config.verbose)
				Genode::log("Timer: scanning disabled");
			return;
		}

		bool explicit_scan = false;
		_network_list.for_each([&] (Network const &network) {
			network.with_accesspoint([&] (Accesspoint const &ap) {
			explicit_scan |= ap.explicit_scan; });
		});

		if (explicit_scan) {
			Explicit_scan_cmd &scan_cmd =
				*new (_action_alloc) Explicit_scan_cmd(_msg);

			scan_cmd.with_ssid_buffer([&] (char *ssid_buffer,
			                               size_t const ssid_buffer_length) {

				size_t buffer_pos = 0;
				_network_list.for_each([&] (Network const &network) {
					network.with_accesspoint([&] (Accesspoint const &ap) {

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
					});
				});
			});
			_queue_action(scan_cmd, _config.verbose);
		}

		else {
			_queue_action(*new (_action_alloc) Scan_cmd(_msg), _config.verbose);
		}

		_dispatch_action_if_needed();
	}

	void _handle_quality_timeout(Genode::Duration)
	{
		if (_rfkilled) {
			if (_config.verbose)
				Genode::log("Quality polling: suspend due to RFKIL");
			return;
		}

		if (!_arm_poll_timer()) {
			if (_config.verbose)
				Genode::log("Timer: signal-strength polling disabled");
			return;
		}

		_queue_action(*new (_action_alloc) Rssi_query(_msg), _config.verbose);

		_dispatch_action_if_needed();
	}

	/* connection state */

	Genode::Constructible<Genode::Expanding_reporter> _state_reporter { };

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

	Accesspoint::Ssid const _extract_ssid(char const *msg)
	{
		char ssid[64] = { };
		size_t const start = 58;

		/* XXX assume "SME:.*SSID='xx xx' ...)", so look for the
		 *     closing ' but we _really_ should use something like
		 *     printf_encode/printf_deccode functions
		 *     (see wpa_supplicant/src/utils/common.c) and
		 *     remove our patch…
		 */
		size_t const len = Util::next_char(msg, start, 0x27);
		if (!len || len >= 33)
			return Accesspoint::Ssid();

		Genode::memcpy(ssid, msg + start, len);

		return Accesspoint::Ssid((char const *)ssid);
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

	/*
	 * CTRL interface event handling
	 */

	bool _connected_event    { false };
	bool _disconnected_event { false };
	bool _disconnected_fail  { false };

	enum { MAX_REAUTH_ATTEMPTS = 3 };
	unsigned _reauth_attempts { 0 };

	Genode::Signal_handler<Wifi::Frontend> _events_handler;

	void _with_new_event(auto const &fn)
	{
		static unsigned _last_event_id { 0 };

		char const *msg = reinterpret_cast<char const*>(_msg.event);
		unsigned const event_id = _msg.event_id;

		if (_last_event_id != event_id) {
			_last_event_id = event_id;
			fn(msg);
		}

		_notify_blockade.wakeup();
	}

	void _handle_events()
	{
		_with_new_event([&] (char const *msg) {

			_connected_event    = false;
			_disconnected_event = false;
			_disconnected_fail  = false;

			/*
			 * CTRL-EVENT-SCAN-RESULTS
			 */
			if (results_available(msg)) {

				/*
				 * We might have to pull the socketcall task out of poll_all()
				 * because otherwise we might be late and wpa_supplicant has
				 * already removed all scan results due to BSS age settings.
				 */
				wifi_kick_socketcall();

				_queue_action(*new (_action_alloc)
					Scan_results_cmd(_msg, *_ap_reporter), _config.verbose);
			} else

			/*
			 * SME: Trying to authenticate with ...
			 */
			if (connecting_to_network(msg)) {

				Accesspoint::Bssid const &bssid =
					_extract_bssid(msg, Bssid_offset::CONNECTING);

				Accesspoint::Ssid const &ssid = _extract_ssid(msg);

				_connecting = Accesspoint(bssid, ssid);

				_state_reporter->generate([&] (Genode::Xml_generator &xml) {

					xml.node("accesspoint", [&] () {
						if (Accesspoint::valid(ssid))
							xml.attribute("ssid", ssid);

						xml.attribute("bssid", bssid);
						xml.attribute("state", "connecting");
					});
				});
			} else

			/*
			 * CTRL-EVENT-NETWORK-NOT-FOUND
			 */
			if (network_not_found(msg)) {

				if (Accesspoint::valid(_connecting.ssid) && _single_autoconnect()) {
					_network_list.for_each([&] (Network &network) {
						network.with_accesspoint([&] (Accesspoint &ap) {

							if (ap.ssid != _connecting.ssid)
								return;

							_state_reporter->generate([&] (Genode::Xml_generator &xml) {
								xml.node("accesspoint", [&] () {
									if (Accesspoint::valid(_connecting.ssid))
										xml.attribute("ssid", _connecting.ssid);

									xml.attribute("state", "disconnected");
									xml.attribute("not_found", true);
								});
							});
							_connecting = Accesspoint();
						});
					});
				}

			} else

			/*
			 * CTRL-EVENT-DISCONNECTED ... reason=...
			 */
			if (disconnected_from_network(msg)) {

				bool const auth_failed = _auth_failure(msg);

				_disconnected_event = true;
				_disconnected_fail  = auth_failed;

				Accesspoint::Bssid const &bssid =
					_extract_bssid(msg, Bssid_offset::DISCONNECT);

				/* simplistic heuristic to ignore re-authentication requests */
				if (Accesspoint::valid(_connected_ap.bssid) && auth_failed) {
					if (_reauth_attempts < MAX_REAUTH_ATTEMPTS) {
						Genode::log("ignore deauth from: ", _connected_ap.bssid);
						_reauth_attempts++;
						return;
					}
				}
				_reauth_attempts = 0;

				Accesspoint::Ssid const ssid            = _connected_ap.ssid;
				Accesspoint::Ssid const connecting_ssid = _connecting.ssid;

				_connected_ap = Accesspoint();
				_connecting   = Accesspoint();

				_network_list.for_each([&] (Network &network) {
					network.with_accesspoint([&] (Accesspoint &ap) {

						if (ap.ssid != (auth_failed ? connecting_ssid : ssid))
							return;

						if (auth_failed) {
							ap.auto_connect = false;

							_queue_action(*new (_action_alloc)
								Update_network_cmd(_msg, ap), _config.verbose);
						}

						_state_reporter->generate([&] (Genode::Xml_generator &xml) {
							xml.node("accesspoint", [&] () {

								if (auth_failed) {
									if (Accesspoint::valid(connecting_ssid))
										xml.attribute("ssid", connecting_ssid);
								} else
									if (Accesspoint::valid(ssid))
										xml.attribute("ssid", ssid);

								xml.attribute("bssid", bssid);
								xml.attribute("state", "disconnected");
								xml.attribute("auth_failure", auth_failed);
							});
						});
					});
				});
			} else

			/*
			 * CTRL-EVENT-CONNECTED - Connection to ...
			 */
			if (connected_to_network(msg)) {

				_connected_event = true;

				Accesspoint::Bssid const &bssid =
					_extract_bssid(msg, Bssid_offset::CONNECT);

				Accesspoint::Ssid const connecting_ssid = _connecting.ssid;

				_connected_ap = Accesspoint();
				_connecting   = Accesspoint();

				_connected_ap.bssid = bssid;

				_queue_action(*new (_action_alloc) Status_query(_msg),
					_config.verbose);

				_network_list.for_each([&] (Network const &network) {
					network.with_accesspoint([&] (Accesspoint const &ap) {
						if (ap.ssid != connecting_ssid)
							return;

						_connected_ap = ap;
					});
				});

				_arm_poll_timer();
			}
		});

		_dispatch_action_if_needed();
	}

	/*
	 * CTRL interface command handling
	 */

	Genode::Signal_handler<Wifi::Frontend> _cmd_handler;

	void _with_new_cmd_result(auto const &fn)
	{
		static unsigned _last_recv_id { 0 };

		char const *msg = reinterpret_cast<char const*>(_msg.recv);
		unsigned const recv_id = _msg.recv_id;

		/* return early */
		if (_last_recv_id != recv_id) {
			_last_recv_id = recv_id;
			fn(msg);
		}

		_notify_blockade.wakeup();
	}

	void _handle_cmds()
	{
		_with_new_cmd_result([&] (char const *msg) {

			_with_pending_action([&] (Action &action) {

				switch (action.type) {

				case Action::Type::COMMAND:
					action.check(msg);
					break;

				case Action::Type::QUERY:
					action.response(msg, _connected_ap);
					{
						_state_reporter->generate([&] (Genode::Xml_generator &xml) {
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
						_network_list.for_each([&] (Network &network) {
							network.with_accesspoint([&] (Accesspoint &ap) {
								if (ap.ssid != added_ap.ssid)
									return;

								if (ap.stored()) {
									Genode::error("accesspoint for SSID '", ap.ssid, "' "
									              "already stored ", ap.id);
									return;
								}

								ap.id = added_ap.id;
								handled = true;
							});
						});

						/*
						 * We have to guard against having the accesspoint removed via a config
						 * update while we are still adding it to the supplicant by removing the
						 *
						 * network directly afterwards.
						 */
						if (!handled) {
							_queue_action(*new (_action_alloc)
								Remove_network_cmd(_msg, added_ap.id), _config.verbose);
						} else

						if (handled && _single_autoconnect())
							/*
							 * To accomodate a management component that only deals
							 * with one network, e.g. the sculpt_manager, generate a
							 * fake connecting event. Either a connected or disconnected
							 * event will bring us to square one.
							 */
							/* XXX if the network is replaced _connecting will not get updated */
							if (!Accesspoint::valid(_connected_ap.ssid) && !_rfkilled) {
								_network_list.for_each([&] (Network const &network) {
									network.with_accesspoint([&] (Accesspoint const &ap) {

										_state_reporter->generate([&] (Genode::Xml_generator &xml) {
											xml.node("accesspoint", [&] () {
												xml.attribute("ssid",  ap.ssid);
												xml.attribute("state", "connecting");
											});
										});

										_connecting = ap;
									});
								});
							}

						break;
					}
					default: /* ignore the rest */
						break;
					}

				return complete;
			});
		});

		_dispatch_action_if_needed();
	}

	/**
	 * Constructor
	 */
	Frontend(Genode::Env &env, Msg_buffer &msg_buffer)
	:
		_network_allocator(env.ram(), env.rm()),
		_action_alloc(env.ram(), env.rm()),
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
			_state_reporter.construct(env, "state", "state");
			_state_reporter->generate([&] (Genode::Xml_generator &xml) {
				xml.node("accesspoint", [&] () {
					xml.attribute("state", "disconnected");
				});
			});
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
	void block_for_processing() { _notify_blockade.block(); }
};

#endif /* _WIFI_FRONTEND_H_ */
