/******************************************************************************
* File Name:   main.c
*
* Version:     1.0.0
*
* Description: This is the source code for the Switching Power Modes Example
*              for ModusToolbox.
*
* Related Document: See Readme.md
*
*
*******************************************************************************
* (c) (2019), Cypress Semiconductor Corporation. All rights reserved.
*******************************************************************************
* This software, including source code, documentation and related materials
* ("Software"), is owned by Cypress Semiconductor Corporation or one of its
* subsidiaries ("Cypress") and is protected by and subject to worldwide patent
* protection (United States and foreign), United States copyright laws and
* international treaty provisions. Therefore, you may use this Software only
* as provided in the license agreement accompanying the software package from
* which you obtained this Software ("EULA").
*
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software source
* code solely for use in connection with Cypress's integrated circuit products.
* Any reproduction, modification, translation, compilation, or representation
* of this Software except as specified above is prohibited without the express
* written permission of Cypress.
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
* including Cypress's product in a High Risk Product, the manufacturer of such
* system or application assumes all risk of such use and in doing so agrees to
* indemnify Cypress against all liability.
*******************************************************************************/

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"

/*******************************************************************************
* Macros
********************************************************************************/
/* Constants to define LONG and SHORT presses on User Button */
#define QUICK_PRESS_COUNT       2u      /* 20 ms < press < 200 ms */
#define SHORT_PRESS_COUNT       20u     /* 200 ms < press < 2 sec */
#define LONG_PRESS_COUNT        200u    /* press > 2 sec */

/* PWM LED frequency constants (in Hz) */
#define PWM_FAST_FREQ           5
#define PWM_SLOW_FREQ           3
#define PWM_DIM_FREQ            100
#define PWM_50P_DUTY_CYCLE      50.0f
#define PWM_10P_DUTY_CYCLE      90.0f
#define PWM_100P_DUTY_CYCLE     0.0f

/* Clock frequency constants (in Hz) */
#define CLOCK_50_MHZ            50000000u
#define CLOCK_100_MHZ           100000000u
#define SYSTEM_CLOCK            0u

typedef enum
{
    SWITCH_NO_EVENT		= 0u,
	SWITCH_QUICK_PRESS  = 1u,
	SWITCH_SHORT_PRESS	= 2u,
	SWITCH_LONG_PRESS   = 3u,
} en_switch_event_t;

/*****************************************************************************
* Function Prototypes
********************************************************************************/
en_switch_event_t get_switch_event(void);

/* Power callbacks */
cy_en_syspm_status_t pwm_sleep_callback(cy_stc_syspm_callback_params_t *params, cy_en_syspm_callback_mode_t mode);
cy_en_syspm_status_t pwm_deepsleep_callback(cy_stc_syspm_callback_params_t *params, cy_en_syspm_callback_mode_t mode);
cy_en_syspm_status_t pwm_enter_lp_callback(cy_stc_syspm_callback_params_t *params, cy_en_syspm_callback_mode_t mode);
cy_en_syspm_status_t pwm_enter_ulp_callback(cy_stc_syspm_callback_params_t *params, cy_en_syspm_callback_mode_t mode);
cy_en_syspm_status_t clock_enter_lp_callback(cy_stc_syspm_callback_params_t *params, cy_en_syspm_callback_mode_t mode);
cy_en_syspm_status_t clock_enter_ulp_callback(cy_stc_syspm_callback_params_t *params, cy_en_syspm_callback_mode_t mode);

/*******************************************************************************
* Global Variables
********************************************************************************/
/* HAL Objects */
cyhal_pwm_t pwm;

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for CM4 CPU. It does...
*    1. Register sleep callbacks.
*    2. Initialize the PWM block that controls the LED brightness.
*    Do Forever loop:
*    3. Check if KIT_BTN1 was pressed and for how long.
*    4. If quickly pressed, swap from Low Power (LP) to Ultra-LP (vice-versa).
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

    /* SysPm callback params */
	cy_stc_syspm_callback_params_t callbackParams = {
	    /*.base       =*/ NULL,
	    /*.context    =*/ NULL
	};

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();
    
    /* Initialize the User Button */
    cyhal_gpio_init((cyhal_gpio_t) CYBSP_USER_BTN, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_PULLUP, CYBSP_BTN_OFF);
    cyhal_gpio_enable_event((cyhal_gpio_t) CYBSP_USER_BTN, CYHAL_GPIO_IRQ_FALL, CYHAL_ISR_PRIORITY_DEFAULT, true);

    /* Initialize the TPWM resource for PWM operation */
    cyhal_pwm_init(&pwm, (cyhal_gpio_t) CYBSP_USER_LED, NULL);
    cyhal_pwm_set_duty_cycle(&pwm, PWM_50P_DUTY_CYCLE, PWM_FAST_FREQ);
    cyhal_pwm_start(&pwm);

    /* Callback declaration for Power Modes */
    cyhal_system_callback_t pwmSleepCb =     {pwm_sleep_callback,       /* Callback function */
                                              CY_SYSPM_SLEEP,           /* Callback type */
                                              CY_SYSPM_SKIP_CHECK_READY |
                                              CY_SYSPM_SKIP_CHECK_FAIL, /* Skip mode */
                                              &callbackParams,          /* Callback params */
                                              NULL, NULL};              /* For internal use */
    cyhal_system_callback_t pwmDeepSleepCb = {pwm_deepsleep_callback,   /* Callback function */
                                              CY_SYSPM_DEEPSLEEP,       /* Callback type */
                                              CY_SYSPM_SKIP_CHECK_READY |
                                              CY_SYSPM_SKIP_CHECK_FAIL, /* Skip mode */
											  &callbackParams,		    /* Callback params */
                                              NULL, NULL};              /* For internal use */
    cyhal_system_callback_t pwmEnterUlpCb =  {pwm_enter_ulp_callback,   /* Callback function */
										      CY_SYSPM_ULP,             /* Callback type */
                                              CY_SYSPM_SKIP_BEFORE_TRANSITION |
                                              CY_SYSPM_SKIP_CHECK_READY |
                                              CY_SYSPM_SKIP_CHECK_FAIL, /* Skip mode */
											  &callbackParams,		    /* Callback params */
                                              NULL, NULL};				/* For internal usage */
    cyhal_system_callback_t pwmEnterLpCb =   {pwm_enter_lp_callback,    /* Callback function */
										      CY_SYSPM_LP,				/* Callback type */
                                              CY_SYSPM_SKIP_BEFORE_TRANSITION |
                                              CY_SYSPM_SKIP_CHECK_READY |
                                              CY_SYSPM_SKIP_CHECK_FAIL, /* Skip mode */
											  &callbackParams,		    /* Callback params */
                                              NULL, NULL};				/* For internal usage */                                              
    cyhal_system_callback_t clkEnterUlpCb =  {clock_enter_ulp_callback, /* Callback function */
										      CY_SYSPM_ULP,             /* Callback type */
                                              CY_SYSPM_SKIP_AFTER_TRANSITION |
                                              CY_SYSPM_SKIP_CHECK_READY |
                                              CY_SYSPM_SKIP_CHECK_FAIL, /* Skip mode */
											  &callbackParams,		    /* Callback params */
                                              NULL, NULL};              /* For internal usage */
    cyhal_system_callback_t clkEnterLpCb =   {clock_enter_lp_callback,  /* Callback function */
										      CY_SYSPM_LP,			    /* Callback type */
                                              CY_SYSPM_SKIP_BEFORE_TRANSITION |
                                              CY_SYSPM_SKIP_CHECK_READY |
                                              CY_SYSPM_SKIP_CHECK_FAIL, /* Skip mode */
										  	  &callbackParams,          /* Callback params */
                                              NULL, NULL};              /* For internal usage */
   
    /* Callback registration */
    cyhal_system_register_callback(&pwmSleepCb); 
    cyhal_system_register_callback(&pwmDeepSleepCb);
    cyhal_system_register_callback(&pwmEnterUlpCb); 
    cyhal_system_register_callback(&pwmEnterLpCb); 
    cyhal_system_register_callback(&clkEnterUlpCb); 
    cyhal_system_register_callback(&clkEnterLpCb); 

    for(;;)
    {
        switch (get_switch_event())
        {
            case SWITCH_QUICK_PRESS:
                /* Check if the device is in System ULP mode */
                if (Cy_SysPm_IsSystemUlp())
                {
                    /* Switch to System LP mode */
                    Cy_SysPm_SystemEnterLp();
                }
                else
                {
                    /* Switch to ULP mode */
                    Cy_SysPm_SystemEnterUlp();
                }
                break;
            
            case SWITCH_SHORT_PRESS:
                /* Go to sleep */
                cyhal_system_sleep();

                /* Wait a bit to avoid glitches from the button press */
                Cy_SysLib_Delay(100);
                break;

            case SWITCH_LONG_PRESS:
                /* Go to deep sleep */
                cyhal_system_deepsleep();

                /* Wait a bit to avoid glitches from the button press */
                Cy_SysLib_Delay(100);
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
*  - SWITCH_NO_EVENT: No press or very quick press
*  - SWITCH_SHORT_PRESS: Short press was detected
*  - SWITCH_LONG_PRESS: Long press was detected
*
* Parameters:
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
        Cy_SysLib_Delay(10);

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
    Cy_SysLib_Delay(10);

    return event;
}

/*******************************************************************************
* Function Name: pwm_sleep_callback
****************************************************************************//**
* Summary:
*  PWM Sleep callback implementation. It changes the LED behavior based on the
*  System Mode.
*  - LP Mode CPU Sleep  : LED is turned ON
*  - ULP Mode CPU Sleep : LED is dimmed.
*  Note that the LED brightness is controlled using the PWM block.
*
* Parameters:
*  params: callback parameter. 
*  mode: callback mode to operate.
*
* Return:
*  Returns CY_SYSPM_SUCCESS if succesful, otherwise CY_SYSPM_FAIL. 
*
*******************************************************************************/
cy_en_syspm_status_t pwm_sleep_callback(cy_stc_syspm_callback_params_t *params, cy_en_syspm_callback_mode_t mode)
{
    cy_en_syspm_status_t status = CY_SYSPM_FAIL;

    /* Stop the PWM before applying any changes */
    cyhal_pwm_stop(&pwm);

    switch (mode)
    {
        case CY_SYSPM_BEFORE_TRANSITION:
            /* Check if the device is in System ULP mode */
            if (Cy_SysPm_IsSystemUlp())
            {
                /* Before going to ULP sleep mode, dim the LED at 10% */
                cyhal_pwm_set_duty_cycle(&pwm, PWM_10P_DUTY_CYCLE, PWM_DIM_FREQ);
            }
            else
            {
                /* Before going to LP sleep mode, turn on the LED 100% */
                cyhal_pwm_set_duty_cycle(&pwm, PWM_100P_DUTY_CYCLE, PWM_DIM_FREQ);
            }

            status = CY_SYSPM_SUCCESS;
            break;

        case CY_SYSPM_AFTER_TRANSITION:
            /* Check if the device is in System ULP mode */
            if (Cy_SysPm_IsSystemUlp())
            {
                /* After waking up, set the slow blink pattern */
                cyhal_pwm_set_duty_cycle(&pwm, PWM_50P_DUTY_CYCLE, PWM_SLOW_FREQ);
            }
            else
            {
                /* After waking up, set the fast blink pattern */
                cyhal_pwm_set_duty_cycle(&pwm, PWM_50P_DUTY_CYCLE, PWM_FAST_FREQ);
            }

            status = CY_SYSPM_SUCCESS;
            break;

        default:
            /* Don't do anything in the other modes */
            status = CY_SYSPM_SUCCESS;
            break;
    }

    /* Restart the PWM */
    cyhal_pwm_start(&pwm);

    return status;
}

/*******************************************************************************
* Function Name: pwm_deepsleep_callback
****************************************************************************//**
* Summary:
*  PWM Deep Sleep callback implementation. It turns the LED off before going
*  to deep sleep power mode. After waking up, it sets the LED to blink.
*
* Parameters:
*  params: callback parameter. 
*  mode: callback mode to operate.
*
* Return:
*  Returns CY_SYSPM_SUCCESS if succesful, otherwise CY_SYSPM_FAIL. 
*
*******************************************************************************/
cy_en_syspm_status_t pwm_deepsleep_callback(cy_stc_syspm_callback_params_t *params, cy_en_syspm_callback_mode_t mode)
{
    cy_en_syspm_status_t status = CY_SYSPM_FAIL;

    /* Stop the PWM before applying any changes */
    cyhal_pwm_stop(&pwm);

    switch (mode)
    {
        case CY_SYSPM_AFTER_TRANSITION:
            /* Check if the device is in System ULP mode */
            if (Cy_SysPm_IsSystemUlp())
            {
                /* After waking up, set the slow blink pattern */
                cyhal_pwm_set_duty_cycle(&pwm, PWM_50P_DUTY_CYCLE, PWM_SLOW_FREQ);
            }
            else
            {
                /* After waking up, set the fast blink pattern */
                cyhal_pwm_set_duty_cycle(&pwm, PWM_50P_DUTY_CYCLE, PWM_FAST_FREQ);
            }

            /* Re-enable the PWM (PDL level) */
            Cy_TCPWM_PWM_Enable(pwm.base, pwm.resource.channel_num);

            /* Restart the PWM */
            cyhal_pwm_start(&pwm);

            status = CY_SYSPM_SUCCESS;
            break;

        default:
            /* Don't do anything in the other modes */
            status = CY_SYSPM_SUCCESS;
            break;
    }

    return status;
}

/*******************************************************************************
* Function Name: pwm_enter_lp_callback
****************************************************************************//**
* Summary:
*  Enter System Low Power mode. It changes the LED pattern to blink faster.
*
* Parameters:
*  params: callback parameter. 
*  mode: callback mode to operate.
*
* Return:
*  Returns CY_SYSPM_SUCCESS if succesful, otherwise CY_SYSPM_FAIL. 
*
*******************************************************************************/
cy_en_syspm_status_t pwm_enter_lp_callback(cy_stc_syspm_callback_params_t *params, cy_en_syspm_callback_mode_t mode)
{
    cy_en_syspm_status_t status = CY_SYSPM_FAIL;

    /* Stop the PWM before applying any changes */
    cyhal_pwm_stop(&pwm);

    switch (mode)
    {
        case CY_SYSPM_AFTER_TRANSITION:

            /* Set slow blink LED pattern */
            cyhal_pwm_set_duty_cycle(&pwm, PWM_50P_DUTY_CYCLE, PWM_FAST_FREQ);

            status = CY_SYSPM_SUCCESS;
            break;

        default:
            /* Don't do anything in the other modes */
            status = CY_SYSPM_SUCCESS;
            break;
    }

    /* Restart the PWM */
    cyhal_pwm_start(&pwm);

    return status;
}

/*******************************************************************************
* Function Name: pwm_enter_ulp_callback
****************************************************************************//**
* Summary:
*  Enter System Low Power mode. It changes the LED pattern to blink slower.
*
* Parameters:
*  params: callback parameter. 
*  mode: callback mode to operate.
*
* Return:
*  Returns CY_SYSPM_SUCCESS if succesful, otherwise CY_SYSPM_FAIL. 
*
*******************************************************************************/
cy_en_syspm_status_t pwm_enter_ulp_callback(cy_stc_syspm_callback_params_t *params, cy_en_syspm_callback_mode_t mode)
{
    cy_en_syspm_status_t status = CY_SYSPM_FAIL;

    /* Stop the PWM before applying any changes */
    cyhal_pwm_stop(&pwm);

    switch (mode)
    {
        case CY_SYSPM_AFTER_TRANSITION:
            /* Set fast blink LED pattern */
            cyhal_pwm_set_duty_cycle(&pwm, PWM_50P_DUTY_CYCLE, PWM_SLOW_FREQ);

            status = CY_SYSPM_SUCCESS;
            break;

        default:
            /* Don't do anything in the other modes */
            status = CY_SYSPM_SUCCESS;
            break;
    }

    /* Restart the PWM */
    cyhal_pwm_start(&pwm);

    return status;
}

/*******************************************************************************
* Function Name: clock_enter_lp_callback
****************************************************************************//**
* Summary:
*  Enter System Low Power mode. It sets the System Clock to 100 MHz.
*
* Parameters:
*  params: callback parameter. 
*  mode: callback mode to operate.
*
* Return:
*  Returns CY_SYSPM_SUCCESS if succesful, otherwise CY_SYSPM_FAIL. 
*
*******************************************************************************/
cy_en_syspm_status_t clock_enter_lp_callback(cy_stc_syspm_callback_params_t *params, cy_en_syspm_callback_mode_t mode)
{
    cy_en_syspm_status_t status = CY_SYSPM_FAIL;

    switch (mode)
    {
        case CY_SYSPM_AFTER_TRANSITION:

            /* Set the system clock to 100 MHz */
            cyhal_system_clock_set_frequency(SYSTEM_CLOCK, CLOCK_100_MHZ);

            status = CY_SYSPM_SUCCESS;
            break;

        default:
            /* Don't do anything in the other modes */
            status = CY_SYSPM_SUCCESS;
            break;
    }

    return status;
}

/*******************************************************************************
* Function Name: clock_enter_ulp_callback
****************************************************************************//**
* Summary:
*  Enter System Ultra Low Power mode. It sets the System Clock to 50 MHz.
*
* Parameters:
*  params: callback parameter. 
*  mode: callback mode to operate.
*
* Return:
*  Returns CY_SYSPM_SUCCESS if succesful, otherwise CY_SYSPM_FAIL. 
*
*******************************************************************************/
cy_en_syspm_status_t clock_enter_ulp_callback(cy_stc_syspm_callback_params_t *params, cy_en_syspm_callback_mode_t mode)
{
    cy_en_syspm_status_t status = CY_SYSPM_FAIL;

    switch (mode)
    {
        case CY_SYSPM_BEFORE_TRANSITION:
            /* Set the System Clock to 50 MHz */
            cyhal_system_clock_set_frequency(SYSTEM_CLOCK, CLOCK_50_MHZ);

            status = CY_SYSPM_SUCCESS;
            break;

        default:
            /* Don't do anything in the other modes */
            status = CY_SYSPM_SUCCESS;
            break;
    }

    return status;
}

/* [] END OF FILE */
