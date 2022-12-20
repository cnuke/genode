/*
 * \brief  XML configuration for ATH9k wireless driver
 * \author Colin Parker
 * \date   2022-12-10
 */

/*
 * Copyright (C) 2022 Colin Parker
 * Based on "wifi_drv.cc" by Norman Feske
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <runtime.h>

void Sculpt::gen_wifi_drv_start_content(Xml_generator &xml)
{
	gen_common_start_content(xml, "wifi_drv",
	                         Cap_quota{250}, Ram_quota{64*1024*1024},
	                         Priority::NETWORK);

	xml.node("binary", [&] () {
		xml.attribute("name", "wifi_ath9k_drv");
	});

	xml.node("config", [&] () {

		xml.node("vfs", [&] () {
			gen_named_node(xml, "dir", "dev", [&] () {
				xml.node("null", [&] () {});
				xml.node("zero", [&] () {});
				xml.node("log",  [&] () {});
				xml.node("wifi_ath9k", [&] () {});
				gen_named_node(xml, "jitterentropy", "random");
				gen_named_node(xml, "jitterentropy", "urandom");
				gen_named_node(xml, "inline", "rtc", [&] () {
					xml.append("2018-01-01 00:01");
				});
			});
		});

		xml.node("libc", [&] () {
			xml.attribute("stdout", "/dev/log");
			xml.attribute("stderr", "/dev/log");
			xml.attribute("rtc",    "/dev/rtc");
		});
	});

	xml.node("route", [&] () {

		xml.node("service", [&] () {
			xml.attribute("name", "Uplink");
			xml.node("child", [&] () {
				xml.attribute("name", "nic_router");
				xml.attribute("label", "wifi_drv -> ");
			});
		});

		gen_service_node<Platform::Session>(xml, [&] () {
			xml.node("parent", [&] () {
				xml.attribute("label", "usb"); }); });

		gen_parent_rom_route(xml, "wifi_ath9k_drv");
		gen_parent_rom_route(xml, "htc_9271-1.4.0.fw");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "libcrypto.lib.so");
		gen_parent_rom_route(xml, "vfs.lib.so");
		gen_parent_rom_route(xml, "libc.lib.so");
		gen_parent_rom_route(xml, "libm.lib.so");
		gen_parent_rom_route(xml, "vfs_jitterentropy.lib.so");
		gen_parent_rom_route(xml, "vfs_wifi_ath9k.lib.so");
		gen_parent_rom_route(xml, "libssl.lib.so");
		gen_parent_rom_route(xml, "wifi_ath9k.lib.so");
		gen_parent_rom_route(xml, "wpa_driver_nl80211.lib.so");
		gen_parent_rom_route(xml, "wpa_supplicant.lib.so");
		gen_parent_rom_route(xml, "regulatory.db");
		gen_parent_rom_route(xml, "regulatory.db.p7s");
		gen_parent_route<Cpu_session>      (xml);
		gen_parent_route<Pd_session>       (xml);
		gen_parent_route<Rm_session>       (xml);
		gen_parent_route<Log_session>      (xml);
		gen_parent_route<Timer::Session>   (xml);
		gen_parent_route<Rtc::Session>     (xml);
		gen_parent_route<Report::Session>  (xml);
		gen_parent_route<Usb::Session>     (xml);

		gen_service_node<Rom_session>(xml, [&] () {
			xml.attribute("label", "wifi_config");
			xml.node("parent", [&] () {
				xml.attribute("label", "config -> managed/wifi"); }); });
	});
}
