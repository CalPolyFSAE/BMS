/* Standard includes */
#include "string.h"
#include <vector>
#include <array>

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* Hardware specific includes */

#include "MKE18F16.h"
#include "clock_config.h"
#include "pin_mux.h"

#include "bmsCommand.h"

#include "gpio.h"
#include "spi.h"

using namespace BSP;
/* The configCHECK_FOR_STACK_OVERFLOW setting in FreeRTOSConifg can be used to
 * check task stacks for overflows.  It does not however check the stack used by
 * interrupts.  This demo has a simple addition that will also check the stack used
 * by interrupts if mainCHECK_INTERRUPT_STACK is set to 1.  Note that this check is
 * only performed from the tick hook function (which runs in an interrupt context).
 * It is a good debugging aid - but won't catch interrupt stack problems until the
 * tick interrupt next executes. */
#define mainCHECK_INTERRUPT_STACK 0
#if mainCHECK_INTERRUPT_STACK == 1
const unsigned char ucExpectedInterruptStackValues[] = { 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC };
#endif

#define STACK_SIZE 100
#define TASK_PRIORITY 1

#define SLAVE_COUNT 10

#define t_SLEEP 10000 // find this
#define t_IDLE 1000 // find this

/*
 * Perform any hardware specific setup in this function
 */
static void prvSetupHardware(void);

int pushTransaction( int com, int length, int num, uint8_t **data, int ticksToWait );