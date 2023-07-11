/*
 * \brief  Log session component and root
 * \author Josef Soentgen
 * \date   2022-11-15
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LOG_H_
#define _LOG_H_

/* base includes */
#include <log_session/log_session.h>
#include <root/component.h>

namespace Black_hole {

	using namespace Genode;

	class  Log_session;
	class  Log_root;
}


class Black_hole::Log_session : public Rpc_object<Genode::Log_session>
{
	public:

		Log_session() { }

		void write(String const &) override { }
};


class Black_hole::Log_root : public Root_component<Log_session>
{
	private:

		Env &_env;

	protected:

		Log_session *_create_session(const char *) override
		{
			return new (md_alloc()) Log_session();
		}

	public:

		Log_root(Env &env,
		         Allocator &alloc)
		:
			Root_component<Log_session> { &env.ep().rpc_ep(), &alloc },
			_env                        { env }
		{ }
};

#endif /* _LOG_H_ */
