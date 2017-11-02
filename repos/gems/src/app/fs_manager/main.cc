/*
 * \brief  File system manager
 * \author Norman Feske
 * \author Josef Soentgen
 * \date   2017-11-01
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/registry.h>
#include <os/reporter.h>

#include <block_session/block_session.h>
#include <cpu_session/cpu_session.h>
#include <file_system_session/file_system_session.h>
#include <log_session/log_session.h>
#include <pd_session/pd_session.h>
#include <report_session/report_session.h>
#include <rm_session/rm_session.h>
#include <rom_session/rom_session.h>
#include <timer_session/timer_session.h>


namespace Fs_manager {
	using namespace Genode;
	struct Priority { int value; };

	struct Start_node;

	struct Fsck;
	struct Fs;
	struct Main;
}


class Fs_manager::Start_node : Noncopyable
{
	public:

		typedef String<64>  Name;
		typedef String<100> Binary;
		typedef String<32>  Service;

	protected:

		static void _gen_common_start_node_content(Xml_generator &xml,
		                                           Name    const &name,
		                                           Binary  const &binary,
		                                           Ram_quota      ram,
		                                           Cap_quota      caps,
		                                           Priority       priority)
		{
			xml.attribute("name", name);
			xml.attribute("caps", String<64>(caps));
			xml.attribute("priority", priority.value);
			xml.node("binary", [&] () { xml.attribute("name", binary); });
			xml.node("resource", [&] () {
				xml.attribute("name", "RAM");
				xml.attribute("quantum", String<64>(ram));
			});
		}

		template <typename SESSION>
		static void _gen_provides_node(Xml_generator &xml)
		{
			xml.node("provides", [&] () {
				xml.node("service", [&] () {
					xml.attribute("name", SESSION::service_name()); }); });
		}

		static void _gen_config_route(Xml_generator &xml, char const *config_name)
		{
			xml.node("service", [&] () {
				xml.attribute("name", Rom_session::service_name());
				xml.attribute("label", "config");
				xml.node("parent", [&] () {
					xml.attribute("label", config_name); });
			});
		}

		static void _gen_default_parent_route(Xml_generator &xml)
		{
			xml.node("any-service", [&] () {
				xml.node("parent", [&] () { }); });
		}

		template <typename SESSION>
		static void _gen_forwarded_service(Xml_generator &xml,
		                                   Start_node::Name const &name)
		{
			xml.node("service", [&] () {
				xml.attribute("name", SESSION::service_name());
				xml.node("default-policy", [&] () {
					xml.node("child", [&] () {
						xml.attribute("name", name);
					});
				});
			});
		};

		virtual ~Start_node() { }

	public:

		virtual void generate(Xml_generator &xml) const = 0;
};


struct Fs_manager::Fsck : Fs_manager::Start_node
{
	bool const _repair;

	Fsck(bool repair) : _repair(repair) { }

	void generate(Xml_generator &xml) const override
	{
		xml.node("start", [&] () {
			_gen_common_start_node_content(xml, _repair ? "fsck-repair" : "fsck", "e2fsck",
			                               Ram_quota{8*1024*1024}, Cap_quota{100},
			                               Priority{-1});
			xml.node("config", [&] () {
				/* arguments */
				xml.node("arg", [&] () { xml.attribute("value", "e2fsck"); });
				xml.node("arg", [&] () { xml.attribute("value", _repair ? "-p" : "-n"); });
				xml.node("arg", [&] () { xml.attribute("value", "/dev/block"); });

				/* vfs */
				xml.node("vfs", [&] () {
					xml.node("dir", [&] () {
						xml.attribute("name", "dev");
						xml.node("null", [&] () { });
						xml.node("log", [&] () { });
						xml.node("block", [&] () { xml.attribute("name", "block"); });
					});
				});

				/* libc */
				xml.node("libc", [&] () {
					xml.attribute("stdin",  "/dev/log");
					xml.attribute("stdout", "/dev/log");
					xml.attribute("stderr", "/dev/log");
				});
			});
			xml.node("route", [&] () {
				_gen_config_route(xml, "fb_drv.config");
				_gen_default_parent_route(xml);
			});
		});
	}
};


struct Fs_manager::Fs : Fs_manager::Start_node
{
	void generate(Xml_generator &xml) const override
	{
		xml.node("start", [&] () {
			_gen_common_start_node_content(xml, "fs", "rump_fs",
			                               Ram_quota{8*1024*1024}, Cap_quota{200},
			                               Priority{-1});
			_gen_provides_node<File_system::Session>(xml);
			xml.node("config", [&] () {
				xml.attribute("fs", "ext2fs");
				xml.node("default-policy", [&] () {
					xml.attribute("root", "/");
					xml.attribute("writeable", true);
				});
			});
			xml.node("route", [&] () {
				_gen_default_parent_route(xml);
			});
		});
	}

	void generate_fs_service_forwarding_policy(Xml_generator &xml) const
	{
		xml.node("default-policy", [&] () {
			xml.node("child", [&] () {
				xml.attribute("name", "fs");
			});
		});
	}
};


struct Fs_manager::Main
{
	Env &_env;

	Attached_rom_dataspace _init_state { _env, "state"  };

	Reporter _init_config { _env, "config", "init.config" };

	Constructible<Fsck> _fsck_check;
	Constructible<Fsck> _fsck_repair;
	Constructible<Fs>   _fs;

	enum State {
		INVALID = 0,
		FSCK_CHECK, FSCK_REPAIR, FSCK_MANUAL_CHECK,
		FS_LAUNCH, FS_USABLE, FS_NOTUSABLE,
	};

	State _state { INVALID };

	void _handle_state_update();

	Signal_handler<Main> _state_update_sigh {
		_env.ep(), *this, &Main::_handle_state_update };

	static void _gen_parent_service_xml(Xml_generator &xml, char const *name)
	{
		xml.node("service", [&] () { xml.attribute("name", name); });
	};

	void _generate_init_config(State, Reporter &) const;

	/*
	 * Children state check helper utilities
	 */

	template <typename FUNC>
	void _for_each_child_with_name(Xml_node node, char const *name,
	                               FUNC const &FN) const
	{
		typedef Fs_manager::Start_node::Name Name;
		node.for_each_sub_node("child", [&] (Xml_node child) {
			if (child.attribute_value("name", Name()) == name) {
				FN(child);
			}
		});
	}

	bool _child_active(Xml_node node, char const *name) const
	{
		bool active = false;
		_for_each_child_with_name(node, name, [&] (Xml_node child) {
			/*
			 * For now init sets 'state' to 'incomplete' only
			 * when the child is not yet active.
			 */
			active = !child.has_attribute("state");
		});
		return active;
	}

	bool _child_exited(Xml_node node, char const *name) const
	{
		bool exited = false;
		_for_each_child_with_name(node, name, [&] (Xml_node child) {
			exited = child.has_attribute("exited");
		});
		return exited;
	}

	int _child_exit_value(Xml_node node, char const *name) const
	{
		int exit_value = -1;
		_for_each_child_with_name(node, name, [&] (Xml_node child) {
			exit_value = child.attribute_value<unsigned>("exited", -1);
		});
		return exit_value;
	}

	Main(Env &env) : _env(env)
	{
		_init_config.enabled(true);

		_init_state.sigh(_state_update_sigh);

		_generate_init_config(State::INVALID, _init_config);

		_handle_state_update();
	}
};


/*
 * Handle init state reports
 *
 * This method implements a state machine to manage the state
 * transitions, which are triggered by the state reports from
 * the sub-init component. Since there can be multiple init
 * state reports in each state, we have to guard against them.
 */
void Fs_manager::Main::_handle_state_update()
{
	_init_state.update();

	if (!_init_state.valid()) { return; }

	Xml_node state = _init_state.xml();

	switch (_state) {
	/*
	 * Initial state
	 *
	 * Start checking fsck child
	 */
	case State::INVALID:
	{
		_fsck_check.construct(false);
		_state = State::FSCK_CHECK;

		_generate_init_config(_state, _init_config);
		break;
	}
	/*
	 * Checking state
	 *
	 * Wait until the fsck child has finished checking the
	 * file system. In case of an error start repairing fsck
	 * child, otherwise start file system provider.
	 */
	case State::FSCK_CHECK:
	{
		/* child is still running */
		if (!_child_exited(state, "fsck")) { break; }

		_fsck_check.destruct();

		int const ec = _child_exit_value(state, "fsck");
		/* e2fsck(8): exit code > 2 denotes errors */
		if (ec > 2) {
			_fsck_repair.construct(true);
			_state = State::FSCK_REPAIR;
		} else {
			_fs.construct();
			_state = State::FS_LAUNCH;
		}

		_generate_init_config(_state, _init_config);
		break;
	}
	/*
	 * Repair state
	 *
	 * Wait until the fsck repair child has finished fixing
	 * the defective file system. When successful start file
	 * system server, otherwise request manual intervention.
	 */
	case State::FSCK_REPAIR:
	{
		/* child is still running */
		if (!_child_exited(state, "fsck-repair")) { break; }

		_fsck_repair.destruct();

		int const ec = _child_exit_value(state, "fsck");
		/* e2fsck(8): exit code < 4 is fine */
		if (ec < 4) {
			_fs.construct();
			_state = State::FS_LAUNCH;

			_generate_init_config(_state, _init_config);
			break;
		}

		_fsck_repair.destruct();
		_state = State::FSCK_MANUAL_CHECK;
		/* fall-through */
	}
	/*
	 * Manual check state
	 *
	 * We end up here in case repairing the file system was
	 * unsuccessful. At this point manual interventions is
	 * required.
	 */
	case State::FSCK_MANUAL_CHECK:
	{
		/* only notify once */
		if (!_fs.constructed()) { return; }

		_fs.destruct();

		Genode::error("could not fix file system errors, "
		              "manual intervention needed");

		_generate_init_config(_state, _init_config);
		break;
	}
	/*
	 * File system launch state
	 *
	 * When entering this state, the file system child was
	 * already created. If it becomes usable, e.g. the component
	 * is running and the file system can be mounted, activate
	 * services forwarding.
	 */
	case State::FS_LAUNCH:
	{
		/* file system child is still in incomplete state */
		if (!_child_active(state, "fs")) { break; }

		/* child has exited, deem the file system unusable */
		if (_child_exited(state, "fs")) {
			_fs.destruct();
			_state = State::FS_NOTUSABLE;

			_generate_init_config(_state, _init_config);
			break;
		}

		_state = State::FS_USABLE;

		_generate_init_config(_state, _init_config);
		break;
	}
	/*
	 * File system usable state
	 *
	 * At this point the file system can be used by any client...
	 */
	case State::FS_USABLE:
	/*
	 * ... or not but it all boils down to the same action.
	 */
	case State::FS_NOTUSABLE:
		/* intentionally left empty */
		break;
	} /* switch */
}


void Fs_manager::Main::_generate_init_config(State state,
                                             Reporter &reporter) const
{
	Reporter::Xml_generator xml(reporter, [&] () {

		xml.attribute("verbose", false);
		xml.attribute("prio_levels", 2);

		xml.node("report", [&] () { xml.attribute("child_ram", true); });

		xml.node("parent-provides", [&] () {
			_gen_parent_service_xml(xml, Rom_session::service_name());
			_gen_parent_service_xml(xml, Cpu_session::service_name());
			_gen_parent_service_xml(xml, Pd_session::service_name());
			_gen_parent_service_xml(xml, Rm_session::service_name());
			_gen_parent_service_xml(xml, Log_session::service_name());
			_gen_parent_service_xml(xml, Timer::Session::service_name());
			_gen_parent_service_xml(xml, Report::Session::service_name());
			_gen_parent_service_xml(xml, Block::Session::service_name());
		});

		/*
		 * Generate an empty configuration to remove all children and
		 * free up any resources, e.g. the Block service.
		 */
		if (state == State::FS_NOTUSABLE) { return; }

		/*
		 * Generate the configuration according to the state. However,
		 * instead of explicitly checking the current state of the state
		 * machine, we use the state of our constructibles.
		 */
		if (_fsck_check.constructed())  { _fsck_check->generate(xml); }
		if (_fsck_repair.constructed()) { _fsck_repair->generate(xml); }
		if (_fs.constructed())          { _fs->generate(xml); }

		/*
		 * If the file system is usable, enable the service forwarding rule
		 * and thereby give any waiting client access.
		 */
		if (_fs.constructed() && state == State::FS_USABLE) {
			xml.node("service", [&] () {
				xml.attribute("name", File_system::Session::service_name());
				_fs->generate_fs_service_forwarding_policy(xml);
			});
		}

		/*
		 * At this point the file system is not usable. Disable service
		 * forwarding so that any client will get a 'Service_denied'
		 * response.
		 */
		if (!_fs.constructed() && state == State::FSCK_MANUAL_CHECK) {
			xml.node("service", [&] () {
				xml.attribute("name", File_system::Session::service_name());
			});
		}
	});
}


void Component::construct(Genode::Env &env) { static Fs_manager::Main main(env); }
