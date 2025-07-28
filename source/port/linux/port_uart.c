/**
 * @file port_uart.c
 * @brief Implementação da HAL UART para plataforma Linux usando PTYs.
 *
 * Este backend utiliza pseudo-terminais (PTYs) para simular dispositivos UART reais
 * em ambientes de desenvolvimento ou teste em Linux.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <pty.h>
#include <sys/select.h>
#include <pthread.h>
#include <string.h>

#include "hal_uart.h"
#include "utl_cbf.h"
#include "utl_printf.h"
#include "utl_dbg.h"

/// Número máximo de portas suportadas (espelhando HAL_UART_NUM_PORTS)
#define MAX_PORTS HAL_UART_NUM_PORTS

<<<<<<< HEAD
/// Tamanho do buffer circular interno por porta
#define UART_BUF_SIZE 512

/**
 * @brief Estrutura interna representando uma porta UART em Linux.
 */
typedef struct {
    int fd;                         ///< Descritor de arquivo do PTY
    hal_uart_config_t cfg;         ///< Configuração UART corrente
    utl_cbf_t cb;                  ///< Buffer circular para RX
    uint8_t cb_buf[UART_BUF_SIZE]; ///< Buffer real alocado
    pthread_t thread;              ///< Thread de leitura RX
    bool in_use;                   ///< Flag de uso
=======
typedef struct
{
    int fd;
    hal_uart_config_t cfg;
>>>>>>> linux_branch
} linux_uart_t;

/// Lista de portas UART ativas
static linux_uart_t ports[MAX_PORTS];

<<<<<<< HEAD
// ─────────────────────────────────────────────
// RX THREAD
// ─────────────────────────────────────────────

/**
 * @brief Thread que realiza leitura assíncrona do PTY e aciona callback ou bufferiza.
 */
static void* rx_thread(void* arg) {
    linux_uart_t* p = (linux_uart_t*)arg;
    uint8_t c;
    while (p->in_use) {
        int n = read(p->fd, &c, 1);
        if (n > 0) {
            if (p->cfg.interrupt_callback) {
                p->cfg.interrupt_callback(c);
            } else {
                utl_cbf_put(&p->cb, c);
            }
        } else {
            usleep(5000);
        }
    }
    UTL_DBG_PRINTF(UTL_DBG_MOD_UART, "Stopping RX thread for fd %d\n", p->fd);
    return NULL;
}

// ─────────────────────────────────────────────
// Configuração de termios
// ─────────────────────────────────────────────

/**
 * @brief Aplica configuração UART no descritor fornecido.
 * @param fd Descritor do terminal (PTY)
 * @param cfg Configuração HAL
 * @return 0 em caso de sucesso, -1 em erro
 */
static int configure_port(int fd, hal_uart_config_t* cfg) {
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) return -1;
    
=======
// --- Helpers de configuração termios ---
static int configure_port(int fd, hal_uart_config_t* cfg)
{
    struct termios tty;
    if(tcgetattr(fd, &tty) != 0)
        return -1;

    // baud rate
>>>>>>> linux_branch
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
<<<<<<< HEAD
    
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);
    if (cfg->parity == HAL_UART_PARITY_ODD)   tty.c_cflag |= (PARENB | PARODD);
    if (cfg->parity == HAL_UART_PARITY_EVEN)  tty.c_cflag |= PARENB;
    if (cfg->stop_bits == HAL_UART_STOP_BITS_2) tty.c_cflag |= CSTOPB;
    if (cfg->flow_control == HAL_UART_FLOW_CONTROL_CTS_RTS) tty.c_cflag |= CRTSCTS;

    tty.c_cflag |= (CLOCAL | CREAD | CS8);
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) return -1;
    return 0;
}

// ─────────────────────────────────────────────
// Implementação do Driver HAL
// ─────────────────────────────────────────────

/**
 * @brief Inicializa o driver UART para Linux.
 */
static void linux_uart_init(void) {
    memset(ports, 0, sizeof(ports));
    for (int i = 0; i < MAX_PORTS; i++) {
        utl_cbf_init(&ports[i].cb, ports[i].cb_buf, UART_BUF_SIZE);
    }
}

/**
 * @brief Finaliza todas as portas UART ativas.
 */
static void linux_uart_deinit(void) {
    for (int i = 0; i < MAX_PORTS; i++) {
        if (ports[i].in_use) {
            ports[i].in_use = false;
            pthread_join(ports[i].thread, NULL);
        }
        if (ports[i].fd >= 0) close(ports[i].fd);
    }
}

/**
 * @brief Abre e configura uma porta UART virtual.
 * @param id Identificador da porta UART
 * @param cfg Configuração desejada
 * @return Handle UART válido ou NULL em caso de falha
 */
static hal_uart_dev_t linux_uart_open(hal_uart_port_t id, hal_uart_config_t* cfg) {
    if (id < 0 || id >= MAX_PORTS) return NULL;
    linux_uart_t* p = &ports[id];
    if (p->in_use) return NULL;

    int master_fd, slave_fd;
    char slave_name[64];
    if (openpty(&master_fd, &slave_fd, slave_name, NULL, NULL) < 0) return NULL;
    if (configure_port(master_fd, cfg) < 0) { close(master_fd); close(slave_fd); return NULL; }

    p->fd = master_fd;
    p->cfg = *cfg;
    p->in_use = true;
    utl_cbf_flush(&p->cb);

    pthread_create(&p->thread, NULL, rx_thread, p);
=======

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
>>>>>>> linux_branch

    fprintf(stderr, "[UART%d] virtual port: %s\n", id, slave_name);
<<<<<<< HEAD
    return (hal_uart_dev_t)p;
}

/**
 * @brief Fecha uma porta UART aberta.
 * @param dev Handle UART
 */
static void linux_uart_close(hal_uart_dev_t dev) {
    linux_uart_t* p = (linux_uart_t*)dev;
    if (!p->in_use) return;
    p->in_use = false;
    pthread_join(p->thread, NULL);
=======

    return (hal_uart_dev_t) &ports[id];
}

static void linux_uart_close(hal_uart_dev_t dev)
{
    linux_uart_t* p = (linux_uart_t*) dev;
    if(!p || p->fd < 0)
        return;
>>>>>>> linux_branch
    close(p->fd);
    p->fd = -1;
}

<<<<<<< HEAD
/**
 * @brief Retorna número de bytes disponíveis no buffer RX.
 * @param dev Handle UART
 * @return Quantidade de bytes disponíveis
 */
static size_t linux_uart_bytes_available(hal_uart_dev_t dev) {
    linux_uart_t* p = (linux_uart_t*)dev;
    if (p->cfg.interrupt_callback) return 0;
    return utl_cbf_bytes_available(&p->cb);
}

/**
 * @brief Lê dados do buffer RX.
 * @param dev Handle UART
 * @param buf Buffer de destino
 * @param sz Tamanho máximo a ler
 * @return Número de bytes lidos
 */
static ssize_t linux_uart_read(hal_uart_dev_t dev, uint8_t* buf, size_t sz) {
    linux_uart_t* p = (linux_uart_t*)dev;
    if (p->cfg.interrupt_callback) return 0;
    size_t available = utl_cbf_bytes_available(&p->cb);
    size_t to_read = available < sz ? available : sz;
    for (size_t i = 0; i < to_read; i++) {
        if (utl_cbf_get(&p->cb, &buf[i]) == UTL_CBF_EMPTY) break;
    }
    return to_read;
}

/**
 * @brief Escreve dados diretamente no descritor da UART.
 * @param dev Handle UART
 * @param buf Dados de entrada
 * @param sz Número de bytes
 * @return Bytes enviados
 */
static ssize_t linux_uart_write(hal_uart_dev_t dev, uint8_t* buf, size_t sz) {
    linux_uart_t* p = (linux_uart_t*)dev;
    return write(p->fd, buf, sz);
}

/**
 * @brief Aguarda a conclusão da transmissão UART.
 * @param dev Handle UART
 */
static void linux_uart_flush(hal_uart_dev_t dev) {
    linux_uart_t* p = (linux_uart_t*)dev;
=======
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
>>>>>>> linux_branch
    tcdrain(p->fd);
}

// ─────────────────────────────────────────────
// Registro do driver no HAL global
// ─────────────────────────────────────────────

#include "hal.h"

/**
 * @brief Instância do driver UART para Linux.
 */
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

/**
 * @brief Ponteiro global para o driver UART atual.
 */
const hal_uart_driver_t* HAL_UART_DRIVER = &linux_uart_driver;
