
#
# Generate fio start node
#
proc fio_config { name type engine depth blocksize jobs runtime filename size singlecpu rwmix} {

	set    config "<start name=\"fio\" caps=\"500\">\n"
	append config "  <resource name=\"RAM\" quantum=\"1G\"/>\n"
	append config "  <config>\n"
	append config "    <arg value=\"fio\"/>\n"
	append config "    <arg value=\"--randrepeat=1\"/>\n"
	append config "    <arg value=\"--rw=$type\"/>\n"
	append config "    <arg value=\"--name=$name\"/>\n"
	append config "    <arg value=\"--bs=$blocksize\"/>\n"
	append config "    <arg value=\"--direct=1\"/>\n"
	if {$size != 0} {
	append config "    <arg value=\"--size=$size\"/>\n"
	}
	if {$rwmix != 0} {
	append config "    <arg value=\"--rwmixread=$rwmix\"/>\n"
	}
	append config "    <arg value=\"--filename=$filename\"/>\n"
	append config "    <arg value=\"--numjobs=$jobs\"/>\n"
	append config "    <arg value=\"--ioengine=$engine\"/>\n"
	append config "    <arg value=\"--iodepth=$depth\"/>\n"
	append config "    <arg value=\"--runtime=$runtime\"/>\n"
	append config "    <arg value=\"--refill_buffers\"/>\n"
	append config "    <arg value=\"--group_reporting\"/>\n"
	append config "    <arg value=\"--time_based\"/>\n"
	append config "    <vfs>\n"
	append config "      <dir name=\"dev\">\n"
	append config "        <null/> <zero/> <log/>\n"
	append config "        <inline name=\"rtc\">2025-03-25 12:00</inline>\n"
	append config "        <jitterentropy name=\"random\"/>\n"
	if {$filename != "/fiofile"} {
	append config "        <block/>\n"
	}
	append config "        <dir name=\"pipe\"> <pipe/> </dir>\n"
	append config "      </dir>\n"
	append config "      <ram/>\n"
	append config "    </vfs>\n"
	append config "    <libc stdin=\"/dev/null\" stdout=\"/dev/log\" stderr=\"/dev/null\"\n"
	append config "          rtc=\"/dev/rtc\" pipe=\"/dev/pipe\">\n"
	if {$singlecpu} {
	append config "      <pthread placement=\"single-cpu\"/>\n"
	}
	append config "    </libc>\n"
	append config "  </config>\n"
	append config "</start>\n"

	return $config
}

set fiofile_size 0

#
#  Query filename for test
#
proc fio_filename { } {

	global use_vfs_ram fiofile_size

	if {$use_vfs_ram} {
		set fiofile_size 64M
		return /fiofile
	}
	return /dev/block
}

proc use_singlecpu { } {

	if {[info exists ::env(GENODE_FIO_SINGLECPU)]} {
		return true
	}
	return false
}

#
# Build
#
append build_components {
	core init timer lib/ld
	server/report_rom
	app/pci_decode
	driver/acpi
	driver/platform
	server/part_block
	lib/vfs
	lib/libc
	app/sequence
	app/top
}

build $build_components

create_boot_directory

import_from_depot cnuke/pkg/fio/3.39-25.05-2025-07-31


#
# Generate config
#
append config {
<config verbose="no">
	<parent-provides>
		<service name="ROM"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="CPU"/>
		<service name="LOG"/>
		<service name="TRACE"/>
	</parent-provides>

	<affinity-space width="4" height="2"/>

	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>

	<default caps="100"/>

	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Timer"/></provides>
	</start>

	<!--
	<start name="top">
		<resource name="RAM" quantum="1M"/>
		<config period_ms="10000"/>
		<route>
			<service name="Timer"> <child name="timer"/> </service>
			<service name="TRACE"> <parent label=""/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>
	-->

	<start name="report_rom">
		<resource name="RAM" quantum="2M"/>
		<provides> <service name="Report"/> <service name="ROM"/> </provides>
		<config>
			<policy label="pci_decode -> system" report="acpi -> acpi"/>
			<policy label="platform -> devices"  report="pci_decode -> devices"/>
		</config>
	</start>

	<start name="acpi" caps="350">
		<resource name="RAM" quantum="4M"/>
		<route>
			<service name="Report"> <child name="report_rom"/> </service>
			<service name="IO_MEM"> <parent/> </service>
			<service name="LOG">    <parent/> </service>
			<service name="PD">     <parent/> </service>
			<service name="RM">     <parent/> </service>
			<service name="CPU">    <parent/> </service>
			<service name="ROM">    <parent/> </service>
		</route>
	</start>

	<start name="pci_decode" caps="350">
		<resource name="RAM" quantum="2M"/>
		<route>
			<service name="Report">             <child name="report_rom"/> </service>
			<service name="ROM" label="system"> <child name="report_rom"/> </service>
			<service name="IO_MEM"> <parent/> </service>
			<service name="LOG">    <parent/> </service>
			<service name="PD">     <parent/> </service>
			<service name="RM">     <parent/> </service>
			<service name="CPU">    <parent/> </service>
			<service name="ROM">    <parent/> </service>
		</route>
	</start>

	<start name="platform" caps="100" managing_system="yes">
		<resource name="RAM" quantum="2M"/>
		<provides>
			<service name="Platform"/>
		</provides>
		<route>
			<service name="ROM" label="devices"> <child name="report_rom"/> </service>
			<service name="Report"> <child name="report_rom"/> </service>
			<service name="IRQ">     <parent/> </service>
			<service name="IO_MEM">  <parent/> </service>
			<service name="IO_PORT"> <parent/> </service>
			<service name="ROM">     <parent/> </service>
			<service name="PD">      <parent/> </service>
			<service name="CPU">     <parent/> </service>
			<service name="LOG">     <parent/> </service>
			<service name="Timer"> <child name="timer"/> </service>
		</route>
		<config>
			<report devices="yes"/>
			<policy label="nvme -> "> <pci class="NVME"/> </policy>
			<policy label="ahci -> "> <pci class="AHCI"/> </policy>
		</config>
	</start>}

append config [block_provider]

append_if $use_part_block config {
	<start name="part_block">
		<resource name="RAM" quantum="10M"/>
		<provides><service name="Block"/></provides>
		<config>
			<report partitions="yes"/>
			<policy label_prefix="sequence" partition="} [block_partition] {" writeable="yes"/>
		</config>
		<route>
			<service name="Block"> <child name="} [block_provider_name] {"/> </service>
			<any-service><parent/><any-child/></any-service>
		</route>
	</start>}

append config {
	<start name="sequence" caps="1000">
		<resource name="RAM" quantum="1200M"/>
		<affinity xpos="2" ypos="0" width="2" height="1"/>
		<config>
} [fio_config aio-iops-randread        randread  posixaio 64    4k 4 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config aio-iops-randwrite       randwrite posixaio 64    4k 4 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config aio-iops-read            read      posixaio 64    4k 1 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config aio-iops-write           write     posixaio 64    4k 1 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config aio-throughput-read      read      posixaio 64 1024k 4 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config aio-throughput-write     write     posixaio 64 1024k 4 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config aio-latency-read         read      posixaio 1     4k 1 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config aio-latency-write        write     posixaio 1     4k 1 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config aio-latency-randread     randread  posixaio 1     4k 1 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config aio-latency-randwrite    randwrite posixaio 1     4k 1 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config aio-latency-mixed-randrw randrw    posixaio 1     4k 1 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config aio-iops-mixed-randrw    randrw    posixaio 64    4k 1 30 [fio_filename] $fiofile_size [use_singlecpu] 75] {

} [fio_config sync-iops-randread        randread  sync 64    4k 4 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config sync-iops-randwrite       randwrite sync 64    4k 4 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config sync-iops-read            read      sync 64    4k 1 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config sync-iops-write           write     sync 64    4k 1 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config sync-throughput-read      read      sync 64 1024k 4 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config sync-throughput-write     write     sync 64 1024k 4 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config sync-latency-read         read      sync 1     4k 1 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config sync-latency-write        write     sync 1     4k 1 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config sync-latency-randread     randread  sync 1     4k 1 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config sync-latency-randwrite    randwrite sync 1     4k 1 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config sync-latency-mixed-randrw randrw    sync 1     4k 1 30 [fio_filename] $fiofile_size [use_singlecpu]  0] {
} [fio_config sync-iops-mixed-randrw    randrw    sync 64    4k 1 30 [fio_filename] $fiofile_size [use_singlecpu] 75] {
		</config>
		<route>
			<service name="Block"> <child name="part_block"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>
</config>}

install_config $config

build_boot_image [build_artifacts]
