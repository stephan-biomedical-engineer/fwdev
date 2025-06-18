#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "hal_uart.h"

int main(void)
{
    hal_uart_dev_t dev = hal_uart_open(HAL_UART_PORT0);

    if(dev)
    {
        uint8_t *data = (uint8_t *) "I am portable !\n";
        size_t size = strlen((uint8_t *)data);

        hal_uart_write(dev,data,size);
        hal_uart_close(dev);
    }

    return 0;
}
