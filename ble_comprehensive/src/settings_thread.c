/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <string.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME settings_thread
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

static uint32_t reboot_cnt = 0;
static uint8_t key_s[8];

static int alpha_handle_set(const char *name, size_t len, settings_read_cb read_cb,
					 void *cb_arg)
{
	const char *next;

	LOG_INF("set handler name=%s, len=%d ", (name), len);
	if (settings_name_steq(name, "boot_cnt", &next) && !next)
	{
		if (len != sizeof(reboot_cnt))
		{
			return -EINVAL;
		}
		read_cb(cb_arg, &reboot_cnt, sizeof(reboot_cnt));
		LOG_INF("*** Reboot counter in Settings: %d ****", reboot_cnt);
		return 0;
	}

	if (settings_name_steq(name, "key", &next) && !next)
	{
		if (len != sizeof(key_s))
		{
			return -EINVAL;
		}
		read_cb(cb_arg, key_s, sizeof(key_s));
		LOG_HEXDUMP_INF(key_s, sizeof(key_s), "Key value in Settings:");
		return 0;
	}

	return -ENOENT;
}

/* dynamic main tree handler */
static struct settings_handler alph_handler = {
	.name = "alpha",
	.h_get = NULL,
	.h_set = alpha_handle_set,
	.h_commit = NULL,
	.h_export = NULL};

static int settings_usage_init(void)
{
	int rc;

	rc = settings_subsys_init();
	if (rc)
	{
		LOG_ERR("settings subsys initialization: fail (err %d) ", rc);
		return rc;
	}

	LOG_INF("settings subsys initialization: OK.");

	rc = settings_register(&alph_handler);
	if (rc)
	{
		LOG_ERR("subtree <%s> handler registered: fail (err %d)",
				alph_handler.name, rc);
		return rc;		
	}

	/* load all key-values at once
	 * In case a key-value doesn't exist in the storage
	 * default valuse should be assigned to settings variable
	 * before any settings load call
	 */
	LOG_INF("Load all key-value pairs using registered handlers");
	return settings_load();

}

static int flash_access(void)
{
	int rc;

	if (reboot_cnt == 0)
	{
		LOG_INF("save key_s By Settings API");
		key_s[0] = 0x30;
		key_s[1] = 0x31;
		key_s[2] = 0x32;
		key_s[3] = 0x33;
		key_s[4] = 0x34;
		key_s[5] = 0x35;
		key_s[6] = 0x36;
		key_s[7] = 0x37;
		rc = settings_save_one("alpha/key", (const void *)key_s,
							   sizeof(key_s));
		if (rc)
		{
			LOG_ERR("key_s save err %d ", rc);
			return rc;
		}
	}

	LOG_INF("save new reboot counter by Settings API");
	reboot_cnt++;
	rc = settings_save_one("alpha/boot_cnt", (const void *)&reboot_cnt,
						   sizeof(reboot_cnt));
	if (rc)
	{
		LOG_ERR("reboot_cnt save err %d ", rc);
	}

	return rc;
}

static void erase_secondary_slot(void)
{
	const struct flash_area *fa;
	int rc;

	rc = flash_area_open(PM_MCUBOOT_SECONDARY_ID, &fa);
	if (rc) {
		LOG_ERR("flash_area_open err:%d", rc);
		return;
	}

	LOG_INF("Erasing MCUboot secondary slot...");	
	rc = flash_area_erase(fa, 0, PM_MCUBOOT_SECONDARY_SIZE);   
	if (rc) {
		LOG_ERR("flash_area_erase err:%d", rc);
		return;
	}		

}

void settings_thread(void)
{
	static bool operation;
	int rc;

	LOG_INF("**Flash access example using settings APIs");

	rc = settings_usage_init();
	if (rc)
	{
		LOG_ERR("settings init failed %d", rc);
		return;
	}
	operation = true;

	while (1)
	{
		LOG_INF("Settings thread");
		
		if (operation)
		{
			operation = false;
			flash_access();
			// erase_secondary_slot();
		}

		k_sleep(K_SECONDS(20));
	}
}

K_THREAD_DEFINE(settings_thread_id, 1024, settings_thread, NULL, NULL,
				NULL, 4, 0, 0);