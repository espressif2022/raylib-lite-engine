// SPDX-License-Identifier: Apache-2.0
#include "native_feedback.h"

#include <stdbool.h>
#include "bsp/motor.h"
#include "esp_timer.h"

static esp_timer_handle_t s_off_timer;
static esp_timer_handle_t s_second_timer;
static uint8_t s_second_strength;
static uint16_t s_second_ms;
static bool s_ready;

static void motor_off(void *arg)
{
    (void)arg;
    if(s_ready)(void)bsp_motor_set(false);
}

static void motor_second(void *arg)
{
    (void)arg;
    if(!s_ready||!s_second_ms)return;
    (void)bsp_motor_set_strength(s_second_strength);
    (void)esp_timer_stop(s_off_timer);
    (void)esp_timer_start_once(s_off_timer,(uint64_t)s_second_ms*1000ULL);
}

void mosaico_native_feedback_init(void)
{
    if(bsp_motor_init()!=ESP_OK)return;
    (void)bsp_motor_set(false);
    if(!s_off_timer){
        const esp_timer_create_args_t off={.callback=motor_off,.name="game_haptic_off"};
        const esp_timer_create_args_t second={.callback=motor_second,.name="game_haptic_second"};
        if(esp_timer_create(&off,&s_off_timer)!=ESP_OK)return;
        if(esp_timer_create(&second,&s_second_timer)!=ESP_OK)return;
    }
    s_ready=true;
}

void mosaico_native_feedback_pulse(uint8_t strength,uint16_t duration_ms)
{
    if(!s_ready||!duration_ms||strength>100)return;
    if(s_second_timer)(void)esp_timer_stop(s_second_timer);
    s_second_ms=0;
    (void)bsp_motor_set_strength(strength);
    (void)esp_timer_stop(s_off_timer);
    (void)esp_timer_start_once(s_off_timer,(uint64_t)duration_ms*1000ULL);
}

void mosaico_native_feedback_pattern(uint8_t first_strength,uint16_t first_ms,
                                     uint8_t second_strength,uint16_t gap_ms,
                                     uint16_t second_ms)
{
    mosaico_native_feedback_pulse(first_strength,first_ms);
    if(!s_ready||!s_second_timer||!second_ms)return;
    s_second_strength=second_strength;s_second_ms=second_ms;
    (void)esp_timer_stop(s_second_timer);
    (void)esp_timer_start_once(s_second_timer,(uint64_t)(first_ms+gap_ms)*1000ULL);
}

void mosaico_native_feedback_stop(void)
{
    if(s_off_timer)(void)esp_timer_stop(s_off_timer);
    if(s_second_timer)(void)esp_timer_stop(s_second_timer);
    s_second_ms=0;
    if(s_ready)(void)bsp_motor_set(false);
}
