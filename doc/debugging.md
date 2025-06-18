
# Depuração

> [!NOTE]
> Useful information that users should know, even when skimming content.

Todo mundo erra. É mais fácil admitir isso e tomar as preucações para eliminar rapidamente os erros. Em ambientes profissionais de desenvolvimento a falha de software, comumente chamada de "bug" ou erro, pode ser mitigada de diversas formas, a depender de quanto investimento se tem disponível. Por exemplo, existem ambientes com desenvolvimento em pares de desenvolvedores com outros desenvolvedores focados em revisão de código, permitindo que ocódigo seja analisado por várias pessoas antes de ser testado e colocado em produção. Também é muito comum a criação de código em "salas limpas", ambientes livres de interrupções para que o foco seja mantido no trabalho em desenvolvimento. A recente onda de IAs tem permitido também o uso de ferramentas automatizadas de revisão e sugestão de correção. 

Independente do seu orçamento, é interessante trabalhar com o objetivo de minimizar as falhas cometidas, tentando encontrar a maioria delas ainda no seu teste inicial de bancada. Para isso, é preciso lançar mão de ferramentas de depuração eficientes, técnicas de desenvolvimento apropriadas e muita análise. Nesse capítulo vamos discutir formas de trabalhar como foco em depuração, evitando propagar falhas no processo de desenvolvimento de software embarcado.

## Desenvolva no PC o quanto puder

Em sistemas embarcados é comum ficarmos escravos do hardware que estamos usando. Normalmente existem muitos periféricos de hardware que tendem a dificultar a tarefa de realizar parte do desenvolvimento no PC. Aspectos temporais podem tornar isso ainda mais complexo, ao exigir que eventos sejam sincronizados de forma precisa.

Enquanto isso possa realmente justificar algus casos de desenvolvimento, a boa notícia é que muita coisa pode ser feita sem que se tenha um hardware em mãos, bastando usar boas técnicas de desenvolvimento, criando abstrações de hardware capazes de permitir o desenvolvimento no PC. Com tudo desenvolvido, o trabalho de porte e verificação de funcionamento na plataforma final de hardware se torna mais simples e, principalmente, reduz o tempo de desenvolvimento ou, se preferir, reduz o custo do projeto.

## Criando boas abstrações

Os fabricantes de microcontroladores costumam fornecer suas implementações (ou drivers) para os periféricos disponíveis no chip. Especialmente para microcontroladores baseados em núcleos de processamento Cortex M, a arquitetura de software conta com pelo menos dois níveis de abstração. A primeira é a abstração do núcleo de processamento, fornecida pela ARM e conhecida como CMSIS (...). Sobre essa abstração é comum o fabricante escrever a sua bibliteca de drivers. No nosso caso, com ferramentas da ST, é possível usar a camada de abstração de hardware da ST, comumente fornecida em dois níveis diferentes:

- HAL (hig level Hardware Abstration Layer): abstração de alto nível, com uso facilitado para o usuário e requerendo pouco conhecimento dos periféricos e registros da ST
- LL (Low Level abstraction layer): abstração de nível mais baixo, normalmente com marcros e funções que fazem uso direto de registros dos periféricos. Requer algum conhecimento dos periféricos da ST.

Para quem está acostumado a usar o HAL da ST, sabe que basta incluir o arquivo de inclusão "main.h" para ter acesso a todos os periféricos e funções da biblioteca. Infelizmente, essa inclusão irá condenar o seu projeto a uma eterna dependência do HAL da ST. Se pretende validar antes no PC ou ter um código mais portável entre fabricantes, é determinante criar o seu nível abstração, sem qualquer dependência de registros ou periféricos específicos. Sim, dá um pouco de trabalho, perde-se um pouco de desempenho, mas reduz o custo final (tempo é dinheiro, não é o que dizem ?).

Como exemplo didático, vamos criar a seguir uma abstração para a porta serial do microcontrolador, seguindo bons padrões de projeto e evitando qualquer dependência de hardware. Alguns pontos devem ser considerados nesse processo:

- podemos ter várias portas seriais diferentes
- todo o processo de configuração precisa ser replicado na sua interface, sem depender de qualquer pré-definição fornecida pelo fabricante
- vamos evitar o uso de alocação dinâmica 

Com isso em mente, vamos criar uma abstração inicial com o mínimo necessário para operação. Você extender esse arquivo depois, com mais funcionalidades. Por exemplo, pode adicionar callbacks para recepção por interrupção ou mesmo DMA, callbacks para indicação de fim de transmissão, etc. O arquivo de inclusão está a seguir, denominado de "hal_ser.h". As explicações serão dadas posteriormente.


```C
#pragma once

typedef enum hal_ser_port_e
{
    HAL_SER_PORT0 = 0,
    HAL_SER_PORT1,
    HAL_SER_NUM_PORTS,
} hal_ser_port_t;

typedef enum hal_ser_baud_rate_e
{
    HAL_SER_BAUD_RATE_9600 = 0,
    HAL_SER_BAUD_RATE_19200,
    HAL_SER_BAUD_RATE_57600,
    HAL_SER_BAUD_RATE_115200,
} hal_ser_baud_rate_t;

typedef enum hal_ser_parity_e
{
    HAL_SER_PARITY_NONE = 0,
    HAL_SER_PARITY_ODD,
    HAL_SER_PARITY_EVEN,
} hal_ser_parity_t;

typedef struct *hal_ser_dev_s hal_ser_dev_t;

hal_ser_dev_t hal_ser_open(hal_ser_port_t dev);
bool hal_ser_close(hal_ser_dev_t dev);
int32_t hal_ser_write(hal_ser_dev_t dev, uint8_t *data, size_t size);
int32_t hal_ser_read(hal_ser_dev_t dev, uint8_t *data, size_t *size, uint32_t timeout_ms);

```

Algumas explicações são importantes. Longe de serem apenas manias do autor, elas refletem anos de desenvolvimento e estudo, criando interfaces que funcionam, geram poucos erros e são fáceis de serem portadas e mantidas. 

### Namespaces

Quem trabalha com linguagens mais modernos do que o C (quase todo mundo?) está acostumado com o conceito de namespacing. A idéia é encapsular, aninhando definições em módulos relacionados [melhorar isso]. Infelizmente, esse conceito não existe em C. Uma forma de tentar imitar o comportamento do namespacing é forçar uma rígida estratégia de nomeação de arquivos e definições, empregando o underscore como separador (o "_"), na notação conhecida como "snake case". Sim, muitos desenvolvedores preferem a lógica do "camel case", onde se omite o underscore e usa-se a primeira letra de cada palavra em maiúscula e o resto em minúsculas. O autor considera o a forma snake case mais legível (e o Python também [REF]), dando a impressão de um espaçco entre palavras. o importante é ter uma notação comum para todos os desenvolvedores do time.

No arquivo apresentado, foram criados três níveis de hierarquia: **hal** para indicar que é um arquivo de inteface de hardware, **ser** que é uma abreviação para porta serial (3 letras é um bom tamanho mas use nomes maiores se preferir) e depois funções ou definições relacionadas com a porta serial, como **open**. O nome final, **hal_ser_open()** permite reduzir conflitos com várias outras partes do código, como uma função de open do adc (**hal_adc_open()**) ou outras funções open sem relacionamento algum com hardware. Confie em mim, é uma excelente prática.

Além disso, algumas notações relacionadas a sufixos são recorrentes:

- **_e**: para enumerações
- **_t**: para tipos definidos
- **_s**: para estruturas

Além de evitar conflitos de nomes, dão visibidade imediata ao tipo de dado utilizado.

Um comentário final é relacionado ao ```#pragma once```, diretiva para evitar inclusões recursivas. Apesar de ser amplamente suportada por quase todos os compiladores, não é algo padrão. Se prefere algo totalmente ANSI-C, recomendo usar algo como:

```C
#ifndef __HAL_SER_H__
#define  __HAL_SER_H__

// file contents

#endif /**  __HAL_SER_H__ /**

```

Note que a definição foi criada com base no nome do arquivo, apenas deixando tudo em maiúsculas e colocandos underscores no início e no fim.

### Ponteiros opacos

Um outro ponto chama a atenção no arquivo de inclusão, a declaração do tipo de dados personalizado ```hal_ser_dev_t```. Ele é declarado como um ponteiro para uma estrutura do tipo ```struct *hal_ser_dev_s``` mas note que não existe nenhuma definição da estrutura em si. Essa é uma técnica conhecida como ponteiro opaco, onde é possível declarar um ponteiro para uma estrutura sem se conhecer o conteúdo da mesma. Como será visto posteriormente, a estrutura ```struct hal_ser_dev_s``` será declarada apenas na realização da implementação da serial, no arquivo ```hal_ser.c```. Com isso, nenhum detalhe é relevado sobre o dispositivo e a estrutura pode ser personalizada, a depender do port desejado.

Para dar vida a nossa implementação, vamos apresentar (só dessa vez!) quatro portes diferentes: Win32, Linux, MacOS e STM32L411 (BlackPill). Assim você vai poder ver claramente as diferenças na realização da implementação. 

### Implementação para Win32


### Implementação para Linux


### Implementação para MacOS

### Implementação para STM32L411

```C
#include "main.h"
#include "hal_ser.h"

#define PORT_SER_BUFFER_SIZE 128

```

## Compilando para uma plataforma específica

Nesse ponto, existem diversos caminhos que podem ser tomados. Vai depender muito do tipo de ferramenta utilizada para build system. As IDEs, em geral, tornam isso mais complicado do que o uso de arquivos de Makefile ou sistemas baseados em cmake. É preciso que se garanta que apenas a realização desejada seja de fato compilada, sem nenhuma definição relacionada a outra plataforma sendo incluída.

Para facilitar a compatibilização com build systemas baseados em IDE, como o STM32CubeIDE, vamos adotar a abordagem de definir o porte desejado através de um define. Assim é simples tratar via Makefile, onde é possível passar o porte na linha de comando, com algo como ```-DHAL_PORT=STM32L4``` ou criar a definição ```HAL_PORT=STM32L4`` manualmente dentro das opções da IDE.

Também iremos organizar todos os portes dentro de um mesmo diretório e criar um arquivo geral que irá selecionar a implementação desejada. 

```bash
hal/hal_ser.h
hal/hal_ser.c
hal/win32/port_ser.c
hal/linux/port_ser.c
hal/macos/port_ser.c
hal/stm32l4/port_ser.c
```

O arquivo ```hal_ser.c``` irá decidir qual versão do porte compilar através da definição ```HAL_PORT```. Uma possível implementação seria:

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

Nesse ponto já é possível apresentar um exemplo completo mostrando o uso da porta serial. Vamos utilizar um Makefile mas lembre-se que o mesmo poderia ser feito na IDE, bastando que exista uma definição adequada para ```HAL_PORT```.

```C
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "hal_ser.h"

int main(void)
{
    hal_ser_dev_t dev = hal_ser_open(HAL_SER_PORT0);

    if(dev)
    {
        uint8_t *data = (uint8_t *) "I am portable !\n";
        size_t size = strlen((uint8_t *)data);

        hal_ser_write(dev,data,size);
        hal_ser_close(dev);
    }

    return 0;
}
```

Para compilação, use o Makefile abaixo:

```make

```

E compile com a seguinte chamada (substitua adequadamente para a sua plataforma)

```
make -DHAL_PORT=win32
```

Os projetos STM32 também pode ser compilados via Makefile, caso prefira. O inconveniente é ter que fazer a compilação manualmente antes de carregar o programa via IDE para depurar. 

## Wraping up

Apesar de requerer um trabalho adicional, os ganhos ao criar abstrações genéricas e portáveis são enormes. E, uma vez desenvolvidas, podem ser reusadas em outros projetos. Considere isso ao iniciar um novo trabalho !

# Verificação formal

Uma vez que seu código roda no PC, existem diversas ferramentas que estão à sua disposição para validação e testes. Para verificação de ponteiros e uso de buffers,alocação de memória, etc, alternativas como Valgrind, memcheck, cppcheck e drmemory podem ser úteis. Para desenvolvimento de testes automatizados, o módulo Unity pode ajudar.

## Abstração para depuração

Nesse ponto já temos as bases necessárias para criação de abstrações de hardware e seria apropriado criar um novo módulo de portabilidade chamadao "debug" ou simplesmente "dbg". O papel dele, como o nome sugere, é permitir a impressão e dump de mensagens de depuração. Em sistemas embarcados podemos fazer esses logs serem enviados via serial. Num porte para PC um caminho possível pode ser a geração de arquivos. 

Adicionalmente, o módulo "dbg" vai permitir a introdução do conceito associado a "system calls".

### Interface de depuração

Para a interface de depuração, vamos criar um arquivo de inclusão como definido a seguir. O módulo será denominado de "utl", de utilidades, uma vez que ele é totalmente portável, apenas reusando interfaces já definidas. O que se pretende com o módulo é criar um mecanismo simples que permita ligar e desligar logs relacionados a partes específicas do programa em construção, direcionando esses logs para a saída escolhida.

Vale a pena deixar aqui uma observação: esse módulo vai fazer uso de funções e macros com parâmetros variáveis, algo comumente condenado em aplicações embarcadas críticas por poder levar a usos indevidos de memória (alocação, estouros, etc). Enquanto entendemos o motivo óbvio, a ideia é que esses logs não façam parte do firmware final, sendo apenas para desenvolvimento.

O arquivo ```utl_log.h``` é apresentado a seguir, inteiro. Ele será explicado a seguir.

```C
#pragma once

#define XMACRO_DBG_MODULES  \
    X(UTL_DBG_MOD_APP, 0)   \
    X(UTL_DBG_MOD_SER, 1)   \
    X(UTL_DBG_MOD_ADC, 2)

typedef enum utl_dbg_modules_e
{
#define X(MOD, INDEX) MOD = INDEX,
    XMACRO_DBG_MODULES
#undef X
} utl_dbg_modules_t;

void utl_dbg_init(void);
void utl_dbg_mod_enable(utl_dbg_modules_t mod_idx);
void utl_dbg_mod_disable(utl_dbg_modules_t mod_idx);
bool utl_dbg_mod_is_enabled(utl_dbg_modules_t mod_idx);
void utl_dbg_printf(utl_dbg_modules_t mod_idx, const char* fmt, ...);
void utl_dbg_dump(char* stamp, uint8_t* data, uint16_t size);
uint8_t* utl_dbg_mod_name(utl_dbg_modules_t mod_idx);

#define UTL_LOG_HEADER(mod, fmt, file, line) "[%s][%s:%d] " fmt, (char*) utl_dbg_mod_name(mod), file, line

#define UTL_DBG_PRINTF(mod, fmt, ...)                     \
    do                                                    \
    {                                                     \
        if(utl_dbg_mod_is_enabled(mod))                   \
            printf(UTL_LOG_HEADER(mod, fmt, __FILE__,__LINE__), ##__VA_ARGS__); \
    } while(0)

#define UTL_DBG_DUMP(mod, data, size)      \
    do                                     \
    {                                      \
        if(utl_dbg_mod_is_enabled(mod))    \
            utl_dbg_dump("", data, size);  \
    } while(0)
```

Por usar muitas macros (e X macros !), o arquivo é relativamente complexo para um iniciante na linguagem C. Mas ele traz muita informação e conhecimento, valendo a pena investir algum tempo entendendo os seus detalhes.

O princípio básico de funcionamento desse arquivo (detahes adiantes) é que se tem uma variável de 32 bits onde cada bit permite o controle de um módulo de depuração. Assim, cada módulo pode ser ativado e desativado individualmente. Para a impressão, é criada uma macro que segue a mesma lógica de um printf() da linguagem C, denominada ```UTL_DBG_PRINTF()```. A diferença é que essa macro recebe como primeiro argumento a qual módulo o log se refere. Se esse módulo estiver com logs habilitados, esse log será impresso. A outra macro é a ```UTL_DBG_DUMP()```, usada principalmente para realizar dumps em formato parecido com o aplicativo hexdump do Linux.

A primeira ação nesse arquivo é definir a lista dos módulos existentes e qual bit ele irá usar na variável de controle. Isso poderia ter sido feito como a seguir:

```C
#pragma once

#define UTL_DBG_MOD_APP 0
#define UTL_DBG_MOD_SER 1
#define UTL_DBG_MOD_ADC 2
// novo define
```

Bastaria ir adicionando novos nomes em sequência à medida que seu firmware evolui, aumentando o valor da posição. No arquivo ```utl_dbg.c``` (apresentado posteriormente), o array que mantem os nomes dos módulos também precisaria ser atualizado a cada alteração na lista de módulos, adicionando-se um novo nome:

```C
const uint8_t* utl_log_mod_name[] = {
    (uint8_t*) "UTL_DBG_MOD_APP",
    (uint8_t*) "UTL_DBG_MOD_SER",
    (uint8_t*) "UTL_DBG_MOD_ADC",
    // nova string
};

Todos nós sabemos onde isso acaba: em erro ! Manter código que demanda alterações em várias partes é geralmente um pesadelo e fica muito fácil cometer erros. Uma forma de contornar esse problema é o uso de X macros [REF]. Com a X macro fica possível realizar alterações em apenas um lugar, mantendo a integridade do código. 

Para entender como isso funciona é preciso ficar claro como o pré-processador da linguagem C trabalha. Quando você cria definições personalizadas, o compilador não realiza nenhum operação sobre elas. Apenas faz anotações, salva o que foi associado e nem se precupa em fazer validações adicionais recursivas. Qualquer avaliação só será feita no momento real do uso da definição. 

No código, existe uma definição de uma macro chamada ```XMACRO_DBG_MODULES```. Essa macro é associada a uma outra macro, chamada de ```X(mod,index)```, com dois parâmetros: o nome do módulo e a index do bit relacionado. Note que a macro não foi definida ainda e tudo bem, afinal XMACRO_DBG_MODULES também não foi usada, logo não existe a necessidade de expansão ou avaliação das macros envolvidas.

Assim, o que se tem até agora é que XMACRO_DBG_MODULES será trocada, futuramente, pelas avaliações da macro X. Eu gosto de ver essa parte como uma base de dados, onde as linhas são os registros dados por macros X e os campos são separados são os argumentos da macro X, separados por vírgulas. Lembre-se que a backslash "\" serve apenas para continuar a macro na linha de baixo. No fundo, seria equivalente a se escrever como a seguir:

```C
#define XMACRO_DBG_MODULES X(UTL_DBG_MOD_APP, 0)  X(UTL_DBG_MOD_SER, 1)  X(UTL_DBG_MOD_ADC, 2)
```

Obviamente, a backslash traz mais legibilidade e facilidade de adição.

No momento em que o desenvolvedor pretende avaliar essa base de dados, ele lança mão da definição da macro X e uso da macro XMACRO_DBG_MODULES, forçando o pré-processador a avaliar tudo que estava pendente. Para entender isso, é melhor uma análise por partes. Primeiro veja a declaração da macro X:

```C
#define X(MOD, INDEX) MOD = INDEX,
```

Aqui, ela está apenas dizendo que, onde quer que X(mod,index) seja chamada, que é para colocar no lugar ```MOD = INDEX,```. Ou seja, se você usar X(TESTE,10) no seu código, isso deveria gerar ```TESTE = 10,```. Só que isso fica mais interessante ao pedir a avaliação de TODA a base de dados criada através da macro XMACRO_DBG_MODULES. É uma definição seguida de avaliação quando temos a seguinte construção (note que é necessário mudar de linha ou a definição da macro X não teria terminado como esperado):

```C
#define X(MOD, INDEX) MOD = INDEX,
    XMACRO_DBG_MODULES
```

O trecho acima irá gerar o seguinte bloco:

```C
UTL_DBG_MOD_APP = 0,
UTL_DBG_MOD_SER = 1,
UTL_DBG_MOD_ADC = 2,
```

No entanto, esse bloco está ao redor de uma declaração de enumeração. Ou seja, o trecho abaixo:

```C
typedef enum utl_dbg_modules_e
{
#define X(MOD, INDEX) MOD = INDEX,
    XMACRO_DBG_MODULES
#undef X
} utl_dbg_modules_t;
```

irá gerar o seguinte código quando o pré-processador avaliar tudo:

```C
typedef enum utl_dbg_modules_e
{
UTL_DBG_MOD_APP = 0,
UTL_DBG_MOD_SER = 1,
UTL_DBG_MOD_ADC = 2,
} utl_dbg_modules_t;
```

Mágico, não ? Uma ação importante é remover a definição da macro X logo após o uso de forma a permitir que outras interações com a base de dados seja feita. Veremos isso na hora de criar o array de nomes de módulos de forma automática, feito pelo trecho a seguir:

```C
const uint8_t* utl_log_mod_name[] = {
#define X(MOD, INDEX) (uint8_t*) #MOD,
    XMACRO_DBG_MODULES
#undef X
};
```

Para entender o que vai acontecer é preciso explicar mais um aspecto do pré-processador conhecido como "stringfy", no caso o operador "#" que aparece antes de MOD. O stringfy faz o que o nome diz, isto é, gera uma string a partir de um símbolo. Ou seja, MOD não será usado literalmente mas sim substituído pela string equivalente. Se MOD valer UTL_DBG_MOD_APP, #MOD gera "UTL_DBG_MOD_APP". 

Com isso fica mais fácil entender que o pré-processador irá gerar o seguinte código:

```C
const uint8_t* utl_log_mod_name[] = {
(uint8_t*) "UTL_DBG_MOD_APP",
(uint8_t*) "UTL_DBG_MOD_SER",
(uint8_t*) "UTL_DBG_MOD_ADC",
};
```

Note que INDEX não foi usado dentro da definição da macro X, não foi preciso, e que depois foi feita também uma remoção da macro X via undef. Mágico e útil, não ?

Para finalizar, duas observações adicionais sobre o pré-processador. A primeira é sobre a concatenação automática de strings. Por exemplo, se você faz algo como

```C
#define TXT  "ABC"  "123"
```

o pré-processador vai juntar as string acima e TXT irá valer "ABC123". Além disso, "__FILE__" e "__LINE__" são automaticamente substituídas pelo nome do arquivo e da linha em pré-processamento naquele momento. A macro UTL_LOG_HEADER faz essa composição.

Para entender, imagine o seguinte uso da macro UTL_DBG_PRINTF():

```C
UTL_DBG_PRINTF(UTL_DBG_MOD_APP,"%d -> %d",10,20)
```

Isso irá gerar a seguinte saída (imaginando que UTL_DBG_PRINTF() foi usada na linha 123 do arquivo port_ser.c e que o módulo está habilitado):

```C
[UTL_DBG_MOD_APP][port_ser.c:123] 10 -> 20
```

A macro UTL_DBG_PRINTF faz a adição do header do log aos parâmetros passados pelo usuário, através da chamada da macro UTL_LOG_HEADER(), imprimindo informações sobre o módulo, arquivo e linha. Novamente, existem alguns detalhes nesse processo, é melhor ir por partes. Veja a definição da macro UTL_LOG_HEADER():

```C
#define UTL_LOG_HEADER(mod, fmt, file, line) "[%s][%s:%d] " fmt, (char*) utl_dbg_mod_name(mod), file, line
```

Sabendo que utl_dbg_mod_name(mod) devolve uma string com o nome do módulo, uma chamada dessa macro com os parâmetros usados anteriormente faria o pré-processador trabalhar e gerar o seguinte resultado:

```C
UTL_LOG_HEADER(UTL_DBG_MOD_APP, "%d -> %d", __FILE__,__LINE__) => "[%s][%s:%d] %d -> %d", (char*) utl_dbg_mod_name(UTL_DBG_MOD_APP), "port_ser.c", 123
```

Perceba que foram adicionadas as informações de formatação do nome do módulo, arquivo e linha ao conjunto de especificações de impressão do usuário, gerando uma string única. Além disso, os valores de arquivo e linha corrente foram substituídos (lembre-se que está tudo na mesma linha, no fundo, devido ao o backslash). Dois elementos ainda estão obscuros: o operador "##" presente dentro da chamada do printf() e a macro __VA_ARGS__. 

Apesar de não recomendado, como já dito, é possível criar uma macro com parâmetros variáveis, como se tem no printf() ou mesmo no suporte nativo da linguagem C. No caso, a macro __VA_ARGS__ representa a lista de parãmetros passada. Já o operador "##", também conhecido como [NOME REF], apenas juntas as duas coisas em uma só. Ou seja, teremos o seguinte na composição final da macro de debug:

```C
printf("[%s][%s:%d] %d -> %d", (char*) utl_dbg_mod_name(UTL_DBG_MOD_APP), "port_ser.c", 123, 10, 20);
```

É isso. O "do { } while(0)" é somente uma forma de fazer um bloco em C, poderia ter sido feito de outra forma. O código apenas fica mais resiliente apossíveis  erros quando se coloca todas essas macros dentro de um bloco "{}", criando um contexo local. 

A implementação do módulo "utl_dbg.c" é relativamente simples, dada a seguir. Perceba que a variável "utl_dbg_mods_activated" controla os módulos ativos no momento, como um campo de bits, como já mencionado.

```C
#include <stdint.h>
#include <stdbool.h>

#include "hal_dbg.h"

const uint8_t* utl_log_mod_name[] = {
#define X(MOD, INDEX) (uint8_t*) #MOD,
    XMACRO_DBG_MODULES
#undef X
};

static uint32_t utl_dbg_mods_activated = 0;

const uint8_t* utl_dbg_mod_name(utl_dbg_modules_t mod_idx)
{
    return (uint8_t*) utl_log_mod_name[mod_idx];
}

void utl_dbg_mod_enable(utl_dbg_modules_t mod_idx)
{
    utl_dbg_mods_activated |= 1 << mod_idx;
}

void utl_dbg_mod_disable(utl_dbg_modules_t mod_idx)
{
    utl_dbg_mods_activated &= ~((uint32_t) (1 << mod_idx));
}

bool utl_dbg_mod_is_enabled(utl_dbg_modules_t mod_idx)
{
    return (utl_dbg_mods_activated & (1 << mod_idx)) > 0;
}

void utl_dbg_dump_lines(char* stamp, uint8_t* data, uint16_t size)
{
    uint8_t* ptr = data;

    printf("%s", stamp);

    for(uint32_t pos = 0; pos < size; pos++)
    {
        if(pos && (pos % 32 == 0))
            printf("\n%s", stamp);

        if(pos % 32 == 0)
            printf("%04X ", (unsigned int) pos);

        printf("%02X", *ptr++);
    }
    printf("\n");
}

void utl_dbg_init(void)
{
    utl_dbg_mod_enable(UTL_DBG_LEVEL_APP);
}
```

### Wrapping up

Toda essa discussão nos leva a um novo ponto: onde afinal termina o printf() ? Num sistema embarcados sem sistema operacional, não temos os tradicionais arquivos de entrada e saída, como stdout e stdin, o que de fato acontece ? Para entender isso é preciso discutir um pouco sobre system calls e como realizar os redirecionamento para que consigamos ter as impressões desejadas.

# System Calls






=> exe: callback para interrupção
=> exe: abstrair outro modulo
=> 