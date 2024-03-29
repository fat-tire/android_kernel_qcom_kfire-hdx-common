/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MSM_THERMAL_H
#define __MSM_THERMAL_H

struct msm_thermal_data {
	uint32_t sensor_id;
	uint32_t poll_ms;
	int32_t limit_temp_degC;
	int32_t temp_hysteresis_degC;
       uint32_t bootup_freq_step;
       uint32_t bootup_freq_control_mask;
	int32_t core_limit_temp_degC;
	int32_t core_temp_hysteresis_degC;
	int32_t hotplug_temp_degC;
	int32_t hotplug_temp_hysteresis_degC;
	uint32_t core_control_mask;
       uint32_t freq_mitig_temp_degc;
       uint32_t freq_mitig_temp_hysteresis_degc;
       uint32_t freq_mitig_control_mask;
       uint32_t freq_mitig_value;
	int32_t vdd_rstr_temp_degC;
	int32_t vdd_rstr_temp_hyst_degC;
	int32_t psm_temp_degC;
	int32_t psm_temp_hyst_degC;
};

#ifdef CONFIG_THERMAL_MONITOR
extern int msm_thermal_init(struct msm_thermal_data *pdata);
extern int msm_thermal_device_init(void);
#else
static inline int msm_thermal_init(struct msm_thermal_data *pdata)
{
	return -ENOSYS;
}
static inline int msm_thermal_device_init(void)
{
	return -ENOSYS;
}
#endif

#ifdef CONFIG_THERMAL_MONITOR_DEV_INTERFACE
#include <linux/msm_thermal_ioctl.h>
extern int msm_thermal_ioctl_init(void);
extern void msm_thermal_ioctl_cleanup(void);
int msm_thermal_set_frequency(struct cpu_freq_arg inp_req,
       bool max_freq);
#else
static inline int msm_thermal_ioctl_init(void)
{
       return -ENOSYS;
}
static inline void msm_thermal_ioctl_cleanup(void)
{
       return;
}
#endif


#endif /*__MSM_THERMAL_H*/
