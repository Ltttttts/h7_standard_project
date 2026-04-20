#include "bsp_key.h"
#include "key_dev.h"
#include "logger.h"
#include "cmsis_os2.h"


#if 0

#define KEY_NUM 4
Key_t Keys[KEY_NUM]; 

static const char* TAG = "Task_KEY";

void Task_Key_Entry(void *argument) 
{
    for(int i = 0; i < KEY_NUM; i++) 
	{
        Key_Init(&Keys[i], i + 1, Drv_Key_Read, 1000);
    }

    while (1) 
    {
        // 2. 批量刷新状态机
        for(int i = 0; i < KEY_NUM; i++) {
            Key_Update(&Keys[i], 10);
            
            // 3. 事件处理分发
            Key_Event_e event = Key_GetEvent(&Keys[i]);
			
			
            if (event != KEY_EVENT_NONE) {
                // 根据发生事件的按键 ID 执行动作
                switch(Keys[i].id) {
                    case 1: 
                        if(event == KEY_EVENT_CLICK)
						{ 
							LOG_I(TAG,"key_1 short");
						}
						if(event == KEY_EVENT_LONG_PRESS)
						{ 
							LOG_I(TAG,"key_1 long");
						}
                        break;
                    case 2:
                        if(event == KEY_EVENT_CLICK)
						{ 
							LOG_I(TAG,"key_2 short");
						}
						if(event == KEY_EVENT_LONG_PRESS)
						{ 
							LOG_I(TAG,"key_2 long");
						}
                        break;
					case 3:
                        if(event == KEY_EVENT_CLICK)
						{ 
							LOG_I(TAG,"key_3 short");
						}
						if(event == KEY_EVENT_LONG_PRESS)
						{ 
							LOG_I(TAG,"key_3 long");
						}
                        break;
					case 4:
                        if(event == KEY_EVENT_CLICK)
						{ 
							LOG_I(TAG,"key_4 short");
						}
						if(event == KEY_EVENT_LONG_PRESS)
						{ 
							LOG_I(TAG,"key_4 long");
						}
                        break;
                }
            }
        }

        osDelay(10); 
    }
}

#endif
