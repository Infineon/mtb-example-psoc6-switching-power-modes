/******************************************************************************
* File Name:   main.c
*
* Description: This example demonstrates how to transition PSoC® 6 MCU to the 
*              following system and MCU power states:
*              - System power states - Normal Power state / Low Power state
*              - MCU power states - Active / Sleep / Deep Sleep
*
* Related Document: See Readme.md
*
*
*******************************************************************************
* Copyright 2019-2023, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

#include "cyhal.h"
#include "cybsp.h"
#include "cyhal_clock.h"

/*******************************************************************************
* Macros
********************************************************************************/
/* Constants to define LONG and SHORT presses on User Button (x10 = ms) */
#define QUICK_PRESS_COUNT       2u      /* 20 ms < press < 200 ms */
#define SHORT_PRESS_COUNT       20u     /* 200 ms < press < 2 sec */
#define LONG_PRESS_COUNT        200u    /* press > 2 sec */

/* PWM LED frequency constants (in Hz) */
#define PWM_FAST_FREQ_HZ        10
#define PWM_SLOW_FREQ_HZ        3
#define PWM_DIM_FREQ_HZ         100

/* PWM Duty cycles (Active Low, in %) */
#define PWM_50P_DUTY_CYCLE      50.0f
#define PWM_10P_DUTY_CYCLE      90.0f
#define PWM_100P_DUTY_CYCLE     0.0f

/* Clock frequency constants (in Hz) */
#define CLOCK_50_MHZ            50000000u
#define CLOCK_100_MHZ           100000000u

/* Glitch delays */
#define SHORT_GLITCH_DELAY_MS   10u     /* in ms */
#define LONG_GLITCH_DELAY_MS    100u    /* in ms */

/* User button press delay*/
#define USER_BTN_PRESS_DELAY    10u     /* in ms */

typedef enum
{
    SWITCH_NO_EVENT     = 0u,
    SWITCH_QUICK_PRESS  = 1u,
    SWITCH_SHORT_PRESS  = 2u,
    SWITCH_LONG_PRESS   = 3u,
} en_switch_event_t;

/*****************************************************************************
* Function Prototypes
********************************************************************************/
en_switch_event_t get_switch_event(void);

/* Power callbacks */
bool pwm_power_callback(cyhal_syspm_callback_state_t state, cyhal_syspm_callback_mode_t mode, void *arg);
bool clk_power_callback(cyhal_syspm_callback_state_t state, cyhal_syspm_callback_mode_t mode, void *arg);

/*******************************************************************************
* Global Variables
********************************************************************************/
/* HAL Objects */
cyhal_pwm_t pwm;
cyhal_clock_t system_clock;

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for the MCU. It does...
*    1. Initialize the PWM block that controls the LED brightness.
*    2. Register power management callbacks.
*    Do Forever loop:
*    3. Check if User button was pressed and for how long.
*    4. If quickly pressed, swap from Normal Power to Low Power state.
*    5. If short pressed, go to sleep.
*    6. If long pressed, go to deep sleep.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    /* API return code */
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();
    
    /* Initialize the User Button */
    cyhal_gpio_init(CYBSP_USER_BTN, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_PULLUP, CYBSP_BTN_OFF);
    /* Enable the GPIO interrupt to wake-up the device */
    cyhal_gpio_enable_event(CYBSP_USER_BTN, CYHAL_GPIO_IRQ_FALL, CYHAL_ISR_PRIORITY_DEFAULT, true);

    /* Initialize the PWM to control LED brightness */
    cyhal_pwm_init(&pwm, CYBSP_USER_LED, NULL);
    cyhal_pwm_set_duty_cycle(&pwm, PWM_50P_DUTY_CYCLE, PWM_FAST_FREQ_HZ);
    cyhal_pwm_start(&pwm);

    /* Reserve, take ownership of, the specified clock instance*/
    result = cyhal_clock_reserve(&system_clock, &CYHAL_CLOCK_FLL);

// If the clock is not already enabled, enable it
    if (!cyhal_clock_is_enabled(&system_clock))
    {
        result = cyhal_clock_set_enabled(&system_clock, true, true);
    }

    /* Callback declaration for Power Modes */
    cyhal_syspm_callback_data_t pwm_callback = {pwm_power_callback,             /* Callback function */
                                               (cyhal_syspm_callback_state_t)
                                               (CYHAL_SYSPM_CB_CPU_SLEEP |
                                                CYHAL_SYSPM_CB_CPU_DEEPSLEEP |
                                                CYHAL_SYSPM_CB_SYSTEM_NORMAL |
                                                CYHAL_SYSPM_CB_SYSTEM_LOW),     /* Power States supported */
                                               (cyhal_syspm_callback_mode_t)
                                               (CYHAL_SYSPM_CHECK_FAIL),        /* Modes to ignore */
                                                NULL,                           /* Callback Argument */
                                                NULL};                          /* For internal use */

    cyhal_syspm_callback_data_t clk_callback = {clk_power_callback,             /* Callback function */
                                               (cyhal_syspm_callback_state_t)
                                               (CYHAL_SYSPM_CB_CPU_SLEEP |
                                                CYHAL_SYSPM_CB_CPU_DEEPSLEEP |
                                                CYHAL_SYSPM_CB_SYSTEM_NORMAL |
                                                CYHAL_SYSPM_CB_SYSTEM_LOW),     /* Power States supported */
                                               (cyhal_syspm_callback_mode_t)
                                               (CYHAL_SYSPM_CHECK_READY |       
                                                CYHAL_SYSPM_CHECK_FAIL),        /* Modes to ignore */
                                                NULL,                           /* Callback Argument */
                                                NULL};                          /* For internal use */

    /* Initialize the System Power Management */
    cyhal_syspm_init();

    /* Power Management Callback registration */
    cyhal_syspm_register_callback(&pwm_callback);
    cyhal_syspm_register_callback(&clk_callback);
    
    
    for(;;)
    {
        switch (get_switch_event())
        {
            case SWITCH_QUICK_PRESS:
                /* Check if the device is in System Low Power state */
                if (cyhal_syspm_get_system_state() == CYHAL_SYSPM_SYSTEM_LOW)
                {
                    /* Switch to System Normal Power state */
                    cyhal_syspm_set_system_state(CYHAL_SYSPM_SYSTEM_NORMAL);
                }
                else
                {
                    /* Switch to System Low Power state */
                    cyhal_syspm_set_system_state(CYHAL_SYSPM_SYSTEM_LOW);
                }
                break;
            
            case SWITCH_SHORT_PRESS:
                /* Go to sleep */
                cyhal_syspm_sleep();

                /* Wait a bit to avoid glitches from the button press */
                cyhal_system_delay_ms(LONG_GLITCH_DELAY_MS);
                break;

            case SWITCH_LONG_PRESS:
                /* Go to deep sleep */
                cyhal_syspm_deepsleep();

                /* Wait a bit to avoid glitches from the button press */
                cyhal_system_delay_ms(LONG_GLITCH_DELAY_MS);
                break;

            default:
                break;
        }            
    }
}

/*******************************************************************************
* Function Name: get_switch_event
****************************************************************************//**
* Summary:
*  Returns how the User button was pressed:
*  - SWITCH_NO_EVENT: No press 
*  - SWITCH_QUICK_PRESS: Very quick press
*  - SWITCH_SHORT_PRESS: Short press was detected
*  - SWITCH_LONG_PRESS: Long press was detected
*
* Return:
*  Switch event that occurred, if any. 
*
*******************************************************************************/
en_switch_event_t get_switch_event(void)
{
    en_switch_event_t event = SWITCH_NO_EVENT;
    uint32_t pressCount = 0;

    /* Check if User button is pressed */
    while (cyhal_gpio_read(CYBSP_USER_BTN) == CYBSP_BTN_PRESSED)
    {
        /* Wait for 10 ms */
        cyhal_system_delay_ms(USER_BTN_PRESS_DELAY);

        /* Increment counter. Each count represents 10 ms */
        pressCount++;
    }

    /* Check for how long the button was pressed */
    if (pressCount > LONG_PRESS_COUNT)
    {
        event = SWITCH_LONG_PRESS;
    }
    else if (pressCount > SHORT_PRESS_COUNT)
    {
        event = SWITCH_SHORT_PRESS;
    }
    else if (pressCount > QUICK_PRESS_COUNT)
    {
        event = SWITCH_QUICK_PRESS;
    }

    /* Add a delay to avoid glitches */
    cyhal_system_delay_ms(SHORT_GLITCH_DELAY_MS);

    return event;
}

/*******************************************************************************
* Function Name: pwm_power_callback
********************************************************************************
* Summary:
*  Callback implementation for the PWM block. It changes the blinking pattern
*  based on the power state and MCU state.
*
* Parameters:
*  state - state the system or CPU is being transitioned into
*  mode  - callback mode
*  arg   - user argument (not used)
*
* Return:
*  Always true
*
*******************************************************************************/
bool pwm_power_callback(cyhal_syspm_callback_state_t state, cyhal_syspm_callback_mode_t mode, void *arg)
{
    (void) arg;

    /* Stop the PWM before applying any changes */
    cyhal_pwm_stop(&pwm);

    if (mode == CYHAL_SYSPM_BEFORE_TRANSITION)
    {
        if (state == CYHAL_SYSPM_CB_CPU_SLEEP)
        {
            /* Check if the device is in Low Power Mode */
            if (cyhal_syspm_get_system_state() == CYHAL_SYSPM_SYSTEM_LOW)
            {
                /* Before going to Low Power Sleep Mode, set LED brightness to 10% */
                cyhal_pwm_set_duty_cycle(&pwm, PWM_10P_DUTY_CYCLE, PWM_DIM_FREQ_HZ);
            }
            else
            {
                /* Before going to Normal Power Sleep Mode, set LED brightness to 100% */
                cyhal_pwm_set_duty_cycle(&pwm, PWM_100P_DUTY_CYCLE, PWM_DIM_FREQ_HZ);
            }

            /* Restart the PWM */
            cyhal_pwm_start(&pwm);
        }
    }
    else if (mode == CYHAL_SYSPM_AFTER_TRANSITION)
    {
        switch (state)
        {
            case CYHAL_SYSPM_CB_CPU_SLEEP:
            case CYHAL_SYSPM_CB_CPU_DEEPSLEEP:
                /* Check if the device is in Low Power Mode */
                if (cyhal_syspm_get_system_state() == CYHAL_SYSPM_SYSTEM_LOW)
                {
                    /* After waking up, set the slow blink pattern */
                    cyhal_pwm_set_duty_cycle(&pwm, PWM_50P_DUTY_CYCLE, PWM_SLOW_FREQ_HZ);
                }
                else
                {
                    /* After waking up, set the fast blink pattern */
                    cyhal_pwm_set_duty_cycle(&pwm, PWM_50P_DUTY_CYCLE, PWM_FAST_FREQ_HZ);
                }
                break;

            case CYHAL_SYSPM_CB_SYSTEM_NORMAL:
                /* Set fast blinking rate when in Normal Power state*/
                cyhal_pwm_set_duty_cycle(&pwm, PWM_50P_DUTY_CYCLE, PWM_FAST_FREQ_HZ);
                break;

            case CYHAL_SYSPM_CB_SYSTEM_LOW:
                /* Set slow blinking rate when in Low Power state */
                cyhal_pwm_set_duty_cycle(&pwm, PWM_50P_DUTY_CYCLE, PWM_SLOW_FREQ_HZ);
                break;

            default:
                break;
        }

        /* Restart the PWM */
        cyhal_pwm_start(&pwm);
    }

    return true;
}

/*******************************************************************************
* Function Name: clk_power_callback
********************************************************************************
* Summary:
*  Callback implementation for the clocks. It swaps the system frequency between
*  50 MHz and 100 MHz, depending on the power state.
*
* Parameters:
*  state - state the system or CPU is being transitioned into
*  mode  - callback mode
*  arg   - user argument (not used)
*
* Return:
*  Always true
*
*******************************************************************************/
bool clk_power_callback(cyhal_syspm_callback_state_t state, cyhal_syspm_callback_mode_t mode, void *arg)
{
    (void) arg;

    if (mode == CYHAL_SYSPM_AFTER_TRANSITION)
    {
        if (state == CYHAL_SYSPM_CB_SYSTEM_NORMAL)
        {
            /* Set the system clock to 100 MHz */
            cyhal_clock_set_frequency(&system_clock, CLOCK_100_MHZ, NULL);
        }
    }
    else if (mode == CYHAL_SYSPM_BEFORE_TRANSITION)
    {
        if (state == CYHAL_SYSPM_CB_SYSTEM_LOW)
        {
            /* Set the System Clock to 50 MHz */
            cyhal_clock_set_frequency(&system_clock, CLOCK_50_MHZ, NULL);
        }
    }

    return true;
}



/* [] END OF FILE */
