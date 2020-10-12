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

#define INIT_SECONDS	56
#define INIT_MINUTES	59
#define INIT_HOURS		14

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

alarm_t alarm;
/*
 * @brief   Application entry point.
 */
int main(void)
{


	alarm.seconds = 0;
	alarm.minutes = 0;
	alarm.hours = 15;

  	/* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
  	/* Init FSL debug console. */
    BOARD_InitDebugConsole();

    xTaskCreate(second_task, "Seconds", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 4, NULL);
    xTaskCreate(minute_task, "Minutes", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 4, NULL);
    xTaskCreate(hour_task, "Hours", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 4, NULL);
    xTaskCreate(alarm_task, "Alarm", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 4, NULL);
    xTaskCreate(print_task, "Print", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 4, NULL);

    Seconds = xSemaphoreCreateBinary();
    Minutes = xSemaphoreCreateBinary();

    Queue = xQueueCreate(3, sizeof(time_msg_t));

    mutex = xSemaphoreCreateMutex();

    Group = xEventGroupCreate();

    vTaskStartScheduler();

    /* Force the counter to be placed into memory. */
    volatile static int i = 0 ;
    /* Enter an infinite loop, just incrementing a counter. */
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

		if( (xEventGroupValue & BIT_SEGUNDOS) && (xEventGroupValue & BIT_MINUTOS) && (xEventGroupValue & BIT_HORAS))
		{
			xSemaphoreTake(mutex, portMAX_DELAY);
			PRINTF("ALARMA");
			xSemaphoreGive(mutex);
		}
	}
}

void print_task(void *parameters)
{
	//time_msg_t *time_msg;
	time_msg_t incoming_msg;
	uint8_t seconds = INIT_SECONDS;
	uint8_t minutes = INIT_MINUTES;
	uint8_t hours = INIT_HOURS;
	
	//time_msg = &incoming_msg;


	for(;;)
	{
		//If Queue is empty wait until its not. 
		if(xQueueReceive(Queue, &incoming_msg, portMAX_DELAY))
		{
			switch(incoming_msg.time_type)
			{
			case seconds_type:
				seconds = incoming_msg.value;
				break;
			case minutes_type:
				minutes = incoming_msg.value;
				break;
			case hours_type:
				hours = incoming_msg.value;
				break;
			}
			
			xSemaphoreTake(mutex, portMAX_DELAY);
			PRINTF("%02d : %02d : %02d\r\n", hours, minutes, seconds);
			xSemaphoreGive(mutex);
		}
	}
}


void second_task(void *parameters)
{
	time_msg_t outcoming_msg;
	outcoming_msg.time_type = seconds_type;

	uint8_t counter_seconds = INIT_SECONDS;
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

		outcoming_msg.value = counter_seconds;

		//Send new seconds value to time_queue
		xQueueSendToBack(Queue, &outcoming_msg, portMAX_DELAY);

		if(alarm.seconds == counter_seconds)
		{
			xEventGroupSetBits(Group, BIT_SEGUNDOS);
		}
	}
}

void minute_task(void *parameters)
{
	uint8_t counter_minutes = INIT_MINUTES;

	time_msg_t outcoming_msg;
	outcoming_msg.time_type = minutes_type;

	while(true)
	{
		xSemaphoreTake(Seconds, portMAX_DELAY);
		counter_minutes++;

		if(counter_minutes == 60)
		{
			counter_minutes = 0;

			xSemaphoreGive(Minutes);
		}

		outcoming_msg.value = counter_minutes;

		//Send new minutes value to time_queue
		xQueueSendToBack( Queue, &outcoming_msg, portMAX_DELAY);

		if(alarm.minutes == counter_minutes)
		{
			xEventGroupSetBits(Group, BIT_MINUTOS);
		}
	}
}

void hour_task(void *parameters)
{
	uint8_t counter_hours = INIT_HOURS;

	time_msg_t outcoming_msg;
	outcoming_msg.time_type = hours_type;

	while(true)
	{
		xSemaphoreTake(Minutes, portMAX_DELAY);
		counter_hours++;

		if(counter_hours == 24)
		{
			counter_hours = 0;
		}

		outcoming_msg.value = counter_hours;

		//Send new hours value to time_queue
		xQueueSendToBack( Queue, &outcoming_msg, portMAX_DELAY);

		if(alarm.hours == counter_hours)
		{
			xEventGroupSetBits(Group, BIT_HORAS);
		}

	}
}
