#ifndef __BSP_JY61P_H
#define __BSP_JY61P_H
#include <stdint.h>

/* 1. 面向对象：JY61P 类定义 */
typedef struct {
    uint8_t id;
    
    // 【属性】：九轴物理量 (0:X轴/Roll, 1:Y轴/Pitch, 2:Z轴/Yaw)
    float acc[3];    // 加速度 (单位: g)
    float gyro[3];   // 角速度 (单位: °/s)
    float angle[3];  // 欧拉角 (单位: °)
    
    // 【方法】：底层数据读取接口 (面向接口编程，隔离硬件)
    uint16_t (*ReadData_CB)(uint8_t *buf, uint16_t max_len);
} JY61P_t;

/* 2. 暴露给上层的成员方法 */
void JY61P_Init(JY61P_t *imu, uint8_t id, uint16_t (*ReadCB)(uint8_t*, uint16_t));
void JY61P_Update(JY61P_t *imu);

#endif



