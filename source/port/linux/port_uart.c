// source/port/linux/port_uart.c
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <pty.h>
#include <sys/select.h>
#include <string.h>
#include "hal_uart.h"

// Internamente, vamos mapear cada HAL_UART_PORTx em um fd de PTY.
#define MAX_PORTS HAL_UART_NUM_PORTS

typedef struct
{
    int fd;
    hal_uart_config_t cfg;
} linux_uart_t;

static linux_uart_t ports[MAX_PORTS];

// --- Helpers de configuração termios ---
static int configure_port(int fd, hal_uart_config_t* cfg)
{
    struct termios tty;
    if(tcgetattr(fd, &tty) != 0)
        return -1;

    // baud rate
    speed_t speed;
    switch(cfg->baud_rate)
    {
    case HAL_UART_BAUD_RATE_9600:
        speed = B9600;
        break;
    case HAL_UART_BAUD_RATE_19200:
        speed = B19200;
        break;
    case HAL_UART_BAUD_RATE_38400:
        speed = B38400;
        break;
    case HAL_UART_BAUD_RATE_57600:
        speed = B57600;
        break;
    case HAL_UART_BAUD_RATE_115200:
        speed = B115200;
        break;
    default:
        speed = B115200;
    }
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    // parity
    tty.c_cflag &= ~(PARENB | PARODD);
    if(cfg->parity == HAL_UART_PARITY_ODD)
        tty.c_cflag |= PARODD | PARENB;
    if(cfg->parity == HAL_UART_PARITY_EVEN)
        tty.c_cflag |= PARENB;

    // stop bits
    tty.c_cflag &= ~CSTOPB;
    if(cfg->stop_bits == HAL_UART_STOP_BITS_2)
        tty.c_cflag |= CSTOPB;

    // flow control
    tty.c_cflag &= ~CRTSCTS;
    if(cfg->flow_control == HAL_UART_FLOW_CONTROL_CTS_RTS)
        tty.c_cflag |= CRTSCTS;

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // raw mode
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;

    // apply
    if(tcsetattr(fd, TCSANOW, &tty) != 0)
        return -1;
    return 0;
}

// --- Driver functions ---
static void linux_uart_init(void)
{
    memset(ports, 0, sizeof(ports));
}

static void linux_uart_deinit(void)
{
    for(int i = 0; i < MAX_PORTS; i++)
    {
        if(ports[i].fd > 0)
            close(ports[i].fd);
    }
}

static hal_uart_dev_t linux_uart_open(hal_uart_port_t id, hal_uart_config_t* cfg)
{
    if(id < 0 || id >= MAX_PORTS)
        return NULL;

    int master_fd, slave_fd;
    char slave_name[64];
    if(openpty(&master_fd, &slave_fd, slave_name, NULL, NULL) < 0)
        return NULL;

    // configuramos o master_fd
    if(configure_port(master_fd, cfg) < 0)
    {
        close(master_fd);
        close(slave_fd);
        return NULL;
    }

    ports[id].fd = master_fd;
    ports[id].cfg = *cfg;

    // para debug: informe qual /dev/pts/X usar no outro terminal
    fprintf(stderr, "[UART%d] virtual port: %s\n", id, slave_name);

    return (hal_uart_dev_t) &ports[id];
}

static void linux_uart_close(hal_uart_dev_t dev)
{
    linux_uart_t* p = (linux_uart_t*) dev;
    if(!p || p->fd < 0)
        return;
    close(p->fd);
    p->fd = -1;
}

static size_t linux_uart_bytes_available(hal_uart_dev_t dev)
{
    linux_uart_t* p = (linux_uart_t*) dev;
    if(!p || p->fd < 0)
        return 0;
    fd_set rfds;
    struct timeval tv = {0};
    FD_ZERO(&rfds);
    FD_SET(p->fd, &rfds);
    return select(p->fd + 1, &rfds, NULL, NULL, &tv) > 0 ? 1 : 0;
}

static ssize_t linux_uart_read(hal_uart_dev_t dev, uint8_t* buf, size_t sz)
{
    linux_uart_t* p = (linux_uart_t*) dev;
    if(!p || p->fd < 0)
        return -1;
    return read(p->fd, buf, sz);
}

static ssize_t linux_uart_write(hal_uart_dev_t dev, uint8_t* buf, size_t sz)
{
    linux_uart_t* p = (linux_uart_t*) dev;
    if(!p || p->fd < 0)
        return -1;
    return write(p->fd, buf, sz);
}

static void linux_uart_flush(hal_uart_dev_t dev)
{
    linux_uart_t* p = (linux_uart_t*) dev;
    if(!p || p->fd < 0)
        return;
    tcdrain(p->fd);
}

// --- Registrar driver no HAL ---
#include "hal.h"
static const hal_uart_driver_t linux_uart_driver = {
    .init = linux_uart_init,
    .deinit = linux_uart_deinit,
    .open = linux_uart_open,
    .close = linux_uart_close,
    .bytes_available = linux_uart_bytes_available,
    .read = linux_uart_read,
    .write = linux_uart_write,
    .flush = linux_uart_flush,
};

const hal_uart_driver_t* HAL_UART_DRIVER = &linux_uart_driver;
