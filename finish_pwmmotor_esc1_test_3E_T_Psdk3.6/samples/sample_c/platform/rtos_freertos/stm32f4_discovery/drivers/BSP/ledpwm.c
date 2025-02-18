#include "LEDPWM.h"

//PWM
//TIM_HandleTypeDef g_timx_pwm_chy_handle;

TIM_HandleTypeDef g_timx_pwm_chy_handle;     /* 定时器x句柄 */
TIM_HandleTypeDef g_timx_motor_chy_handle;     /* 定时器x句柄 */

/**
 * @brief       通用定时器TIMX 通道Y PWM输出 初始化函数（使用PWM模式1）
 * @note
 *              通用定时器的时钟来自APB1,当PPRE1 ≥ 2分频的时候
 *              通用定时器的时钟为APB1时钟的2倍, 而APB1为42M, 所以定时器时钟 = 84Mhz
 *              定时器溢出时间计算方法: Tout = ((arr + 1) * (psc + 1)) / Ft us.
 *              Ft = 定时器工作频率,单位:Mhz
 *
 * @param       arr: 自动重装值
 * @param       psc: 预分频系数
 * @retval      无
 */
void gtim_timx_pwm_chy_init(uint16_t arr, uint16_t psc)
{
    TIM_OC_InitTypeDef timx_oc_pwm_chy = {0};                       /* 定时器输出句柄 */
    
    g_timx_pwm_chy_handle.Instance = GTIM_TIMX_PWM;                 /* 定时器x */
    g_timx_pwm_chy_handle.Init.Prescaler = psc;                     /* 预分频系数 */
    g_timx_pwm_chy_handle.Init.CounterMode = TIM_COUNTERMODE_UP;    /* 递增计数模式 */
    g_timx_pwm_chy_handle.Init.Period = arr;                        /* 自动重装载值 */
    HAL_TIM_PWM_Init(&g_timx_pwm_chy_handle);                       /* 初始化PWM */

    timx_oc_pwm_chy.OCMode = TIM_OCMODE_PWM1;                       /* 模式选择PWM1 */
    timx_oc_pwm_chy.Pulse = 0;                               /* 设置比较值,此值用来确定占空比 */

    timx_oc_pwm_chy.OCPolarity = TIM_OCPOLARITY_LOW;                                        /* 输出比较极性为低 */
    HAL_TIM_PWM_ConfigChannel(&g_timx_pwm_chy_handle, &timx_oc_pwm_chy, GTIM_TIMX_PWM_CHY); /* 配置TIMx通道y */
    HAL_TIM_PWM_Start(&g_timx_pwm_chy_handle, GTIM_TIMX_PWM_CHY);                           /* 开启对应PWM通道 */
}

/**
 * @brief       通用定时器TIMX 通道Y PWM输出 初始化函数（使用PWM模式1）
 * @note
 *              通用定时器的时钟来自APB1,当PPRE1 ≥ 2分频的时候
 *              通用定时器的时钟为APB1时钟的2倍, 而APB1为42M, 所以定时器时钟 = 84Mhz
 *              定时器溢出时间计算方法: Tout = ((arr + 1) * (psc + 1)) / Ft us.
 *              Ft = 定时器工作频率,单位:Mhz
 *
 * @param       arr: 自动重装值
 * @param       psc: 预分频系数
 * @retval      无
 */
void gtim_timx_motor_chy_init(uint16_t arr, uint16_t psc)
{
    TIM_OC_InitTypeDef timx_oc_motorpwm_chy = {0};                       /* 定时器输出句柄 */
    
    g_timx_motor_chy_handle.Instance = GTIM_TIMX_MOTORPWM;                 /* 定时器x */
    g_timx_motor_chy_handle.Init.Prescaler = psc;                     /* 预分频系数 */
    g_timx_motor_chy_handle.Init.CounterMode = TIM_COUNTERMODE_UP;    /* 递增计数模式 */
    g_timx_motor_chy_handle.Init.Period = arr;                        /* 自动重装载值 */
    HAL_TIM_PWM_Init(&g_timx_motor_chy_handle);                       /* 初始化PWM */

    timx_oc_motorpwm_chy.OCMode = TIM_OCMODE_PWM1;                       /* 模式选择PWM1 */
    timx_oc_motorpwm_chy.Pulse = 0;                               /* 设置比较值,此值用来确定占空比 */

    timx_oc_motorpwm_chy.OCPolarity = TIM_OCPOLARITY_LOW;                                        /* 输出比较极性为低 */
    HAL_TIM_PWM_ConfigChannel(&g_timx_motor_chy_handle, &timx_oc_motorpwm_chy, GTIM_TIMX_MOTORPWM_CHY); /* 配置TIMx通道y */
    HAL_TIM_PWM_Start(&g_timx_motor_chy_handle, GTIM_TIMX_MOTORPWM_CHY);                           /* 开启对应PWM通道 */
}
/**
 * @brief       定时器底层驱动，时钟使能，引脚配置
                此函数会被HAL_TIM_PWM_Init()调用
 * @param       htim:定时器句柄
 * @retval      无
 */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == GTIM_TIMX_PWM)
    {
        GPIO_InitTypeDef gpio_init_struct;
        GTIM_TIMX_PWM_CHY_GPIO_CLK_ENABLE();                            /* 开启通道y的CPIO时钟 */
        GTIM_TIMX_PWM_CHY_CLK_ENABLE();                                 /* 使能定时器时钟 */

        gpio_init_struct.Pin = GTIM_TIMX_PWM_CHY_GPIO_PIN;              /* 通道y的CPIO口 */
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;                        /* 复用推完输出 */
        gpio_init_struct.Pull = GPIO_PULLUP;                            /* 上拉 */
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;                  /* 高速 */
        gpio_init_struct.Alternate = GTIM_TIMX_MOTORPWM_CHY_GPIO_AF;         /* IO口REMAP设置, 是否必要查看头文件配置的说明! */
        HAL_GPIO_Init(GTIM_TIMX_MOTORPWM_CHY_GPIO_PORT, &gpio_init_struct);
    }

    if (htim->Instance == GTIM_TIMX_MOTORPWM)
    {
        GPIO_InitTypeDef gpio_init_struct;
        GTIM_TIMX_MOTORPWM_CHY_GPIO_CLK_ENABLE();                            /* 开启通道y的CPIO时钟 */
        GTIM_TIMX_MOTORPWM_CHY_CLK_ENABLE();                                 /* 使能定时器时钟 */

        gpio_init_struct.Pin = GTIM_TIMX_MOTORPWM_CHY_GPIO_PIN;              /* 通道y的CPIO口 */
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;                        /* 复用推完输出 */
        gpio_init_struct.Pull = GPIO_PULLUP;                            /* 上拉 */
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;                  /* 高速 */
        gpio_init_struct.Alternate = GTIM_TIMX_MOTORPWM_CHY_GPIO_AF;         /* IO口REMAP设置, 是否必要查看头文件配置的说明! */
        HAL_GPIO_Init(GTIM_TIMX_MOTORPWM_CHY_GPIO_PORT, &gpio_init_struct);
    }
}
