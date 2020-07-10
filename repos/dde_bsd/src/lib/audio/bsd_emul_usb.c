/*
 * \brief  Audio driver BSD API emulation
 * \author Josef Soentgen
 * \date   2014-11-09
 */

/*
 * Copyright (C) 2014-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <bsd_emul.h>

#include <sys/device.h>
#include <dev/audio_if.h>
#include <dev/usb/usb.h>

/******************
 ** sys/kernel.h **
 ******************/

/* ioconf.c */
extern struct cfdriver audio_cd;
extern struct cfattach audio_ca;
extern struct cfdriver uaudio_cd;
extern struct cfattach uaudio_ca;

short pv[] = { -1, 32 };

struct cfdata cfdata[] = {
	{&audio_ca,  &audio_cd,  0, 0, 0, 0, pv+0, 0, 0},
	{&uaudio_ca, &uaudio_cd, 0, 0, 0, 0, pv+1, 0, 0},
};


/**
 * This function is our little helper that matches and attaches
 * the driver to the device.
 */
int probe_cfdata(struct usb_attach_arg *attach_args)
{
	size_t ncd = sizeof(cfdata) / sizeof(struct cfdata);

	size_t i;
	for (i = 0; i < ncd; i++) {
		struct cfdata *cf = &cfdata[i];
		struct cfdriver *cd = cf->cf_driver;

		struct cfattach *ca = cf->cf_attach;
		if (!ca->ca_match) {
			continue;
		}

		int rv = ca->ca_match(0, 0, attach_args);

		if (rv) {
			struct device *dev =
				(struct device *) malloc(ca->ca_devsize,
				                         M_DEVBUF, M_NOWAIT|M_ZERO);

			dev->dv_cfdata = cf;

			snprintf(dev->dv_xname, sizeof(dev->dv_xname), "%s%d", cd->cd_name,
			         dev->dv_unit);
			ca->ca_attach(0, dev, attach_args);

			return 1;
		}
	}
	return 0;
}
