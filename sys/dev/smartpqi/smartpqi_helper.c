/*-
 * Copyright 2016-2021 Microchip Technology, Inc. and/or its subsidiaries.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $FreeBSD$ */

#include "smartpqi_includes.h"

/* read and modify controller diagnostic option - PQI_PTRAID_UPDATE_ON_RESCAN_LUNS */
void
pqisrc_ctrl_diagnostic_options(pqisrc_softstate_t *softs)
{
	int ret = PQI_STATUS_SUCCESS;
	uint32_t diags_options = 0;
	pqisrc_raid_req_t request;

	DBG_NOTE("IN\n");

	memset(&request, 0, sizeof(request));
	/* read diags options of controller */
	ret =  pqisrc_build_send_raid_request(softs, &request,
					(void*)&diags_options,
					sizeof(diags_options),
					BMIC_SENSE_DIAGS_OPTIONS,
					0, (uint8_t *)RAID_CTLR_LUNID, NULL);
	if (ret != PQI_STATUS_SUCCESS) {
		DBG_WARN("Request failed for BMIC Sense Diags Option command."
			"ret:%d\n",ret);
		return;
	}
	DBG_NOTE("diags options data after read: %#x\n",diags_options);
	diags_options |= PQI_PTRAID_UPDATE_ON_RESCAN_LUNS;
	DBG_NOTE("diags options data to write: %#x\n",diags_options);
	memset(&request, 0, sizeof(request));
	/* write specified diags options to controller */
	ret =  pqisrc_build_send_raid_request(softs, &request,
					(void*)&diags_options,
					sizeof(diags_options),
					BMIC_SET_DIAGS_OPTIONS,
					0, (uint8_t *)RAID_CTLR_LUNID, NULL);
	if (ret != PQI_STATUS_SUCCESS)
		DBG_WARN("Request failed for BMIC Set Diags Option command."
			"ret:%d\n",ret);
#if 0
	diags_options = 0;
	memset(&request, 0, sizeof(request));
	ret =  pqisrc_build_send_raid_request(softs, &request,
					(void*)&diags_options,
					sizeof(diags_options),
					BMIC_SENSE_DIAGS_OPTIONS,
					0, (uint8_t *)RAID_CTLR_LUNID, NULL);
	if (ret != PQI_STATUS_SUCCESS)
		DBG_WARN("Request failed for BMIC Sense Diags Option command."
			"ret:%d\n",ret);
	DBG_NOTE("diags options after re-read: %#x\n",diags_options);
#endif
	DBG_NOTE("OUT\n");
}

/*
 * Function used to validate the adapter health.
 */
boolean_t
pqisrc_ctrl_offline(pqisrc_softstate_t *softs)
{
	DBG_FUNC("IN\n");

	DBG_FUNC("OUT\n");

	return !softs->ctrl_online;
}

/* Function used set/clear legacy INTx bit in Legacy Interrupt INTx
 * mask clear pqi register
 */
void
pqisrc_configure_legacy_intx(pqisrc_softstate_t *softs, boolean_t enable_intx)
{
	uint32_t intx_mask;
	uint32_t *reg_addr = NULL;

	DBG_FUNC("IN\n");

	if (enable_intx)
		reg_addr = &softs->pqi_reg->legacy_intr_mask_clr;
	else
		reg_addr = &softs->pqi_reg->legacy_intr_mask_set;

	intx_mask = PCI_MEM_GET32(softs, reg_addr, PQI_LEGACY_INTR_MASK_CLR);
	intx_mask |= PQISRC_LEGACY_INTX_MASK;
	PCI_MEM_PUT32(softs, reg_addr, PQI_LEGACY_INTR_MASK_CLR ,intx_mask);

	DBG_FUNC("OUT\n");
}

/*
 * Function used to take exposed devices to OS as offline.
 */
void
pqisrc_take_devices_offline(pqisrc_softstate_t *softs)
{
	pqi_scsi_dev_t *device = NULL;
	int i,j;

	DBG_FUNC("IN\n");
	for(i = 0; i < PQI_MAX_DEVICES; i++) {
		for(j = 0; j < PQI_MAX_MULTILUN; j++) {
			if(softs->device_list[i][j] == NULL)
				continue;
			device = softs->device_list[i][j];
			pqisrc_remove_device(softs, device);
		}
	}

	DBG_FUNC("OUT\n");
}

/*
 * Function used to take adapter offline.
 */
void
pqisrc_take_ctrl_offline(pqisrc_softstate_t *softs)
{

	DBG_FUNC("IN\n");

	softs->ctrl_online = false;

	int lockupcode = 0;

	if (SIS_IS_KERNEL_PANIC(softs)) {
                lockupcode = PCI_MEM_GET32(softs, &softs->ioa_reg->mb[7], LEGACY_SIS_SRCV_OFFSET_MAILBOX_7);
                DBG_ERR("Controller FW is not running, Lockup code = %x\n", lockupcode);
        }
        else {
                pqisrc_trigger_nmi_sis(softs);
        }

	os_complete_outstanding_cmds_nodevice(softs);
	pqisrc_wait_for_rescan_complete(softs);
	pqisrc_take_devices_offline(softs);

	DBG_FUNC("OUT\n");
}

/*
 * Timer handler for the adapter heart-beat.
 */
void
pqisrc_heartbeat_timer_handler(pqisrc_softstate_t *softs)
{
	uint8_t take_offline = false;

	DBG_FUNC("IN\n");

	if (CTRLR_HEARTBEAT_CNT(softs) == softs->prev_heartbeat_count) {
		take_offline = true;
		goto take_ctrl_offline;
	}
	softs->prev_heartbeat_count = CTRLR_HEARTBEAT_CNT(softs);
	DBG_INFO("CTRLR_HEARTBEAT_CNT(softs)  = %lx \
		softs->prev_heartbeat_count = %lx\n",
		CTRLR_HEARTBEAT_CNT(softs), softs->prev_heartbeat_count);

take_ctrl_offline:
	if (take_offline){
		DBG_ERR("controller is offline\n");
		os_stop_heartbeat_timer(softs);
		pqisrc_take_ctrl_offline(softs);
	}
	DBG_FUNC("OUT\n");
}

/*
 * Conditional variable management routine for internal commands.
 */
int
pqisrc_wait_on_condition(pqisrc_softstate_t *softs, rcb_t *rcb,
				uint32_t timeout_in_msec)
{
	DBG_FUNC("IN\n");

	int ret = PQI_STATUS_SUCCESS;

	/* 1 msec = 500 usec * 2 */
	uint32_t loop_cnt = timeout_in_msec * 2;
	uint32_t i = 0;

	while (rcb->req_pending == true) {
		OS_SLEEP(500); /* Micro sec */
		/* Polling needed for FreeBSD : since ithread routine is not scheduled
		 * during bootup, we could use polling until interrupts are
		 * enabled (using 'if (cold)'to check for the boot time before
		 * interrupts are enabled). */
		IS_POLLING_REQUIRED(softs);

		if ((timeout_in_msec != TIMEOUT_INFINITE) && (i++ == loop_cnt)) {
			DBG_ERR("ERR: Requested cmd timed out !!!\n");
			ret = PQI_STATUS_TIMEOUT;
			rcb->timedout = true;
			break;
		}

		if (pqisrc_ctrl_offline(softs)) {
			DBG_ERR("Controller is Offline");
			ret = PQI_STATUS_FAILURE;
			break;
		}

	}
	rcb->req_pending = true;

	DBG_FUNC("OUT\n");

	return ret;
}

/* Function used to validate the device wwid. */
boolean_t
pqisrc_device_equal(pqi_scsi_dev_t *dev1,
	pqi_scsi_dev_t *dev2)
{
	return dev1->wwid == dev2->wwid;
}

/* Function used to validate the device scsi3addr. */
boolean_t
pqisrc_scsi3addr_equal(uint8_t *scsi3addr1, uint8_t *scsi3addr2)
{
	return memcmp(scsi3addr1, scsi3addr2, 8) == 0;
}

/* Function used to validate hba_lunid */
boolean_t
pqisrc_is_hba_lunid(uint8_t *scsi3addr)
{
	return pqisrc_scsi3addr_equal(scsi3addr, (uint8_t*)RAID_CTLR_LUNID);
}

/* Function used to validate type of device */
boolean_t
pqisrc_is_logical_device(pqi_scsi_dev_t *device)
{
	return !device->is_physical_device;
}

/* Function used to sanitize inquiry string */
void
pqisrc_sanitize_inquiry_string(unsigned char *s, int len)
{
	boolean_t terminated = false;

	DBG_FUNC("IN\n");

	for (; len > 0; (--len, ++s)) {
		if (*s == 0)
			terminated = true;
		if (terminated || *s < 0x20 || *s > 0x7e)
			*s = ' ';
	}

	DBG_FUNC("OUT\n");
}

static char *raid_levels[] = {
	"RAID 0",
	"RAID 4",
	"RAID 1(1+0)",
	"RAID 5",
	"RAID 5+1",
	"RAID ADG",
	"RAID 1(ADM)",
};

/* Get the RAID level from the index */
char *
pqisrc_raidlevel_to_string(uint8_t raid_level)
{
	DBG_FUNC("IN\n");
	if (raid_level < ARRAY_SIZE(raid_levels))
		return raid_levels[raid_level];
	DBG_FUNC("OUT\n");

	return " ";
}

/* Debug routine for displaying device info */
void
pqisrc_display_device_info(pqisrc_softstate_t *softs,
	char *action, pqi_scsi_dev_t *device)
{
	DBG_INFO( "%s scsi BTL %d:%d:%d:  %.8s %.16s %-12s SSDSmartPathCap%c En%c Exp%c qd=%d\n",
		action,
		device->bus,
		device->target,
		device->lun,
		device->vendor,
		device->model,
		pqisrc_raidlevel_to_string(device->raid_level),
		device->offload_config ? '+' : '-',
		device->offload_enabled_pending ? '+' : '-',
		device->expose_device ? '+' : '-',
		device->queue_depth);
	pqisrc_raidlevel_to_string(device->raid_level); /* To use this function */
}

/* validate the structure sizes */
void
check_struct_sizes(void)
{

    ASSERT(sizeof(SCSI3Addr_struct)== 2);
    ASSERT(sizeof(PhysDevAddr_struct) == 8);
    ASSERT(sizeof(LogDevAddr_struct)== 8);
    ASSERT(sizeof(LUNAddr_struct)==8);
    ASSERT(sizeof(RequestBlock_struct) == 20);
    ASSERT(sizeof(MoreErrInfo_struct)== 8);
    ASSERT(sizeof(ErrorInfo_struct)== 48);
    /* Checking the size of IOCTL_Command_struct for both
       64 bit and 32 bit system*/
    ASSERT(sizeof(IOCTL_Command_struct)== 86 ||
           sizeof(IOCTL_Command_struct)== 82);
    ASSERT(sizeof(struct bmic_host_wellness_driver_version)== 42);
    ASSERT(sizeof(struct bmic_host_wellness_time)== 20);
    ASSERT(sizeof(struct pqi_dev_adminq_cap)== 8);
    ASSERT(sizeof(struct admin_q_param)== 4);
    ASSERT(sizeof(struct pqi_registers)== 256);
    ASSERT(sizeof(struct ioa_registers)== 4128);
    ASSERT(sizeof(struct pqi_pref_settings)==4);
    ASSERT(sizeof(struct pqi_cap)== 20);
    ASSERT(sizeof(iu_header_t)== 4);
    ASSERT(sizeof(gen_adm_req_iu_t)== 64);
    ASSERT(sizeof(gen_adm_resp_iu_t)== 64);
    ASSERT(sizeof(op_q_params) == 9);
    ASSERT(sizeof(raid_path_error_info_elem_t)== 276);
    ASSERT(sizeof(aio_path_error_info_elem_t)== 276);
    ASSERT(sizeof(struct init_base_struct)== 24);
    ASSERT(sizeof(pqi_iu_layer_desc_t)== 16);
    ASSERT(sizeof(pqi_dev_cap_t)== 576);
    ASSERT(sizeof(pqi_aio_req_t)== 128);
    ASSERT(sizeof(pqisrc_raid_req_t)== 128);
    ASSERT(sizeof(pqi_raid_tmf_req_t)== 32);
    ASSERT(sizeof(pqi_aio_tmf_req_t)== 32);
    ASSERT(sizeof(struct pqi_io_response)== 16);
    ASSERT(sizeof(struct sense_header_scsi)== 8);
    ASSERT(sizeof(reportlun_header_t)==8);
    ASSERT(sizeof(reportlun_ext_entry_t)== 24);
    ASSERT(sizeof(reportlun_data_ext_t)== 32);
    ASSERT(sizeof(raidmap_data_t)==8);
    ASSERT(sizeof(pqisrc_raid_map_t)== 8256);
    ASSERT(sizeof(bmic_ident_ctrl_t)== 325);
    ASSERT(sizeof(bmic_ident_physdev_t)==2048);

}

uint32_t
pqisrc_count_num_scsi_active_requests_on_dev(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
	uint32_t i, active_io = 0;
	rcb_t* rcb;

	for(i = 1; i <= softs->max_outstanding_io; i++) {
		rcb = &softs->rcb[i];
		if(rcb && IS_OS_SCSICMD(rcb) && (rcb->dvp == device) && rcb->req_pending) {
			active_io++;
		}
	}
	return active_io;
}

void
check_device_pending_commands_to_complete(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
	uint32_t tag = softs->max_outstanding_io, active_requests;
	uint64_t timeout = 0, delay_in_usec = 1000; //In micro Seconds
	rcb_t* rcb;

	DBG_FUNC("IN\n");

	active_requests = pqisrc_count_num_scsi_active_requests_on_dev(softs, device);

	DBG_WARN("Device Outstanding IO count = %u\n", active_requests);

	if(!active_requests)
		return;

	do {
		rcb = &softs->rcb[tag];
		if(rcb && IS_OS_SCSICMD(rcb) && (rcb->dvp == device) && rcb->req_pending) {
			OS_BUSYWAIT(delay_in_usec);
			timeout += delay_in_usec;
		}
		else
			tag--;
		if(timeout >= PQISRC_PENDING_IO_TIMEOUT_USEC) {
			DBG_WARN("timed out waiting for pending IO\n");
			return;
		}
	} while(tag);

}

inline uint64_t
pqisrc_increment_device_active_io(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
#if PQISRC_DEVICE_IO_COUNTER
	/*Increment device active io count by one*/
	return OS_ATOMIC64_INC(&device->active_requests);
#endif
}

inline uint64_t
pqisrc_decrement_device_active_io(pqisrc_softstate_t *softs,  pqi_scsi_dev_t *device)
{
#if PQISRC_DEVICE_IO_COUNTER
	/*Decrement device active io count by one*/
	return OS_ATOMIC64_DEC(&device->active_requests);
#endif
}

inline void
pqisrc_init_device_active_io(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
#if PQISRC_DEVICE_IO_COUNTER
	/* Reset device count to Zero */
	OS_ATOMIC64_INIT(&device->active_requests, 0);
#endif
}

inline uint64_t
pqisrc_read_device_active_io(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
#if PQISRC_DEVICE_IO_COUNTER
	/* read device active count*/
	return OS_ATOMIC64_READ(&device->active_requests);
#endif
}

void
pqisrc_wait_for_device_commands_to_complete(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
	uint64_t timeout_in_usec = 0, delay_in_usec = 1000; //In microseconds

	DBG_FUNC("IN\n");

	if(!softs->ctrl_online)
		return;

#if PQISRC_DEVICE_IO_COUNTER
	DBG_NOTE("Device Outstanding IO count = %ld\n", pqisrc_read_device_active_io(softs, device));

	while(pqisrc_read_device_active_io(softs, device)) {
		OS_BUSYWAIT(delay_in_usec); // In microseconds
		if(!softs->ctrl_online) {
			DBG_WARN("Controller Offline was detected.\n");
		}
		timeout_in_usec += delay_in_usec;
		if(timeout_in_usec >= PQISRC_PENDING_IO_TIMEOUT_USEC) {
			DBG_WARN("timed out waiting for pending IO. DeviceOutStandingIo's=%ld\n",
                                 pqisrc_read_device_active_io(softs, device));
			return;
		}
	}
#else
	check_device_pending_commands_to_complete(softs, device);
#endif
}
