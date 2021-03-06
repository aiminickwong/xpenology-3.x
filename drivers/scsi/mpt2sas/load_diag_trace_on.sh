#!/bin/bash
#
# load_diag_trace_on.sh : loads driver with trace buffer turned on
#

#shopt -o -s xtrace

# global parameters
declare -r CURRENT_PATH=$PWD
declare -r SCSI_HOST="/sys/class/scsi_host"
declare -r FILE_NAME="trace_buffer"

# enable scanning debuging
sysctl -w dev.scsi.logging_level=0x1C0

# turn on KERN_DEBUG messages
echo -7 > /proc/sysrq-trigger

# loading scsi mid layer and transports
modprobe sg
modprobe sd_mod
modprobe scsi_transport_sas
modprobe raid_class

# loading the driver
insmod mpt2sas.ko logging_level=0x210 diag_buffer_enable=1

if [ $? -ne 0 ] ; then
	printf "ERROR: insmod failed !!!\n"
	exit 1
fi;

DRIVER_LOAD_STATUS=$(lsmod | grep mpt2sas)
if [ -z DRIVER_LOAD_STATUS ]; then
	printf "ERROR: driver didn't load !!!\n"
	exit 1
fi

# run multiple instances of the trace buffer script per IOC
for HOST in $(ls ${SCSI_HOST});  do
	cd ${SCSI_HOST}/${HOST};
	if [ `cat proc_name` != "mpt2sas" ]; then
		continue;
	fi;
	host_number=${HOST//host/}
	cd ${CURRENT_PATH}
	trace_buffer/start_tb.sh ${host_number} &
done;

cd ${CURRENT_PATH}
exit 0
