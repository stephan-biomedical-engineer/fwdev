# Interrupções no Cortex M4

> [!NOTE]
> :brain:

Antes de lançar a linha Cortex M, a ARM tinha alguns processadores chamados [ARM7TDMI](https://en.wikipedia.org/wiki/ARM7) com foco em sistemas embarcados, apesar de faltarem algumas características importantes para quem precisava de tempo real. Nesses processadores, existiam dois grupos de interrupções: as do núcleo, denominadas de FIRQ (_Fast Interrupt Request_) e as do integrador, chamadas IRQ (_Interrupt Request_). As interrupções FIRQs eram sempre tratadas com prioridade maior do que as IRQs, o que significava que uma interrupção FIRQ não poderia ser interrompida por uma IRQ. Com isso, as interrupções do integrador tinham a desvantagem de ter uma latência variável. Além disso, o processador não possuía um sistema de interrupções aninhadas baseado em prioridades como temos hoje num Cortex M e nenhuma interrupção não maascarável.

Para quem desejava um controle temporal fino sobre o sistema, isso era um enorme problema. Esses processadores não tinham ponto flutuante e suportavam o que a ARM chama hoje de Thumb2, que é um conjunto de instruções misto com instruções de 16 e 32 bits. Era necessário escolher se o conjunto a ser executado seria de 16 ou 32 bits e existia um procedimento para alternar entre esses conjuntos de instruções. 

Mesmo assim, o ARM7TDMI foi um processador muito popular e provavelmente você usou um deles se teve um iPod clássico, um Nokia 6110 ou mesmo um Microsoft Zune ou Lego Mindstorms de segunda geração. Inúmeras empresas fizeram produtos com esses processadores, entre elas fabricantes como Analog Devices, Atmel, NXP, ST, Mediatek, Nuvoton, Samsung, entre outras. Ok, acabamos de denunciar a nossa idade.

O Cortex M foi a solução da ARM para todos esses problemas. Ele é um processador de 32 bits, com ponto flutuante, que suporta o conjunto de instruções Thumb2 e tem um sistema de interrupções aninhadas baseado em prioridades. Apesar de não possuir mais as FIRQ, é de certa forma notório que as interrupções do núcleo são tratadas de forma diferente das interrupções do integrador, como já descrito na introdução e que parece ter relação com a herança dos processadores ARM7TDMI. Apenas para ficar temporalmente claro, o Cortex M3 foi o primeiro lançamento da linha Cortex M, seguido pelo Cortex M4 e depois o Cortex M0.

Agora, se existe algo confuso no Cortex M4 é como o sistema de interrupções é explicado e discutido na literatura. Alguns fatores contribuem para essa confusão:

- A documentação não deixa muito claro que as interrupções do Cortex M4 (chamadas de exceções) e as do integrador do microcontrolador (essas sim, denominadas de interrupções) são tratadas de formas diferentes. Existem registros diferentes para habilitar e desabilitar essas interrupções (vamos chamar tudo de interrupção daqui pra diante) e algumas coisas só se aplicam a interrupções do integrador, como o BASEPRI (explicado mais adiante), não a exceções do Cortex M4. O CMSIS é quem acaba deixando tudo com a mesma apresentação, escondendo os detalhes internos.
- A quantidade de níveis de prioridade é dependente do integrador, assim como a quantidade de interrupções que o integrador pode usar, existindo apenas o limite de 240 interrupções do integrador no Cortex M4. Isso, associado à possibilidade de organizar as prioridades em grupos e subgrupos, dificulta a compreensão do sistema de interrupções. 
- A confusão aumenta ao numerar as interrupções. Existe uma numeração onde interrupções do Cortex M4 são números negativos e as do integrador começam em zero. No fundo, isso tem mais relação com a forma como a enumeração das interrupções é tratada no CMSIS pela funções `NVIC_SetPriority()`, um atalho para deixar o código mais simples, uma vez que o conjunto de registros que trata as interrupções do Cortex M4 está no SCB (_System Control Block_) e as interrupções do integrador são controladas pelo NVIC (_Nested Vectored Interrupt Controller_). 
- Para piorar, existe uma outra numeração relacionada à tabela de vetores de interrupção que, obviamente, não vai ter número negativos para representar um index para a escolha de um endereço dentro dessa tabela. Assim, uma nova numeração é criada em várias documentações existentes. Junte que o primeiro endereço da tabela é o valor do stack pointer e não um endereço de função de tratamento de interrupção e isso piora ainda mais.
- A cereja do bolo vem do fato de que algo mais prioritário tem valor numérico menor, o que dificulta o entendimento e a comunicação entre desenvolvedores. Essa relação é especialmente perversa ao usar o recurso BASEPRI.

Nessa seção vamos esclarecer esses pontos e discutir as funções de tratamento de interrupções presentes no CMSIS do Cortex M4.

## Interrupções do núcleo Cortex M4

No Cortex M4, o tratamento das interrupções passa a ser feito pelo componentes NVIC, que é o controlador de interrupções aninhadas vetorizadas. Enquanto a origem das interrupções possa variar, vindo do núcleo Cortex ou do integrador, o NVIC é o responsável por gerenciar todas interrupções. O fato de serem originadas de locais diferentes muda apenas a forma como essas interrupções são habilitadas e priorizadas, mas o tratamento é sempre feito pelo componente NVIC, com as mesmas regras.

Como discutido na seção sobre a [partida do processador](./startup.md), existe uma tabela de vetores de interrupção que deve ser escrita no endereço de partida do processador, em geral o endereço `0x00000000`. Essa tabela contém os endereços das funções de tratamento de interrupção, que são chamadas quando uma interrupção ocorre. A exceção é o primeiro endereço da tabela cujo valor é, no fundo, o stack pointer, usado pelo processador para saber onde está o topo da pilha, não sendo um endereço de função de tratamento de interrupção.

Vamos apresentar as interrupções seguindo os vetores de interrupção. A primeira coluna é o index dentro dessa tabela, lembrando que cada endereço é de 4 bytes, logo o índice 4 está relacionado ao endereço 16 (0x00000010). Depois temos o valor usado na enumeração do CMSIS onde os valores negativos são úteis para o CMSIS calcular o registro SHP (_System Handler Priority Register_) correto dentro do SCB. Informações adicionais são apresentadas indicando se a interrupção é mascarável ou não, quais são os registros que configuram a prioridade e uma breve descrição da interrupção.

| Index da Interrupção | Enumeração do CMSIS | Mascarável? | Configuração de Prioridade | Descrição |
|-------------|-----------|-----------|-----------|-----------|
|  0 |  -- |  -- | -- | Endereço inicial do Stack Pointer |
|  1 |  -- | Não (-3) | -- | Endereço da função de Reset |
|  2 | -14 | Não (-2) | -- | NMI (Non-Maskable Interrupt) |
|  3 | -13 | Não (-1) | -- | HardFault |
|  4 | -12 | Sim | SCB→SHPR1[7:0] | MemManage Fault |
|  5 | -11 | Sim | SCB→SHPR1[15:8] | Bus Fault |
|  6 | -10 | Sim | SCB→SHPR1[23:16] | Usage Fault |
|  7 |  -- | --  | -- | Reservado |
|  8 |  -- | --  | -- | Reservado |
|  9 |  -- | --  | -- | Reservado |
| 10 |  -- | --  | -- | Reservado |
| 11 | -5  | Sim | SCB→SHPR2[31:24]  | SVC |
| 12 | -4  | Sim | SCB→SHPR3[7:0] | Debug Monitor |
| 13 |  -- | --  | -- | Reservado |
| 14 | -2  | Sim | SCB→SHPR3[23:16]| PendSV |
| 15 | -1  | Sim | SCB→SHPR3[31:24]  | SysTick |
| 16-255 | 0-239 | Sim | NVIC→IPR | Interrupções do integrador |

## Interrupções não mascaráveis e mascaráveis

> [!NOTE]
> :robot:

As interrupções no Cortex M4 podem ser divididas em dois grandes grupos: as mascaráveis e as não mascaráveis. Interrupções mascaráveis são aquelas que podem ser temporariamente desabilitadas ou postergadas por mecanismos internos do processador ou por software, como o uso dos registradores PRIMASK, BASEPRI ou FAULTMASK. Por outro lado, interrupções não mascaráveis são sempre executadas quando ocorrem, não podendo ser inibidas por esses mecanismos.

As exceções não mascaráveis no Cortex M4 incluem a exceção de Reset, a NMI (Non-Maskable Interrupt) e o HardFault. Essas exceções são utilizadas para eventos críticos e não podem ser desabilitadas por nenhum mecanismo de mascaramento como PRIMASK, BASEPRI ou FAULTMASK (vistos mais adiante). A ordem de prioridade e atendimento dessas exceções é fixa e determinada pela arquitetura: o Reset possui a prioridade mais alta, seguido pela NMI e, por fim, o HardFault. Na tabela, elas são indicadas por prioridades negativas, -3, -2 e -1. No fundo, é apenas uma forma de indicar que são mais prioritárias do que a maior prioridade configurável, que é a prioridade 0, não existe de fato uma prioridade negativa.

## Relação entre HardFault, Bus Fault, MemManage Fault e Usage Fault

> [!NOTE]
> :robot:

O Cortex M4 conta com quatro exceções principais para tratamento de falhas de execução: *HardFault*, *Bus Fault*, *MemManage Fault* e  *Usage Fault*. Embora possam parecer similares à primeira vista, elas possuem propósitos distintos e níveis de criticidade diferentes.

O *MemManage Fault* está relacionado a violações de acesso à memória protegida, como acesso a regiões não permitidas pelo controle de memória (MPU - Memory Protection Unit). O *Bus Fault* está ligado a falhas de acesso ao barramento, como acessos inválidos ou erros em periféricos. Já o *Usage Fault* trata erros de uso da CPU, como execução de instruções inválidas, divisão por zero e violação de alinhamento. Essas três exceções podem ser habilitadas ou desabilitadas individualmente por meio de bits de controle no registrador `SCB->SHCSR` (_System Handler Control and State Register_). Quando essas exceções estão desabilitadas e um erro correspondente ocorre, a falha não deixa de ser tratada: ela é automaticamente redirecionada ao manipulador de *HardFault*. Isso ocorre porque o *HardFault* atua como um “guarda-chuva” para qualquer exceção crítica não tratada diretamente. Por esse motivo, o *HardFault* não pode ser desabilitado — ele assegura que falhas graves não passem despercebidas, mesmo quando outros mecanismos de exceção estão inativos.

Essa hierarquia de tratamento oferece uma abordagem robusta e flexível, permitindo que desenvolvedores configurem o comportamento do sistema conforme as necessidades específicas de desenvolvimento ou produção, enquanto mantêm um nível mínimo de proteção contra falhas críticas.

## Uso do CMSIS para Controle de Interrupções

> [!NOTE]
> :robot: :brain:

O CMSIS (Cortex Microcontroller Software Interface Standard) fornece um conjunto padronizado de funções para configurar e manipular as interrupções no Cortex M4 de forma portável e simples. As principais funções envolvidas no controle das interrupções são:

- `NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority)`: Define a prioridade de uma interrupção específica. O parâmetro IRQn indica a interrupção a ser configurada e priority define o nível de prioridade (valores menores indicam prioridade mais alta, por convenção do NVIC). O número de níveis disponíveis depende da implementação do microcontrolador e da quantidade de bits utilizados para codificar a prioridade.
- `NVIC_GetPriority(IRQn_Type IRQn)`: Retorna a prioridade atual da interrupção especificada.
- `NVIC_EnableIRQ(IRQn_Type IRQn)`: Habilita a interrupção identificada por IRQn, permitindo que ela dispare se ocorrer um evento correspondente.
- `NVIC_DisableIRQ(IRQn_Type IRQn)`: Desabilita a interrupção, impedindo que ela seja atendida mesmo que o evento ocorra.
- `NVIC_SetPendingIRQ(IRQn_Type IRQn)`: Sinaliza manualmente uma interrupção como pendente, forçando sua execução se estiver habilitada.
- `NVIC_ClearPendingIRQ(IRQn_Type IRQn)`: Limpa o estado de pendência de uma interrupção, impedindo sua execução caso ainda não tenha ocorrido.

A enumeração IRQn_Type define os identificadores simbólicos para todas as interrupções disponíveis em um microcontrolador baseado em Cortex M. Como comentado anteriormente, a enumeração tem valores negativos para a parte das exceções do Cortex M. Por exemplo, para o STM32F411, uma parte da definição pode ser vista a seguir:

```C copy
typedef enum
{
  NonMaskableInt_IRQn         = -14,
  MemoryManagement_IRQn       = -12, 
  BusFault_IRQn               = -11,
  UsageFault_IRQn             = -10
  SVCall_IRQn                 = -5, 
  DebugMonitor_IRQn           = -4, 
  PendSV_IRQn                 = -2, 
  SysTick_IRQn                = -1, 
  /* custom Interrupt Numbers */
  WWDG_IRQn                   = 0,  
  PVD_IRQn                    = 1,
  TAMP_STAMP_IRQn             = 2,
  // ... more interrupts
} IRQn_Type;
```
 Esses identificadores são utilizados como argumentos nas chamadas de funções do CMSIS, como `NVIC_SetPriority()` e `NVIC_EnableIRQ()`, para selecionar a interrupção desejada de maneira abstrata e portável. Cada valor dessa enumeração corresponde a uma posição na tabela de vetores de interrupção.

Como referência, veja a implementação da função `NVIC_SetPriority()` do CMSIS para o Cortex M4, evidenciando o tratamento para as definições com valores negativos.

```C copy
__STATIC_INLINE void __NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority)
{
  if ((int32_t)(IRQn) >= 0)
  {
    NVIC->IP[((uint32_t)IRQn)]= (uint8_t)((priority << (8U - __NVIC_PRIO_BITS)) & (uint32_t)0xFFUL);
  }
  else
  {
    SCB->SHP[(((uint32_t)IRQn) & 0xFUL)-4UL] = (uint8_t)((priority << (8U - __NVIC_PRIO_BITS)) & (uint32_t)0xFFUL);
  }
}
```

Percebam que existe um shift da prioridade antes de se fazer a atribuição no registro. Esse ponto merece uma explicação e está relacionado ao fato de as interrupções em Cortex M3, M4 e M7 possuírem a divisão de prioridades em grupos e subgrupos. 

O número de bits utilizados para a prioridade é definido pelo parâmetro `__NVIC_PRIO_BITS`, que é configurável no CMSIS e dependente do processador utilizado. Por exemplo, se `__NVIC_PRIO_BITS` for 4, as prioridades vão de 0 a 15, onde 0 é a prioridade mais alta e 15 é a mais baixa. 

É possível ainda dividir esses 4 bits em grupo e subgrupo, alocando uma quantidade de bits para cada parte. Por exemplo, se tivermos 2 bits para o grupo e 2 bits para o subgrupo, a prioridade total será representada por 4 bits, mas composta de 4 grupos e cada grupo com 4 com possibilidades de prioridade de subgrupo. A quantidade de bits alocados para cada grupo e subgrupo é configurável no registro `AIRCR` (_Application Interrupt and Reset Control Register_) do SCB (_System Control Block_), `SCB→AIRCR[10:8]`. No fundo, isso não altera a quantidade de prioridades, mas apenas a forma como elas são organizadas, podendo ser útil para organizar as interrupções de forma hierárquica.

Para finalizar, como a configuração da prioridade usa apenas alguns bits, pensando num campo de 8 bits, existem bits não usados. Por exemplo, se temos 4 bits para codificar a prioridade, os outros 4 bits não são usados. Os bits usados ficam na parte mais significativa e os não usados na parte menos significativa, sendo que a ARM decidiu que esses bits não usados devem ser representados com zeros. Assim, uma configuração de prioridade 10 (0x0A ou 00001010b, em binário) sem subgrupos, seria vista como:

```
PRIO = 0x0A << 4 | 0x00
PRIO = 10100000b = 0xA0
```

Usando 1 bit para grupo e 3 bits para subgrupo, a configuração de prioridade 1 para grupo e 2 para subgrupo seria vista como:

```
PRIO = (0x01 << 3 | 0x02) << 4 | 0x00
PRIO = 10100000b = 0xA0
```

No fundo, a prioridade final é a mesma nos dois casos (0xA0) e o CMSIS faz esse ajuste automaticamente ao atribuir a prioridade ao registro de interrupção, lendo a configuração do `AIRCR` e aplicando o shift necessário.

## Habilitando e Desabilitando Interrupções de Forma Global

O Cortex-M3/M4 oferece dois registradores especiais para controlar interrupções globalmente, de forma rápida e eficiente, sem depender do NVIC, através dos registradores PRIMASK e FAULTMASK. Apesar de serem de 32 bits, apenas o bit 0 é utilizado, sendo que o restante é reservado e deve ser mantido em zero. Esses registradores são úteis para desabilitar todas as interrupções mascaráveis de uma só vez, sem precisar configurar cada uma individualmente.

O PRIMASK	pode desabilitar todas as interrupções mascaráveis, ou seja, NMI e HardFault não são afetadas. Seria algo equivalente a desabilitar todas as interrupções maiores ou iguais a zero (lembre-se que existem os níveis simbólicos negativos, de -3 a -1).

As funções CMSIS para manipular o PRIMASK são:

```c copy
__disable_irq();  // Usa a instrução CPSID I
__enable_irq();   // Usa a instrução CPSIE I
// ou
void __set_PRIMASK(uint32_t priMask); // Usa a instrução MSR PRIMASK
uint32_t __get_PRIMASK(void); // Usa a instrução MRS PRIMASK
````

O registrador FAULTMASK atua de forma similar ao PRIMASK, mas com escopo ainda mais restritivo. Quando FAULTMASK = 1, o processador desabilita todas as interrupções mascaráveis, inclusive a exceção HardFault. Seria algo equivalente a  desabilitar todas as interrupções maiores ou iguais -1, ode somente NMI (Non-Maskable Interrupt) permanece habilitada. Além disso, o FAULTMASK só pode ser ativado dentro de uma interrupção (mas que não pode ser nem NMI nem Fault handler) e é automaticamente resetado ao retornar da interrupção. Isso gera cenários muitos específicos de uso desse recursos, como por exemplo:

- o FAULTMASK pode ser usado por tratadores de exceção configuráveis (como BusFault, MemManage e UsageFault) quando desejam garantir exclusividade total para tratar uma falha.  Isso garante que, enquanto o FAULTMASK estiver ativo, nenhuma outra interrupção mascarável será atendida, incluindo a HardFault, permitindo ignorar falhas de barramento ou proteções de área de memória, por exemplo, dentro de um tratador de falha de hardware como UsageFault.
- um outro caso de uso peculiar é forçar a execução de uma interrupção de maior prioridade apenas após o término da interrupção corrente, mesmo que esta última tenha prioridade inferior. Isso é possível porque o FAULTMASK é automaticamente limpo (setado para 0) ao sair de um handler de exceção (exceto NMI). Assim, a interrupção pendente só será atendida após o FAULTMASK ser automaticamente limpo, ao sair do handler.

As funções CMSIS para manipular o FAULTMASK são:

```c copy
void __enable_fault_irq(void); // Usa a instrução CPSIE F
void __disable_fault_irq(void); // Usa a instrução CPSID F
// ou
void __set_FAULTMASK(uint32_t faultMask); // Usa a instrução MSR FAULTMASK
uint32_t __get_FAULTMASK(void); // Usa a instrução MRS FAULTMASK
```

E um exemplo de uso do FAULTMASK para forçar a execução de uma interrupção de maior prioridade após o término da interrupção corrente, poderia ser:

```C copy
__set_FAULTMASK(1);
NVIC_SetPendingIRQ(HigherIRQn);
// Handler atual continua...
//
// Ao retornar, FAULTMASK é limpo e HigherIRQn é atendida
```


## Cuidados no Uso de `__disable_irq()` e `__enable_irq()`

> [!NOTE]
> :robot:

As funções `__disable_irq()` e `__enable_irq()` são frequentemente utilizadas aos pares para proteger **regiões críticas**, isto é, trechos de código que acessam recursos compartilhados e que não devem ser interrompidos durante sua execução. Um uso típico seria:

```c copy
__disable_irq();
// Região crítica
__enable_irq();
```

No entanto, o uso ingênuo dessas funções pode introduzir um erro sutil quando chamadas aninhadas de `__disable_irq()` e `__enable_irq()` ocorrem. Suponha o seguinte cenário:

```c copy
void rotina_interna(void) {
    __disable_irq();
    // Operações internas
    __enable_irq(); // reabilita interrupções
}

void rotina_externa(void) {
    __disable_irq();
    rotina_interna(); 
    // interrupções são reabilitadas prematuramente
    // Região crítica sem proteção
    __enable_irq();
}
```

No exemplo acima, a chamada `__enable_irq()` dentro da função `rotina_interna()` reabilita as interrupções enquanto a função `rotina_externa()` ainda não terminou sua região crítica. Isso viola a proteção pretendida e pode resultar em **condições de corrida**, ou seja, acesso concorrente a recursos e falhas de sincronização.

A maneira mais segura de garantir a restauração do estado original do PRIMASK é utilizar as funções `__get_PRIMASK()` e `__set_PRIMASK()` com salvamento e restauração do valor do registrador, conforme o exemplo abaixo:

```c copy
uint32_t primask = __get_PRIMASK();
__disable_irq();

// Região crítica

__set_PRIMASK(primask); // restaura o estado anterior do PRIMASK
```

Com essa abordagem, é possível garantir que:

- Se as interrupções já estavam desabilitadas antes da entrada na região crítica, continuarão desabilitadas após a saída.
- Se estavam habilitadas, serão corretamente reabilitadas após o término da seção protegida.
- O código passa a ser seguro para **chamadas aninhadas** e uso inadvertido de múltiplos `__disable_irq()` e `__enable_irq()`.

Esse padrão é particularmente importante em bibliotecas reutilizáveis ou em sistemas de tempo real onde múltiplas camadas do software podem tentar controlar interrupções simultaneamente. 

## Alterando o nível de prioridade via BASEPRI


