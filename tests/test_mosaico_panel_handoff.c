// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>
#include "sdk.h"
uint32_t mock_handoff;
static esp_lcd_panel_t mock_panel;
static const co5300_vendor_config_t *vendor;
static unsigned resets, inits, ons, dels, commands[256];
static bool v1_0 = true;
static unsigned fail_create, fail_tx;
static int reset_gpio;

bool mosaico_hardware_is_v1_0(void) { return v1_0; }
static esp_err_t reset(esp_lcd_panel_t *p) { assert(p == &mock_panel); ++resets; return ESP_OK; }
static esp_err_t init(esp_lcd_panel_t *p)
{
    assert(p == &mock_panel);
    ++inits;
    for (size_t i = 0; i < vendor->init_cmds_size; ++i) commands[vendor->init_cmds[i].cmd]++;
    return ESP_OK;
}
static esp_err_t on(esp_lcd_panel_t *p, bool value)
{ assert(p == &mock_panel && value); ++ons; return ESP_OK; }
static esp_err_t del(esp_lcd_panel_t *p) { assert(p == &mock_panel); ++dels; return ESP_OK; }
esp_err_t esp_lcd_new_panel_co5300(esp_lcd_panel_io_handle_t io,
    const esp_lcd_panel_dev_config_t *cfg, esp_lcd_panel_handle_t *out)
{
    assert(io && cfg && out);
    if (fail_create) { --fail_create; return ESP_FAIL; }
    reset_gpio = cfg->reset_gpio_num;
    vendor = cfg->vendor_config;
    mock_panel = (esp_lcd_panel_t){reset, init, on, del};
    *out = &mock_panel;
    return ESP_OK;
}
esp_err_t esp_lcd_panel_co5300_set_brightness(esp_lcd_panel_handle_t p, uint8_t value)
{ (void)p; (void)value; return ESP_OK; }
esp_err_t esp_lcd_panel_io_tx_param(esp_lcd_panel_io_handle_t io, int cmd,
    const void *data, size_t bytes)
{
    assert(io && ((unsigned)cmd >> 24) == 2);
    assert(data || !bytes);
    if (fail_tx) { --fail_tx; return ESP_FAIL; }
    ++commands[(cmd >> 8) & 255];
    return ESP_OK;
}
esp_err_t esp_lcd_touch_new_i2c_cst9220(esp_lcd_panel_io_handle_t io,
    const esp_lcd_touch_config_t *cfg, esp_lcd_touch_handle_t *out)
{ (void)io; (void)cfg; (void)out; return ESP_OK; }
extern esp_err_t lcd_panel_factory_entry_t(esp_lcd_panel_io_handle_t,
    const esp_lcd_panel_dev_config_t *, esp_lcd_panel_handle_t *);

int main(void)
{
    esp_lcd_panel_dev_config_t cfg = {.reset_gpio_num = 42};
    esp_lcd_panel_handle_t panel = NULL;
    void *io = &cfg;
    mock_handoff = UINT32_C(0x4D4C4344);
    assert(lcd_panel_factory_entry_t(io, &cfg, &panel) == ESP_OK);
    assert(mock_handoff == 0 && reset_gpio == 42);
    assert(panel->reset(panel) == ESP_OK && resets == 0);
    assert(panel->init(panel) == ESP_OK && inits == 0);
    assert(commands[0x3a] == 1 && commands[0x11] == 0 &&
           commands[0x29] == 0 && commands[0x51] == 0);
    assert(panel->disp_on_off(panel, true) == ESP_OK && ons == 0);
    assert(panel->disp_on_off(panel, true) == ESP_OK && ons == 1);
    /* A subsequent hardware reset restores Sleep Out and brightness: the
     * first-boot adoption table must not replace lifetime initialization. */
    assert(panel->reset(panel) == ESP_OK && resets == 1);
    assert(panel->init(panel) == ESP_OK && inits == 1);
    assert(commands[0x11] == 1 && commands[0x29] == 1 && commands[0x51] == 1);
    assert(panel->del(panel) == ESP_OK && dels == 1);

    v1_0 = false;
    assert(lcd_panel_factory_entry_t(io, &cfg, &panel) == ESP_OK && reset_gpio == 44);
    assert(panel->reset(panel) == ESP_OK && resets == 2);
    assert(panel->init(panel) == ESP_OK && inits == 2);
    assert(panel->disp_on_off(panel, true) == ESP_OK && ons == 2);
    assert(panel->del(panel) == ESP_OK);

    mock_handoff = UINT32_C(0x4D4C4344);
    fail_create = 1;
    assert(lcd_panel_factory_entry_t(io, &cfg, &panel) == ESP_FAIL);
    assert(panel == NULL && mock_handoff == 0);
    assert(lcd_panel_factory_entry_t(io, &cfg, &panel) == ESP_OK);
    assert(panel->reset(panel) == ESP_OK && resets == 3);
    assert(panel->del(panel) == ESP_OK);

    mock_handoff = UINT32_C(0x4D4C4344);
    assert(lcd_panel_factory_entry_t(io, &cfg, &panel) == ESP_OK);
    assert(panel->reset(panel) == ESP_OK && resets == 3);
    fail_tx = 1;
    assert(panel->init(panel) == ESP_FAIL);
    assert(panel->del(panel) == ESP_OK);
    assert(lcd_panel_factory_entry_t(io, &cfg, &panel) == ESP_OK);
    assert(panel->reset(panel) == ESP_OK && resets == 4);
    assert(panel->init(panel) == ESP_OK && inits == 3);
    assert(panel->del(panel) == ESP_OK);
    puts("Mosaico panel handoff lifecycle: ok");
}
