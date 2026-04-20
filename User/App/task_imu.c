#include "bsp_jy61p.h"
#include "drv_uart_imu.h" // 包含你的串口环形缓冲接口
#include "logger.h"
#include "cmsis_os2.h"

JY61P_t MyIMU; // 实例化传感器对象

void Task_IMU_Entry(void *argument) 
{
    Drv_IMU_UART_Init(); // 开启底层串口接收
    
    // 绑定底层读取接口
    JY61P_Init(&MyIMU, 1, Drv_IMU_Read_Data);

    while(1) 
    {
        // 1. 让对象自己去更新数据
        JY61P_Update(&MyIMU);
        
        // 2. 随心所欲地使用这 9 轴数据！
        // 比如打印 Roll, Pitch, Yaw (对应的数组下标是 0, 1, 2)
//        LOG_I("IMU", "Roll: %.2f, Pitch: %.2f, Yaw: %.2f", 
//              MyIMU.angle[0], MyIMU.angle[1], MyIMU.angle[2]);
              
        // 打印 Z 轴加速度
        // LOG_I("IMU", "Z-Acc: %.2f g", MyIMU.acc[2]);
        
        osDelay(50); 
    }
}


