/*
 * Copyright (C)  2015. Marvell International Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <hardware/lights.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <utils/Log.h>
#include <hardware/hardware.h>

#define LOG_TAG "marvl-lights"
#define LED_PATH(path, num, node)  \
    sprintf(path, "/sys/class/leds/usr%d/%s", num, node)

const char* const LCD_BACKLIGHT = "/sys/class/backlight/lcd-bl/brightness";
const char* const LCD_MAX_BACKLIGHT = "/sys/class/backlight/lcd-bl/max_brightness";
const char* const KEYBOARD_BACKLIGHT = "";
const char* const BUTTON_BACKLIGHT = "/sys/class/leds/button-backlight/brightness";
const char* const BATTERY_BACKLIGHT = "";
const char* const ATTENTION_BACKLIGHT = "";

static int lights_device_open ( const struct hw_module_t* module,
    const char* name, struct hw_device_t** device );

static struct hw_module_methods_t lights_module_methods = {
    .open = lights_device_open
};

struct light_module_t {
    struct hw_module_t common;
};

struct light_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag : HARDWARE_MODULE_TAG,
        version_major : 1,
        version_minor : 0,
        id : LIGHTS_HARDWARE_MODULE_ID,
        name : "lights module",
        author : "Marvell SEEDS",
        methods : &lights_module_methods,
    }
};

struct my_light_device_t {
    struct light_device_t dev;
    int max_brightness;
};

static int read_from_file(const char* const file_name, int* result)
{
    FILE* fp = NULL;

    fp = fopen(file_name, "r");
    if (!fp) {
        ALOGE("fail to open file: %s", file_name);
        return -1;
    }

    fscanf(fp, "%d", result);
    fclose(fp);

    return 0;
}

static int write_to_file(const char* const file_name, int value)
{
    FILE* fp = NULL;

#ifdef LIGHTS_DEBUG
    ALOGD("write %d to %s\n", value, file_name);
#endif

    fp = fopen(file_name, "w");
    if (!fp) {
        ALOGE("fail to open file: %s", file_name);
        return -1;
    }

    fprintf(fp, "%d", value);
    fclose(fp);

    return 0;
}

static unsigned char get_color_b(struct light_device_t* dev,
                                 struct light_state_t const* state)
{
    struct my_light_device_t* mydev = (struct my_light_device_t*)dev;
    unsigned char color = 0xff;

    if (state)
        color = (state->color & 0xff) * mydev->max_brightness / 0xff;

    return color;
}

static unsigned char get_color_r(struct light_device_t* dev,
                                 struct light_state_t const* state)
{
    struct my_light_device_t* mydev = (struct my_light_device_t*)dev;
    unsigned char color = 0xff;

    if (state)
        color = ((state->color >> 16) & 0xff) * mydev->max_brightness / 0xff;

    return color;
}

static unsigned char get_color_g(struct light_device_t* dev,
                          struct light_state_t const* state)
{
    struct my_light_device_t* mydev = (struct my_light_device_t*)dev;
    unsigned char color = 0xff;

    if (state)
        color = ((state->color >> 8) & 0xff) * mydev->max_brightness / 0xff;

    return color;
}

static int get_flash_on_ms(struct light_device_t* dev,
                           struct light_state_t const* state)
{
    struct my_light_device_t* mydev = (struct my_light_device_t*)dev;
    int flash_on_ms = 0;

    if (state)
        flash_on_ms = state->flashOnMS;

    return flash_on_ms;
}

static int get_flash_off_ms(struct light_device_t* dev,
                            struct light_state_t const* state)
{
    struct my_light_device_t* mydev = (struct my_light_device_t*)dev;
    int flash_off_ms = 0;

    if (state)
        flash_off_ms = state->flashOffMS;

    return flash_off_ms;
}

static int get_flash_mode(struct light_device_t* dev,
                          struct light_state_t const* state)
{
    struct my_light_device_t* mydev = (struct my_light_device_t*)dev;
    int flash_mode = 0;

    if (state)
        flash_mode = state->flashMode;

    return flash_mode;
}

static int lights_set_lcd(struct light_device_t* dev,
                          struct light_state_t const* state)
{
    unsigned char brightness = get_color_b(dev, state);

    write_to_file(LCD_BACKLIGHT, brightness);
#ifdef BUTTONS_VIA_LCD
    lights_set_button(dev, state);
#endif

    return 0;
}

static int lights_set_keyboard(struct light_device_t* dev,
                               struct light_state_t const* state)
{
    return 0;
}

static int lights_set_button(struct light_device_t* dev,
                             struct light_state_t const* state)
{
    unsigned char brightness = get_color_b(dev, state);

    write_to_file(BUTTON_BACKLIGHT, brightness);

    return 0;
}

static int lights_set_battery(struct light_device_t* dev,
                              struct light_state_t const* state)
{
    return 0;
}

static int lights_set_notifications(struct light_device_t* dev,
                                    struct light_state_t const* state)
{
    int flash_on_ms = get_flash_on_ms(dev, state);
    int flash_off_ms = get_flash_off_ms(dev, state);
    int flash_mode = get_flash_mode(dev, state);

#ifdef LIGHTS_DEBUG
    ALOGD("state: color: %x, flashOnMS: %d, flashOffMS: %d, flashMode: %d\n",
        state->color, state->flashOnMS, state->flashOffMS, state->flashMode);
#endif

    char path[64];
    unsigned char brightness[3];
    int i;

    brightness[0] = get_color_g(dev, state);
    brightness[1] = get_color_b(dev, state);
    brightness[2] = get_color_r(dev, state);
    for (i = 0; i < 3; i++) {
        LED_PATH(path, i, "led_mode");
        write_to_file(path, flash_mode);
        LED_PATH(path, i, "brightness");
        write_to_file(path, brightness[i]);
        LED_PATH(path, i, "delay_on");
        write_to_file(path, flash_on_ms);
        LED_PATH(path, i, "delay_off");
        write_to_file(path, flash_off_ms);
    }

    return 0;
}

static int lights_set_attention(struct light_device_t* dev,
                                struct light_state_t const* state)
{
    return 0;
}

static int lights_device_close(struct hw_device_t* dev)
{
    struct my_light_device_t* light_dev = (struct my_light_device_t*)dev;

    if (light_dev)
        free(light_dev);

    return 0;
}

static int lights_device_open(const struct hw_module_t* module,
                              const char* name, struct hw_device_t** device)
{
    struct light_device_t* dev = NULL;
    struct my_light_device_t* mydev = NULL;

    mydev = (struct my_light_device_t*)calloc(
                sizeof(struct my_light_device_t), 1);
    dev = &(mydev->dev);
    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*)(module);
    dev->common.close = lights_device_close;
    mydev->max_brightness = 0x64;    // default max brightness is 100
    *device = &dev->common;

#ifdef LIGHTS_DEBUG
    ALOGD("lights_device_open: device name: %s\n", name);
#endif

    int ret = 0;
    if (!strcmp(name, LIGHT_ID_BACKLIGHT)) {
        dev->set_light = lights_set_lcd;
        read_from_file(LCD_MAX_BACKLIGHT, &mydev->max_brightness);
        ret = access(LCD_BACKLIGHT, F_OK);
    } else if (!strcmp(name, LIGHT_ID_KEYBOARD)) {
        dev->set_light = lights_set_keyboard;
        ret = access(KEYBOARD_BACKLIGHT, F_OK);
    } else if (!strcmp(name, LIGHT_ID_BUTTONS)) {
        dev->set_light = lights_set_button;
        ret = access(BUTTON_BACKLIGHT, F_OK);
    } else if (!strcmp(name, LIGHT_ID_BATTERY)) {
        dev->set_light = lights_set_battery;
        ret = access(BATTERY_BACKLIGHT, F_OK);
    } else if (!strcmp(name, LIGHT_ID_NOTIFICATIONS)) {
        char path[64];
        dev->set_light = lights_set_notifications;
        LED_PATH(path, 0, "brightness");
        ret = access(path, F_OK);
    } else if (!strcmp(name, LIGHT_ID_ATTENTION)) {
        dev->set_light = lights_set_attention;
        ret = access(ATTENTION_BACKLIGHT, F_OK);
    } else {
        ALOGE("lights_device_open: invalid %s\n", name);
        ret = -1;
    }

    return ret;
}
