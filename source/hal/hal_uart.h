#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum hal_uart_port_e
    {
        HAL_UART_PORT0 = 0,
        HAL_UART_PORT1,
        HAL_UART_NUM_PORTS,
    } hal_uart_port_t;

    typedef enum hal_uart_baud_rate_e
    {
        HAL_UART_BAUD_RATE_9600 = 0,
        HAL_UART_BAUD_RATE_19200,
        HAL_UART_BAUD_RATE_57600,
        HAL_UART_BAUD_RATE_115200,
    } hal_uart_baud_rate_t;

    typedef enum hal_uart_parity_e
    {
        HAL_UART_PARITY_NONE = 0,
        HAL_UART_PARITY_ODD,
        HAL_UART_PARITY_EVEN,
    } hal_uart_parity_t;

    typedef struct* hal_uart_dev_s hal_uart_dev_t;

    hal_uart_dev_t hal_uart_open(hal_uart_port_t dev);
    bool hal_uart_close(hal_uart_dev_t dev);
    int32_t hal_uart_write(hal_uart_dev_t dev, uint8_t* data, size_t size);
    int32_t hal_uart_read(hal_uart_dev_t dev, uint8_t* data, size_t* size, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
