#include "bsp_jy61p.h"
#include "wit_c_sdk.h"  // 引入维特官方库，获取 sReg 和宏定义
#include "cmsis_os.h"


static void SensorDataUpdata(uint32_t uiReg, uint32_t uiRegNum)
{

}

void Delayms(uint16_t ms)
{
	osDelay(ms);
}

void JY61P_Init(JY61P_t *imu, uint8_t id, uint16_t (*ReadCB)(uint8_t*, uint16_t)) 
{
    imu->id = id;
    imu->ReadData_CB = ReadCB;
    
    // 数组清零初始化
    for(int i = 0; i < 3; i++) {
        imu->acc[i] = 0.0f;
        imu->gyro[i] = 0.0f;
        imu->angle[i] = 0.0f;
    }
    
    WitInit(WIT_PROTOCOL_NORMAL, 0x50); 
	WitRegisterCallBack(SensorDataUpdata);
	WitDelayMsRegister(Delayms);
}

/**
 * @brief 核心更新逻辑：抽水 -> 喂给官方协议栈 -> 执行你的高阶解算 -> 存入对象
 */
void JY61P_Update(JY61P_t *imu) 
{
    uint8_t temp_buf[64];
    
    // 1. 从我们在中断里写的“环形缓冲区水池”抽水
    uint16_t len = imu->ReadData_CB(temp_buf, 64);
    if (len == 0) return; // 没水直接退出，节约 CPU
    
    // 2. 喂给官方协议栈 (它内部会自动拼接数据包，并更新全局的 sReg 数组)
    for (uint16_t i = 0; i < len; i++) {
        WitSerialDataIn(temp_buf[i]); 
    }
    
    // =================================================================
    // 3. 核心解算：直接榨取官方 sReg 的寄存器数据！
    // 注意：常数必须带 'f' (如 32768.0f)，强制使用单精度浮点，防止编译器转成慢速的 double！
    // =================================================================
    for(int i = 0; i < 3; i++)
    {
        imu->acc[i]   = sReg[AX + i]   / 32768.0f * 16.0f;
        imu->gyro[i]  = sReg[GX + i]   / 32768.0f * 2000.0f;
        imu->angle[i] = sReg[Roll + i] / 32768.0f * 180.0f;
    }
}


