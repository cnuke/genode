create_boot_directory
import_from_depot [depot_user]/src/[base_src] \
                  [depot_user]/pkg/drivers_managed-[board] \
                  [depot_user]/src/init \
                  [depot_user]/src/report_rom
append build_components {
	drivers/gpu/etnaviv
}

append config {
<config verbose="yes">
	<parent-provides>
		<service name="ROM"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="CPU"/>
		<service name="LOG"/>
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<default caps="100"/>

	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Timer"/></provides>
		<route>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="report_rom">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Report"/> <service name="ROM"/> </provides>
		<config verbose="no"/>
		<route>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="drivers" caps="2500" managing_system="yes">
		<resource name="RAM" quantum="256M"/>
		<binary name="init"/>
		<provides> <service name="Platform"/> </provides>
		<route>
			<service name="ROM" label="config"> <parent label="drivers.config"/> </service>
			<service name="Timer">   <child name="timer"/> </service>
			<service name="Report">  <child name="report_rom"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="imx8q_gpu_drv" caps="150">
		<!--
		 -->
		<resource name="RAM" quantum="64M"/>
		<provides> <service name="Drm"/> </provides>
		<route>
			<service name="Timer"> <child name="timer"/> </service>
			<service name="Platform"> <child name="drivers"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>}

append boot_modules {
	imx8q_gpu_drv
	libc.lib.so vfs.lib.so
	libdrm-etnaviv.lib.so
}
