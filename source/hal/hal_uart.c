#include "hal.h"

static hal_uart_dev_t *drv = &HAL_UART_DRIVER;

void hal_uart_init(void)
{
    drv->init();
}

void hal_uart_deinit(void)
{
    drv->deinit();
}

hal_uart_dev_t hal_uart_create(hal_uart_port_t dev)
{
    return drv->create(dev);
}

void hal_uart_delete(hal_uart_port_t dev)
{
    drv->delete(dev);
}

bool hal_uart_configure(hal_uart_dev_t dev, hal_uart_config_t* cfg)
{
    return drv->configure(dev, cfg);
}

void hal_uart_interrupt_set(hal_uart_dev_t dev, hal_uart_interrupt_t fun)
{
    drv->interrupt_set(dev, fun);
}

void hal_uart_open(hal_uart_dev_t dev)
{
    drv->open(dev);
}

void hal_uart_close(hal_uart_dev_t dev)
{
    drv->close(dev);
}

size_t hal_uart_bytes_available(hal_uart_dev_t dev)
{
    return drv->bytes_available(dev);
}

int32_t hal_uart_read(hal_uart_dev_t dev, uint8_t* buffer, size_t size)
{
    return drv->read(dev, buffer, size);
}

int32_t hal_uart_write(hal_uart_dev_t dev, uint8_t* buffer, size_t len)
{
    return drv->write(dev, buffer, len);
}

void hal_uart_flush(hal_uart_dev_t dev)
{
    drv->flush(dev);
}

int32_t hal_uart_byte_read(hal_uart_dev_t dev, uint8_t* c)
{
    if (drv->bytes_available(dev) > 0)
        return drv->read(dev, c, 1);
    else
        return -1;
}

int32_t hal_uart_byte_write(hal_uart_dev_t dev, uint8_t c)
{
    return drv->write(dev, &c, 1);
}
