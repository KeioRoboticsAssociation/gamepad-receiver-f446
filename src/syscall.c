#include <inttypes.h>

#include "stm32f4xx_hal.h"

extern UART_HandleTypeDef huart2;

/*Low layer read(input) function*/
__attribute__ ((used))
int _read(int file, char* ptr, int len) {
  char c;
  if (HAL_UART_Receive(&huart2, (uint8_t*) &c, 1, 0xFFFF) == HAL_OK) {
    HAL_UART_Transmit(&huart2, (uint8_t*) &c, 1, 0xFFFF); // callback
    *ptr = c;
    return len;
  }
  return -1;
}

/*Low layer write(output) function*/
__attribute__ ((used))
int _write(int file, char* ptr, int len) {
  if (HAL_UART_Transmit(&huart2, (uint8_t*) ptr, len, 0xFFFF) == HAL_OK) {
    return len;
  }
  return -1;
}
