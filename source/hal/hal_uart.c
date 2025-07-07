#include "hal.h"

// static const hal_uart_driver_t* drv = HAL_UART_DRIVER;

void hal_uart_init(void)
{
    // drv->init();
    HAL_UART_DRIVER->init();
}

void hal_uart_deinit(void)
{
    // drv->deinit();
    HAL_UART_DRIVER->deinit();
}

hal_uart_dev_t hal_uart_open(hal_uart_port_t dev, hal_uart_config_t* cfg)
{
    // return drv->open(dev, cfg);
    return HAL_UART_DRIVER->open(dev, cfg);
}

void hal_uart_close(hal_uart_dev_t dev)
{
    // drv->close(dev);
    HAL_UART_DRIVER->close(dev);
}

size_t hal_uart_bytes_available(hal_uart_dev_t dev)
{
    // return drv->bytes_available(dev);
    return HAL_UART_DRIVER->bytes_available(dev);
}

ssize_t hal_uart_read(hal_uart_dev_t dev, uint8_t* buffer, size_t size)
{
    // return drv->read(dev, buffer, size);
    return HAL_UART_DRIVER->read(dev, buffer, size);
}

ssize_t hal_uart_write(hal_uart_dev_t dev, uint8_t* buffer, size_t size)
{
    // return drv->write(dev, buffer, size);
    return HAL_UART_DRIVER->write(dev, buffer, size);
}

void hal_uart_flush(hal_uart_dev_t dev)
{
    // drv->flush(dev);
    HAL_UART_DRIVER->flush(dev);
}

ssize_t hal_uart_byte_read(hal_uart_dev_t dev, uint8_t* c)
{
    // return drv->read(dev, c, 1);
    return HAL_UART_DRIVER->read(dev, c, 1);
}

ssize_t hal_uart_byte_write(hal_uart_dev_t dev, uint8_t c)
{
    // return drv->write(dev, &c, 1);
    return HAL_UART_DRIVER->write(dev, &c, 1);
}
