/*
 * \brief  Gpio driver for the RaspberryPI
 * \author Reinier Millo Sánchez <rmillo@uclv.cu>
 * \author Alexy Gallardo Segura <alexy@uclv.cu>
 * \author Humberto Lopéz Leon <humberto@uclv.cu>
 * \date   2015-07-23
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__GPIO__SPEC__RPI__DRIVER_H_
#define _DRIVERS__GPIO__SPEC__RPI__DRIVER_H_

/* Genode includes */
#include <base/log.h>
#include <drivers/defs/rpi.h>
#include <gpio/driver.h>
#include <irq_session/connection.h>

/* local includes */
#include <gpio.h>

static int verbose = 1;

namespace Gpio { class Rpi_driver; }

class Gpio::Rpi_driver : public Driver
{
	private:

		enum {
			IRQ      = 49,
			MAX_PINS = 54,
		};

		Reg                                    _reg;
		Genode::Irq_connection                 _irq;
		Genode::Signal_handler<Rpi_driver>     _dispatcher;
		Genode::Signal_context_capability      _sig_cap[MAX_PINS];
		bool                                   _irq_enabled[MAX_PINS];
		bool                                   _async;

		void _handle()
		{
			_reg.for_each_gpio_status([&] (unsigned i, bool s) {
				if (!s || !_irq_enabled[i] || !_sig_cap[i].valid()) { return; }
				Genode::Signal_transmitter(_sig_cap[i]).submit();
			});
		}

		void _invalid_gpio(unsigned gpio) {
			Genode::error("invalid GPIO pin number ", gpio); }

	public:

		Rpi_driver(Genode::Env &env)
		:
			_reg(env, Rpi::GPIO_CONTROLLER_BASE, 0, Rpi::GPIO_CONTROLLER_SIZE),
			_irq(env, IRQ),
			_dispatcher(env.ep(), *this, &Rpi_driver::_handle),
			_async(false)
		{
			_irq.sigh(_dispatcher);
			_irq.ack_irq();
		}

		void set_async_events(bool async) { _async = async; }

		void set_func(unsigned gpio, Reg::Function function)
		{
			if (verbose)
				Genode::log("set_func: gpio=", gpio, " function=", (int)function);

			_reg.set_gpio_function(gpio, function);
		}


		/******************************
		 **  Driver interface  **
		 ******************************/

		bool gpio_valid(unsigned gpio) override { return gpio < MAX_PINS; }

		void direction(unsigned gpio, bool input) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }

			if (verbose)
				Genode::log("direction: gpio=", gpio, " input=", input);

			Reg::Function f = input ? Reg::FSEL_INPUT : Reg::FSEL_OUTPUT;
			_reg.set_gpio_function(gpio, f);
		}

		void write(unsigned gpio, bool level) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }

			if (verbose)
				Genode::log("write: gpio=", gpio, " level=", level);

			if (_reg.get_gpio_function(gpio)!=Reg::FSEL_OUTPUT)
				warning("GPIO pin ", gpio, " is not configured for output");

			if (level)
				_reg.set_gpio_level(gpio);
			else
				_reg.clear_gpio_level(gpio);
		}

		bool read(unsigned gpio) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return 0; }
			if(_reg.get_gpio_function(gpio) != Reg::FSEL_INPUT)
				warning("GPIO pin ", gpio, " is not configured for input");

			return _reg.get_gpio_level(gpio);
		}

		void debounce_enable(unsigned, bool) override {
			Genode::warning("debounce_enable not supported!"); }

		void debounce_time(unsigned, unsigned long) override {
			Genode::warning("debounce_time not supported!"); }

		void falling_detect(unsigned gpio) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }

			if (verbose) Genode::log("falling_detect: gpio=", gpio);

			if(_async)
				_reg.set_gpio_async_falling_detect(gpio);
			else
				_reg.set_gpio_falling_detect(gpio);
		}

		void rising_detect(unsigned gpio) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }

			if (verbose) Genode::log("rising_detect: gpio=", gpio);

			if(_async)
				_reg.set_gpio_async_rising_detect(gpio);
			else
				_reg.set_gpio_rising_detect(gpio);
		}

		void high_detect(unsigned gpio) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }
			if (verbose) Genode::log("high_detect: gpio=", gpio);

			_reg.set_gpio_high_detect(gpio);
		}

		void low_detect(unsigned gpio) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }
			if (verbose) Genode::log("low_detect: gpio=", gpio);

			_reg.set_gpio_low_detect(gpio);
		}

		void irq_enable(unsigned gpio, bool enable) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }
			if (verbose) Genode::log("irq_enable: gpio=", gpio, " enable=", enable);

			_irq_enabled[gpio] = enable;
		}

		void ack_irq(unsigned gpio) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }
			if (verbose) Genode::log("ack_irq: gpio=", gpio);

			_reg.clear_event(gpio);
			_irq.ack_irq();
		}

		void register_signal(unsigned gpio,
		                     Genode::Signal_context_capability cap) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }
			if (verbose) Genode::log("register_signal: gpio=", gpio);

			_sig_cap[gpio] = cap;
		}

		void unregister_signal(unsigned gpio) override
		{
			if (!gpio_valid(gpio)) { _invalid_gpio(gpio); return; }
			if (verbose) Genode::log("unregister_signal: gpio=", gpio);
			Genode::Signal_context_capability cap;

			_sig_cap[gpio] = cap;
		}
};

#endif /* _DRIVERS__GPIO__SPEC__RPI__DRIVER_H_ */
