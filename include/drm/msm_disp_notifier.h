/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */
#ifndef _MSM_DRM_NOTIFY_H_
#define _MSM_DRM_NOTIFY_H_

#include <linux/notifier.h>

/* A hardware display power mode state change occurred */
#define MSM_DISP_DPMS_EVENT             0x01
/* A hardware display power mode state early change occurred */
#define MSM_DISP_DPMS_EARLY_EVENT       0x02

enum {
	/* panel: power on */
	MSM_DISP_DPMS_ON   = 0,
	MSM_DISP_DPMS_LP1       = 1,
	MSM_DISP_DPMS_LP2       = 2,
	MSM_DISP_DPMS_STANDBY   = 3,
	MSM_DISP_DPMS_SUSPEND   = 4,
	/* panel: power off */
	MSM_DISP_DPMS_POWERDOWN = 5,
};

enum msm_disp_id {
	/* primary display */
	MSM_DISPLAY_PRIMARY = 0,
	/* external display */
	MSM_DISPLAY_SECONDARY,
	MSM_DISPLAY_MAX,
};

struct msm_disp_notifier {
	int disp_id;
	void *data;
};

#if IS_ENABLED(CONFIG_DRM)
int msm_disp_register_client(struct notifier_block *nb);
int msm_disp_unregister_client(struct notifier_block *nb);
int msm_disp_notifier_call_chain(unsigned long val, void *v);
#else
static inline int msm_disp_register_client(struct notifier_block *nb)
{
	return 0;
}

static inline int msm_disp_unregister_client(struct notifier_block *nb)
{
	return 0;
}
static inline int msm_disp_notifier_call_chain(unsigned long val, void *v)
{
	return 0;
}
#endif
#endif
