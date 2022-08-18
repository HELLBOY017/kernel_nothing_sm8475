/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */


#ifndef _MSM_CVP_COMMON_H_
#define _MSM_CVP_COMMON_H_
#include "msm_cvp_internal.h"

void eva_cvp_put_inst(struct msm_cvp_inst *inst);
struct msm_cvp_inst *eva_cvp_get_inst(struct msm_cvp_core *core,
		void *session_id);
struct msm_cvp_inst *eva_cvp_get_inst_validate(struct msm_cvp_core *core,
		void *session_id);
bool is_cvp_inst_valid(struct msm_cvp_inst *inst);
void cvp_change_inst_state(struct msm_cvp_inst *inst,
		enum instance_state state);
struct msm_cvp_core *eva_get_cvp_core(int core_id);
int eva_msm_cvp_comm_try_state(struct msm_cvp_inst *inst, int state);
int eva_msm_cvp_deinit_core(struct msm_cvp_inst *inst);
int eva_msm_cvp_comm_suspend(int core_id);
void eva_msm_cvp_comm_session_clean(struct msm_cvp_inst *inst);
int eva_msm_cvp_comm_kill_session(struct msm_cvp_inst *inst);
void eva_msm_cvp_comm_generate_sys_error(struct msm_cvp_inst *inst);
int msm_cvp_comm_smem_cache_operations(struct msm_cvp_inst *inst,
		struct msm_cvp_smem *mem, enum smem_cache_ops cache_ops);
int eva_msm_cvp_comm_check_core_init(struct msm_cvp_core *core);
int eva_wait_for_sess_signal_receipt(struct msm_cvp_inst *inst,
	enum hal_command_response cmd);
int eva_cvp_comm_set_arp_buffers(struct msm_cvp_inst *inst);
int cvp_comm_release_persist_buffers(struct msm_cvp_inst *inst);
int eva_msm_cvp_noc_error_info(struct msm_cvp_core *core);
#endif
