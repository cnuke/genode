/*
 * \brief  Debugging tool for triggering of timing issues
 * \author Christian Prochaska
 * \date   2022-10-20
 *
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>
#include <base/blockade.h>

class Burn_helper : public Genode::Thread
{
	private:

		Genode::Env      &_env;
		Genode::Blockade &_blockade;

	public:

		Burn_helper(Genode::Env &env, int cpu_index, Genode::Blockade &blockade)
		: Genode::Thread(env,
		                 "burn_helper",
		                 8192,
		                 env.cpu().affinity_space().location_of_index(cpu_index),
		                 Weight(),
		                 env.cpu()),
		  _env(env),
		  _blockade(blockade) { }

		void entry() override
		{
			for (;;) { _blockade.wakeup(); }
		}
};


void Component::construct(Genode::Env &env)
{
	static constexpr int test = 1;

	switch (test) {
	case 0:
		{
			for (;;) { }
		}
		break;
	case 1:
		{
			Genode::Blockade blockade;
			Burn_helper burn_helper(env, 1, blockade);
			burn_helper.start();

			for (;;) {
				blockade.block();
			}
		}
		break;
	case 2:
		{
			for (;;) {
				env.pd().ram_quota();
			}
		}
		break;
	}
}
