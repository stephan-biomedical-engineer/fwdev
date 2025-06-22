<!--- cSpell:enable --->

# Criando abstrações de hardware

> [!NOTE]
> :brain:

O desenvolvimento em sistemas embarcados reais pode ser complexo. Uma abordagem para reduzir essa complexidade é tentar desenvolver o máximo possível do firmware no PC, deixando apenas aspectos de tempo real ou hardwares mais complexos de serem emulados no PC como foco no desenvolvimento direto na plataforma embarcada.

## Desenvolva no PC o quanto puder

Em sistemas embarcados é comum ficarmos escravos do hardware que estamos usando. Normalmente existem muitos periféricos de hardware que tendem a dificultar a tarefa de realizar parte do desenvolvimento no PC. Aspectos temporais podem tornar isso ainda mais complexo, ao exigir que eventos sejam sincronizados de forma precisa. Além disso, a ausência de uma placa pode atrasar o desenvolvimento caso se decida fazer tudo diretamente no hardware final.

Enquanto isso possa realmente justificar alguns casos de desenvolvimento, a boa notícia é que muita coisa pode ser feita sem que se tenha um hardware em mãos, bastando usar boas técnicas de desenvolvimento, criando abstrações de hardware capazes de permitir o desenvolvimento no PC. Com tudo desenvolvido, o trabalho de porte e verificação de funcionamento na plataforma final de hardware se torna mais simples e, principalmente, reduz o tempo de desenvolvimento. Se preferir, entenda como redução de custo do projeto.

## Hardware Abstraction Layer (HAL)

Os fabricantes de microcontroladores costumam fornecer suas implementações (ou drivers) para os periféricos disponíveis no chip. Especialmente para microcontroladores baseados em núcleos de processamento Cortex M, a arquitetura de software conta com pelo menos dois níveis de abstração. A primeira é a abstração do núcleo de processamento, fornecida pela ARM e conhecida como CMSIS (Common Microcontroller Software Interface Standard). Sobre essa abstração é comum o fabricante que emprega núcleos ARM escrever a sua biblioteca de drivers. No nosso caso principal, com ferramentas da ST, é possível usar a camada de abstração de hardware da ST, comumente fornecida em dois níveis diferentes:

- **HAL** (High Level Hardware Abstration Layer): abstração de alto nível, com uso facilitado para o usuário e requerendo pouco conhecimento dos periféricos e registros da ST
- **LL** (Low Level abstraction layer): abstração de nível mais baixo, normalmente com macros e funções que fazem uso direto de registros dos periféricos. Requer algum conhecimento dos periféricos da ST.

Para quem está acostumado a usar o HAL da ST, sabe que basta incluir o arquivo de inclusão `main.h` para ter acesso a todos os periféricos e funções da biblioteca. Infelizmente, essa inclusão irá condenar o seu projeto a uma eterna dependência do HAL da ST. Se pretende validar seu código antes no PC ou ter um projeto mais portável entre fabricantes, é determinante criar o seu nível abstração, sem qualquer dependência de registros ou periféricos específicos. Sim, dá um pouco de trabalho, perde-se um pouco de desempenho, mas pode reduzir o tempo de desenvolvimento (ou custo) final.

Como exemplo, vamos criar a seguir algumas abstrações de hardware para um microcontrolador, seguindo bons padrões de projeto e evitando qualquer dependência de hardware. A partir desses exemplos e suas implementações para diversos sistemas operacionais, será mais fácil criar outras.

Alguns pontos releventes devem ser considerados nesse processo de abstração:

- Podemos ter várias instâncias de um mesmo hardware. Por exemplo, mais de uma porta serial ou SPI.
- Todo o processo de configuração precisa ser replicado na sua interface, sem depender de qualquer pré-definição fornecida pelo fabricante. 
- Como simular o comportamento de um periférico pode mudar significativamente. Por exemplo, uma memória flash pode ser simulada como um arquivo. Já um pino de IO pode ser um pouco mais complexo, principalmente se pretende simular alterações nas entradas desses pinos. Seu programa poderia ler comandos do console para setar um pino ou poderia ter um endpoint com uma biblioteca como ZeroMQ capaz de receber comandos remotos via rede. Um pouco de criatividade pode ajudar.
- Algumas interfaces serão feitas com a técnica de ponteiros opacos, explicada posteriormente nessa seção. Isso é importante para uma separação completa entre definição e implementação.
- As chamadas das funções de um determinado módulo serão apresentadas como drivers, através de uma estrutura de ponteiros de função. Isso permite acoplar dinamicamente o driver desejado, dando suporte para revisões diferentes ou implementações personalizadas.
- Vamos evitar o uso de alocação dinâmica sempre que possível.

Como vai ser notado, a\ implementação diretamente para o microcontrolador acaba sendo até mesmo mais simples do que a implementação para o PC. No PC, a menos que se escreva um código todo em pooling, pode ser necessário lidar com threads, mutexes e outras estruturas de sincronização, além de arquivos e bibliotecas adicionais, a depender da sua escolha de implementação. 

### Abstração da CPU

Com isso em mente, a proposta é desenvolver uma abstração inicial com o mínimo necessário para operação. Você pode extender esse arquivo depois, com mais funcionalidades. Por exemplo, pode adicionar callbacks para recepção por interrupção ou mesmo DMA, callbacks para indicação de fim de transmissão, etc. O arquivo de inclusão está a seguir, denominado de `hal_uart.h`. As explicações serão dadas posteriormente.

https://github.com/marcelobarrosufu/fwdev/blob/b156bcffe07d7005796ca77682ff5ff5acee0ab9/source/port/hal_uart.h#L1-L38

Algumas explicações são importantes. Longe de serem apenas manias do autor, elas refletem anos de desenvolvimento e estudo, criando interfaces que funcionam, geram poucos erros e são fáceis de serem portadas e mantidas. 

### Namespaces

Quem trabalha com linguagens mais modernos do que o C está acostumado com o conceito de _namespacing_. A idéia é encapsular, aninhando definições em módulos relacionados, evitando conflitos. Infelizmente, esse conceito não existe em C. Uma forma de tentar imitar o comportamento do namespacing é forçar uma rígida estratégia de nomeação de arquivos e definições, empregando o underscore como separador (o "_"), na notação conhecida como "snake case". Sim, muitos desenvolvedores preferem a lógica do "pascal case", onde se omite o underscore e usa-se a primeira letra de cada palavra em maiúscula e o resto em minúsculas. O autor considera o a forma snake case mais legível, dando a impressão de um espaço entre palavras e facilitando a leitura. Caso prefira outra, não tem problema: o importante é ter uma notação comum para todos os desenvolvedores do seu time.

No arquivo apresentado, foram criados três níveis de hierarquia: **hal** para indicar que é um arquivo de interface de hardware, **uart** que é uma abreviação para porta serial e depois funções ou definições relacionadas com a porta serial, como **open**. O nome final, **hal_uart_open()** permite reduzir conflitos com várias outras partes do código, como uma possível função de open de um adc (**hal_adc_open()**) ou outras funções **open** sem relacionamento algum com hardware. Confie em mim, é uma excelente prática.

Além disso, algumas notações relacionadas a sufixos são recorrentes:

- **_e**: para enumerações
- **_t**: para tipos definidos
- **_s**: para estruturas

Além de evitar conflitos de nomes, dão visibilidade imediata ao tipo de dado utilizado. E, antes que pergunte, eu não incentivo o uso de notação Húngara. Prefiro padronizar tudo com `stdint.h`, `stdbool.h` e `stddef.h`.

Dois comentários finais: o primeiro é relacionado ao ```#pragma once```, diretiva para evitar inclusões recursivas. Apesar de ser amplamente suportada por quase todos os compiladores, não é algo padrão. Se prefere algo totalmente ANSI-C, recomendo usar algo como:

```C copy
#ifndef __HAL_UART_H__
#define  __HAL_UART_H__

// file contents

#endif /**  __HAL_UART_H__ /**
```

Note que a definição foi criada com base no nome do arquivo, apenas deixando tudo em maiúsculas e colocando underscores no início e no fim.

O segundo comentário é relacionado a proteção de inclusão quando o header é usado por arquivos escritos em C++. O ```extern "C"``` é uma forma de garantir que o código seja compilado como C, evitando problemas de decoração de nomes.

```C copy
#ifdef _cplusplus
extern "C" {
#endif

// file contents, exported as C for further decoration

#ifdef __cplusplus
}
#endif
```

### Ponteiros opacos

Um outro ponto chama a atenção no arquivo de inclusão: a declaração do tipo de dados personalizado `hal_uart_dev_t`. Ele é declarado como um ponteiro para uma estrutura do tipo `struct *hal_uart_dev_s` mas note que não existe nenhuma definição do conteúdo da estrutura. Essa é uma técnica conhecida como ponteiro opaco, onde é possível declarar um ponteiro para uma estrutura sem se conhecer o conteúdo da mesma. Como será visto posteriormente, a estrutura `struct hal_uart_dev_s` será declarada apenas na realização da implementação da serial, no arquivo `hal_uart.c`. Com isso, nenhum detalhe é relevado sobre o dispositivo e a estrutura pode ser personalizada, a depender do porte desejado.

Para dar vida a nossa implementação, vamos apresentar (só dessa vez!) quatro portes diferentes: Win32, Linux, MacOS e STM32L411 (BlackPill). Assim você vai poder ver claramente as diferenças na realização da implementação. 

### Implementação para Win32


### Implementação para Linux


### Implementação para MacOS

### Implementação para STM32L411

```C
#include "main.h"
#include "hal_uart.h"

#define PORT_SER_BUFFER_SIZE 128

```

## Compilando para uma plataforma específica

Nesse ponto, existem diversos caminhos que podem ser tomados. Vai depender muito do tipo de ferramenta utilizada no build system. As IDEs, em geral, tornam isso mais complicado do que o uso de arquivos de Makefile ou sistemas baseados em cmake. É preciso que se garanta que apenas a realização desejada seja de fato compilada, sem nenhuma definição relacionada a outra plataforma sendo incluída.

Para facilitar a compatibilização com build systems baseados em IDE, como o STM32CubeIDE, vamos adotar a abordagem de definir o porte desejado através de uma definição do pré-processador. Assim é simples tratar via Makefile, onde é possível passar o porte na linha de comando, com algo como `-DHAL_PORT=STM32L4` ou criar a definição `HAL_PORT=STM32L4` manualmente dentro das opções da IDE.

Também iremos organizar todos os portes dentro de um mesmo diretório e criar um arquivo geral que irá selecionar a implementação desejada. 

```bash
source/hal/hal_uart.h
source/hal/hal_uart.c
source/hal/win32/port_ser.c
source/hal/linux/port_ser.c
source/hal/macos/port_ser.c
source/hal/stm32l4/port_ser.c
```

O arquivo `hal_uart.c` irá decidir qual versão do porte compilar através da definição `HAL_PORT`. Uma possível implementação seria:

```C
#if HAL_PORT == "WIN32"
    #include "./win32/port_ser.c"
#elif HAL_PORT == "LINUX"
    #include "./linux/port_ser.c"
#elif HAL_PORT == "MACOS"
    #include "./macos/port_ser.c"
#elif HAL_PORT == "STM32L4"
    #include "./stm32l4/port_ser.c"
#else
    #error Port not defined
#endif
```

Nesse ponto já é possível apresentar um exemplo completo mostrando o uso da porta serial. Vamos utilizar um Makefile mas lembre-se que o mesmo poderia ser feito na IDE, bastando que exista uma definição adequada para `HAL_PORT`.

https://github.com/marcelobarrosufu/fwdev/blob/ff0fde98f0564b7762885dedb0cd874bd1858465/test/hal/utl_dbg/main.c#L1-L22

Para compilação, use o Makefile abaixo:

```make

```

E compile com a seguinte chamada (substitua adequadamente para a sua plataforma)

```
make -DHAL_PORT=win32
```

Os projetos STM32 também pode ser compilados via Makefile, caso prefira. O inconveniente é ter que fazer a compilação manualmente antes de carregar o programa via IDE para depurar. 

## Verificação formal

Uma vez que seu código roda no PC, existem diversas ferramentas que estão à sua disposição para validação e testes. Para verificação de ponteiros e uso de buffers,alocação de memória, etc, alternativas como Valgrind, memcheck, cppcheck e drmemory podem ser úteis. Para desenvolvimento de testes automatizados, o módulo Unity pode ajudar.


## Wraping up

Apesar de requerer um trabalho adicional, os ganhos ao criar abstrações genéricas e portáveis são enormes. E, uma vez desenvolvidas, podem ser reusadas em outros projetos. Considere isso ao iniciar um novo trabalho !
