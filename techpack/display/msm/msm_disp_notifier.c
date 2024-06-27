/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */
#include <drm/msm_disp_notifier.h>
#include <linux/notifier.h>

static BLOCKING_NOTIFIER_HEAD(msm_disp_notifier_list);

/**
 * msm_drm_register_client - register a client notifier
 * @nb: notifier block to callback on events
 *
 * This function registers a notifier callback function
 * to msm_drm_notifier_list, which would be called when
 * received unblank/power down event.
 */
int msm_disp_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&msm_disp_notifier_list, nb);
}
EXPORT_SYMBOL(msm_disp_register_client);

/**
 * msm_drm_unregister_client - unregister a client notifier
 * @nb: notifier block to callback on events
 *
 * This function unregisters the callback function from
 * msm_drm_notifier_list.
 */
int msm_disp_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&msm_disp_notifier_list, nb);
}
EXPORT_SYMBOL(msm_disp_unregister_client);

/**
 * msm_drm_notifier_call_chain - notify clients of drm_events
 * @val: event MSM_DISP_DPMS_EARLY_EVENT or MSM_DISP_DPMS_EVENT
 * @v: notifier data, inculde display id and display blank
 *     event(unblank or power down).
 */
int msm_disp_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&msm_disp_notifier_list, val, v);
}
EXPORT_SYMBOL(msm_disp_notifier_call_chain);
