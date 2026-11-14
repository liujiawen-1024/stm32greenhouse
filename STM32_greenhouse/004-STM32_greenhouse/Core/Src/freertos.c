/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "app.h"
#include "app_event.h"
#include "app_state.h"
#include "esp01s.h"
#include "gpio.h"
#include "oled.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* 周期 */
#define CTRL_PERIOD_MS      100u    /* 按键 / 手自动 / 继电器 / LED */
#define SENSOR_PERIOD_MS    5000u   /* 采样周期，和 003 的 5000 次 1ms 中断一致 */
#define DISPLAY_PERIOD_MS   200u    /* OLED 刷新 */
#define CLOUD_PERIOD_MS     200u    /* 无上报时的轮询节拍 */

/* 栈深度，单位 words（xTaskCreate 的 usStackDepth） */
#define CTRL_STACK_WORDS    192u
#define SENSOR_STACK_WORDS  320u
#define DISPLAY_STACK_WORDS 192u
#define CLOUD_STACK_WORDS   384u

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* I2C1 总线互斥量：OLED / BH1750 / EEPROM 三处共用 hi2c1 */
osMutexId g_i2c_mutex = NULL;

/* 采样完成通知：SensorTask 发，CloudTask 收 */
static osSemaphoreId s_sem_report = NULL;

/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartControlTask(void const * argument);
void StartSensorTask(void const * argument);
void StartDisplayTask(void const * argument);
void StartCloudTask(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  osMutexDef(i2cMutex);
  g_i2c_mutex = osMutexCreate(osMutex(i2cMutex));
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  osSemaphoreDef(reportSem);
  s_sem_report = osSemaphoreCreate(osSemaphore(reportSem), 1);
  /* 先取一次清零：CloudTask 只在有新的采样结果时才上报 */
  if (s_sem_report != NULL) {
    (void)osSemaphoreWait(s_sem_report, 0u);
  }
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* USER CODE END RTOS_QUEUES */

  /* USER CODE BEGIN RTOS_THREADS */
  osThreadDef(controlTask, StartControlTask, osPriorityAboveNormal, 0, CTRL_STACK_WORDS);
  (void)osThreadCreate(osThread(controlTask), NULL);

  osThreadDef(sensorTask, StartSensorTask, osPriorityNormal, 0, SENSOR_STACK_WORDS);
  (void)osThreadCreate(osThread(sensorTask), NULL);

  osThreadDef(displayTask, StartDisplayTask, osPriorityBelowNormal, 0, DISPLAY_STACK_WORDS);
  (void)osThreadCreate(osThread(displayTask), NULL);

  /* 云端任务优先级最低：AT 指令是阻塞轮询等待，别让它抢占本地控制 */
  osThreadDef(cloudTask, StartCloudTask, osPriorityLow, 0, CLOUD_STACK_WORDS);
  (void)osThreadCreate(osThread(cloudTask), NULL);
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartControlTask */
/**
  * @brief  本地控制任务：按键事件 -> 手自动 -> 继电器 -> LED。
  *         100ms 一个节拍，等价于 003 主循环去掉显示与云端之后的骨架。
  */
/* USER CODE END Header_StartControlTask */
void StartControlTask(void const * argument)
{
  /* USER CODE BEGIN StartControlTask */
  uint8_t idx;

  (void)argument;
  for(;;)
  {
    /* 只保留最后一个按键：取一次就够，后面的按键已经覆盖前面的 */
    if (EV_KeyTake(&idx) != 0u) {
      APP_Key(idx);
    }

    APP_ManualPoll();      /* 手动 60s 无新指令 -> 回自动 */
    APP_Data_Ctrl();       /* 阈值 + 回差，仅自动模式改执行器与报警位 */
    GPIO_RelayApply();     /* 逻辑开关 -> 四路继电器 */
    LED_Scan();            /* 自动/手动 + 正常/报警 */

    osDelay(CTRL_PERIOD_MS);
  }
  /* USER CODE END StartControlTask */
}

/* USER CODE BEGIN Header_StartSensorTask */
/**
  * @brief  采集任务：DHT11 / BH1750 / 土壤 ADC，采完通知云端任务。
  */
/* USER CODE END Header_StartSensorTask */
void StartSensorTask(void const * argument)
{
  /* USER CODE BEGIN StartSensorTask */
  (void)argument;
  for(;;)
  {
    APP_GET_DATA();

    printf("temp=%d humi=%d ill=%d soil=%d\r\n",
           (int)g_app.env_data.temp, (int)g_app.env_data.humi,
           (int)g_app.env_data.ill,  (int)g_app.env_data.soil);

    if (s_sem_report != NULL) {
      (void)osSemaphoreRelease(s_sem_report);
    }

    osDelay(SENSOR_PERIOD_MS);
  }
  /* USER CODE END StartSensorTask */
}

/* USER CODE BEGIN Header_StartDisplayTask */
/**
  * @brief  显示任务：200ms 刷一页，OLED 与 BH1750 共用 I2C1，所以要拿锁。
  */
/* USER CODE END Header_StartDisplayTask */
void StartDisplayTask(void const * argument)
{
  /* USER CODE BEGIN StartDisplayTask */
  (void)argument;
  for(;;)
  {
    APP_I2C_Lock();
    OLED_ALL();
    APP_I2C_Unlock();

    osDelay(DISPLAY_PERIOD_MS);
  }
  /* USER CODE END StartDisplayTask */
}

/* USER CODE BEGIN Header_StartCloudTask */
/**
  * @brief  云端任务：先连 WiFi/MQTT（阻塞约 10s），之后有采样就上报，
  *         并在每个节拍解析下行 property/set。
  */
/* USER CODE END Header_StartCloudTask */
void StartCloudTask(void const * argument)
{
  /* USER CODE BEGIN StartCloudTask */
  (void)argument;

  /* 放在任务里而不是 APP_InitLocal：这段最长 10s，不该挡住 OLED 和采集 */
  (void)ESP01S_Init();

  for(;;)
  {
    if (osSemaphoreWait(s_sem_report, CLOUD_PERIOD_MS) == osOK) {
      ESP01S_Send_Data();   /* 温湿光土 */
      ESP01S_Send_SW();     /* 四路开关状态 */
    }

    ESP01S_ParseMQTTMessage();
  }
  /* USER CODE END StartCloudTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
