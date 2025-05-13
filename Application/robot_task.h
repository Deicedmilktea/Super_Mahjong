/* 注意该文件应只用于任务初始化,只能被robot.c包含*/
#pragma once

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

#include "robot.h"

osThreadId robotTaskHandle;

void StartROBOTTASK(void const *argument);

/**
 * @brief 初始化机器人任务,所有持续运行的任务都在这里初始化
 *
 */
void OSTaskInit()
{
    osThreadDef(robottask, StartROBOTTASK, osPriorityNormal, 0, 256);
    robotTaskHandle = osThreadCreate(osThread(robottask), NULL);
}

__attribute__((noreturn)) void StartROBOTTASK(void const *argument)
{
    // static float robot_dt;
    // static float robot_start;

    // 200Hz-500Hz,若有额外的控制任务如平衡步兵可能需要提升至1kHz
    for (;;)
    {
        // robot_start = DWT_GetTimeline_ms();
        RobotTask();
        // robot_dt = DWT_GetTimeline_ms() - robot_start;
        osDelay(5);
    }
}