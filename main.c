/*
 * main.c
 *
 *  Created on: 10 oct. 2020
 *      Author: urik_
 */


#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MK66F18.h"
#include "fsl_debug_console.h"
/* TODO: insert other include files here. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "event_groups.h"

/* TODO: insert other definitions and declarations here. */

#define BIT_SEGUNDOS (1 << 0)
#define BIT_MINUTOS (1 << 1)
#define BIT_HORAS (1 << 2)

typedef enum
{
  seconds_type,
  minutes_type,
  hours_type
} time_types_t;

typedef struct
{
	time_types_t time_type;
	uint8_t value;
} time_msg_t;

typedef struct
{
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;
} alarm_t;

void second_task(void *parameters);
void minute_task(void *parameters);
void hour_task(void *parameters);

void alarm_task(void *parameters);
void print_task(void *parameters);

SemaphoreHandle_t Seconds;
SemaphoreHandle_t Minutes;

QueueHandle_t Queue;

EventGroupHandle_t Group;

SemaphoreHandle_t mutex;


int main(void)
{
  alarm_t alarm;

  alarm.seconds = 0;
  alarm.minutes = 0;
  alarm.hours = 0;

  /* Init board hardware. */
  BOARD_InitBootPins();
  BOARD_InitBootClocks();
  BOARD_InitBootPeripherals();
#ifndef BOARD_INIT_DEBUG_CONSOLE_PERIPHERAL
  /* Init FSL debug console. */
  BOARD_InitDebugConsole();
#endif

  xTaskCreate(second_task, "Seconds", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 4, NULL);
  xTaskCreate(minute_task, "Minutes", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 4, NULL);
  xTaskCreate(hour_task, "Hours", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 4, NULL);
  xTaskCreate(alarm_task, "Alarm", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 4, NULL);
  xTaskCreate(print_task, "Print", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 4, NULL);

  /* Force the counter to be placed into memory. */
  volatile static int i = 0 ;
  /* Enter an infinite loop, just incrementing a counter. */
  Seconds = xSemaphoreCreateBinary();
  Minutes = xSemaphoreCreateBinary();

  Queue = xQueueCreate(3, sizeof(time_msg_t));

  mutex = xSemaphoreCreateMutex();

  Group = xEventGroupCreate();

  while(1) {
      i++ ;
      /* 'Dummy' NOP to allow source level single stepping of
          tight while() loop */
      __asm volatile ("nop");
  }
  return 0 ;
}

void alarm_task(void *parameters)
{
  const EventBits_t xBitsToWaitFor = ( BIT_SEGUNDOS | BIT_MINUTOS | BIT_HORAS );
  EventBits_t xEventGroupValue;

  for(;;)
  {

    xEventGroupValue = xEventGroupWaitBits(Group, xBitsToWaitFor, pdTRUE, pdTRUE, portMAX_DELAY);

    if( (xEventGroupValue & BIT_SEGUNDOS) && (xEventGroupValue & BIT_MINUTOS) && (xEventGroupValue & BIT_HORAS)!= 0)
    {
		xSemaphoreTake(mutex, portMAX_DELAY);
		PRINTF("ALARMA");
		xSemaphoreGive(mutex);
    }
  }
}

void print_task(void *parameters)
{
  for(;;)
  {
    xSemaphoreTake(mutex, portMAX_DELAY);
    PRINTF("Imprimir hora obtenida desde la Queue");
    xSemaphoreGive(mutex);
  }
}


void second_task(void *parameters)
{
  uint8_t counter_seconds = 0;
  TickType_t  xLastWakeTime = xTaskGetTickCount();

  TickType_t   xfactor = pdMS_TO_TICKS(1000);

  while(true)
  {
    vTaskDelayUntil(&xLastWakeTime,xfactor);

    counter_seconds++;

    if(counter_seconds >= 60)
    {
      counter_seconds = 0;
      xSemaphoreGive(Seconds);
    }
  }
}
void minute_task(void *parameters)
{
  uint8_t counter_minutes = 0;

  while(true)
  {
    xSemaphoreTake(Seconds, portMAX_DELAY);
    counter_minutes++;

    if(counter_minutes == 60)
    {
      counter_minutes = 0;

      xSemaphoreGive(Minutes);
    }

  }
}
void hour_task(void *parameters)
{
  uint8_t counter_hours = 0;

  while(true)
  {
    xSemaphoreTake(Minutes, portMAX_DELAY);
    counter_hours++;

    if(counter_hours == 24)
    {
      counter_hours = 0;
    }

  }
}
