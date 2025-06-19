<!--- cSpell:enable --->

# System Calls

> [!NOTE]
> :robot:

A interação entre linguagens de programação e sistemas operacionais é mediada por interfaces que permitem que programas executem tarefas como leitura de arquivos, escrita em dispositivos e alocação de memória. Em linguagens como C, essas tarefas são abstraídas por bibliotecas como a libc, mas, no fundo, dependem de chamadas de sistema (system calls). Esta seção explora a relação entre compilador, sistema operacional e biblioteca C, abordando tanto o ambiente tradicional de PCs com Linux quanto o cenário de sistemas embarcados bare-metal.

## 3. A Trindade da Compilação

A compilação de um programa C não se resume ao processo de tradução do código-fonte para linguagem de máquina. Ela envolve uma interação entre três componentes essenciais:

- **O Compilador (ex: GCC):** Responsável pela análise sintática, geração de código intermediário e posterior tradução para o formato de instrução da arquitetura alvo.
- **O Sistema Operacional:** Em sistemas convencionais, fornece serviços como alocação de memória e chamadas de sistema. Em sistemas embarcados, essas funções precisam ser tratadas diretamente.
- **A biblioteca C (libc):** Provê implementações de funções padrão como `printf`, `scanf`, `exit`, etc. Sua integração exige compatibilidade de chamada.

Quando usamos um compilador para PC, essa relação entre os três elementos pode estar implícita, com o linker introduzindo as dependências necessárias. Dessa forma, não precisamos nos preocupar com detalhes de como o compilador interage com o sistema operacional ou a biblioteca C. No entanto, em sistemas embarcados, essa relação precisa ser explícita e cuidadosamente configurada.

O nome do compilador muitas vezes reflete essa relação. Por exemplo, `arm-linux-gnueabihf-gcc` indica:

- **arm:** A arquitetura alvo (ARM).
- **linux:** O sistema operacional alvo (Linux).
- **gnueabihf:** A ABI (Application Binary Interface) usada, que define como as funções são chamadas e como os dados são organizados na memória.
- **gcc:** O compilador GNU C.


## Dependências Implícitas: libc e System Calls

Em sistemas operacionais para PC, como Linux, o programador C raramente interage diretamente com o kernel. Em vez disso, ele utiliza funções da biblioteca padrão C (libc), como `printf`, `fopen`, `fprintf` e `exit`. Estas funções fazem, por sua vez, chamadas ao sistema operacional por meio de *system calls*.

Considere o seguinte exemplo:

```C copy
#include <stdio.h>

int main(void)
{
    FILE *fp = fopen("saida.txt", "w");

    if (fp == NULL)
    {
        perror("Erro ao abrir arquivo");
        return 1;
    }

    fprintf(fp, "Resultado: %d pontos obtidos\n", 42);
    fclose(fp);
    
    return 0;
}
```

Este simples trecho de código realiza as seguintes operações:

- `fopen()` solicita ao sistema operacional a criação/abertura de um arquivo.
- `perror()` exibe uma mensagem de erro caso a abertura falhe, utilizando a biblioteca C para formatar a mensagem. O conteúdo será enviado para o *stderr* (saída de erro padrão).
- `fprintf()` formata a saída e a envia para o arquivo, utilizando *buffers* internos da libc.
- `fclose()` finaliza a operação, liberando recursos e garantindo que os dados sejam realmente gravados no disco.

Apesar de parecer que o código foi escrito apenas com funções da libc, sua execução depende de *system calls* específicas, disponibilizadas pelo kernel do sistema operacional. No caso do Linux (x64), com o auxílio de um programa como `strace`, poderíamos observar algumas chamadas de sistema, tais como:

| Número | System Call  | Protótipo                                                         |
|--------|-------|-------------------------------------------------------------------|
| 1      | write | `ssize_t write(int fd, const void *buf, size_t count);`           |
| 2      | open  | `int open(const char *pathname, int flags, mode_t mode);`         |
| 3      | close | `int close(int fd);`                                              |   
 
Uma lista completa de chamadas de sistema do Linux x64 pode ser encontrada [nesse link]9https://syscalls64.paolostivanin.com/).


## Como as chamadas de sistema são resolvidas em sistemas embarcados bare-metal

Em sistemas embarcados do tipo bare-metal, onde não há um sistema operacional presente para atender às chamadas de sistema, as funções da libc precisam ser adaptadas ou reimplementadas. Uma abordagem comum é o uso da `newlib`, uma implementação da biblioteca padrão C projetada para ambientes embarcados.

A `newlib` inclui implementações de funções como `printf`, `fopen`, `malloc`, entre outras. No entanto, para que essas funções operem corretamente em um ambiente bare-metal, o desenvolvedor precisa fornecer implementações das funções de baixo nível que normalmente seriam resolvidas via system calls no Linux. Essas funções são conhecidas como *syscalls stubs*.

Por exemplo, a função `write()` da `newlib`, usada internamente por `printf()`, delega sua funcionalidade a uma função chamada `_write`, que deve ser definida pelo usuário. Se essa função não for provida, o processo de linkedição resultará em erro. O usuário poderia implementar `_write` para redirecionar a saída para uma porta serial, um display ou qualquer outro meio de comunicação disponível no sistema embarcado:

```c copy
int _write(int file, char *ptr, int len) 
{
    // Redireciona a saída para uma porta serial, por exemplo
    for (int i = 0; i < len; i++) 
    {
        send_uart(ptr[i]);
    }
    return len;
}
```

Outras funções que precisam ser definidas incluem `_read`, `_open`, `_close`, `_sbrk` (para alocação de memória), entre outras. Em muitos toolchains, como `arm-none-eabi-gcc`, já existem versões mínimas dessas funções fornecidas por arquivos como `nosys.specs` ou `syscalls.c`.

Portanto, a funcionalidade da biblioteca C em sistemas embarcados depende da integração adequada dessas *stubs*, seja via código do desenvolvedor, seja por bibliotecas auxiliares. Essa arquitetura modular permite que funções de alto nível sejam utilizadas em ambientes altamente restritos, desde que haja uma base mínima de suporte à execução.

Novamente, o nome do compilador reflete essa relação. Por exemplo, `arm-none-eabi-gcc` indica:

- **arm:** A arquitetura alvo (ARM).
- **none:** Ausência de sistema operacional (bare-metal).
- **eabi:** A ABI (Application Binary Interface) usada, que define como as funções são chamadas e como os dados são organizados na memória.
- **gcc:** O compilador GNU C.

##  System calls e STM32CubeIDE

O código gerado pelo STM32CubeIDE, por exemplo, já inclui uma implementação mínima de *system calls* para o ambiente bare-metal. Essas implementações são encontradas no arquivo `core/src/syscalls.c`, que contém as funções necessárias para suportar a biblioteca C em um microcontrolador STM32.

Várias dessas funções geralmente vem com corpo vazio, sem implementação específica. Outras tem implementação mínima. Por exemplo, a função `_write` é definida como a seguir, na versão 1.18 do CubeIDE:

```c copy
__attribute__((weak)) int _write(int file, char *ptr, int len)
{
  (void)file;
  int DataIdx;

  for (DataIdx = 0; DataIdx < len; DataIdx++)
  {
    __io_putchar(*ptr++);
  }
  return len;
}
```

Note que é uma função do tipo `weak`, ou seja, se o usuário definir uma função com o mesmo nome, essa definição do usuário será utilizada. Caso contrário, a implementação padrão será usada. E, além disso, a função chama `__io_putchar`, que é uma função definida pelo usuário, ou seja, o usuário deve implementar essa função para que a saída funcione corretamente. 

`__io_putchar` é uma função bem simples que apenas decide o futuro de cada byte recebido por `_write`. Redirecioná-lo para uma UART ou ITM (Instrumentation Trace Macrocell) é uma escolha comum. 

Outra função, como `_open()`, é definida como:

```c copy
int _open(char *path, int flags, ...)
{
  (void)path;
  (void)flags;
  /* Pretend like we always fail */
  return -1;
}
```

No fundo, qualquer tentativa de abrir um arquivo falhará, retornando -1. Isso é esperado em um ambiente bare-metal, onde não há sistema de arquivos. Mas o usuário pode implementar essa função para abrir arquivos em um sistema de arquivos específico, se necessário. O que de fato esse arquivo aberto irá representar é uma abstração que o desenvolvedor pode criar, como escrever o dado em uma RAM ou em uma memória flash.

As chamadas para alocação de memória, geridas pela biblioteca C, como `malloc` e `free`, precisam ser implementadas para que a memória dinâmica funcione corretamente. Essas chamadas geralmente
são tratadas pela função `_sbrk()`, presente no arquivo `core/src/sysmem.c`, sendo  responsável por gerenciar a memória dinâmica. 
