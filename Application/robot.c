#include "robot.h"
#include "robot_task.h"
#include "mahjong.h"

void RobotInit()
{
    // 关闭中断,防止在初始化过程中发生中断
    // 请不要在初始化过程中使用中断和延时函数！
    // 若必须,则只允许使用DWT_Delay()
    __disable_irq();

    DWT_Init(168);

    mahjong_init(); // 初始化麻将

    OSTaskInit(); // 创建基础任务

    // 初始化完成,开启中断
    __enable_irq();
}

void RobotTask()
{
    mahjong_task();
}