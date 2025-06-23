# Interrupções no Cortex M4

> [!NOTE]
> :brain:

Antes de lançar a linha Cortex M, a ARM tinha alguns processadores chamados [ARM7TDMI](https://en.wikipedia.org/wiki/ARM7) como foco em sistemas embarcados industriais apesar de faltarem algumas características importantes. Nesses processadores, existiam dois grupos de interrupções: as do núcleo, denominadas de FIRQ (_Fast Interrupt Request_) e as do integrador, chamadas IRQ (_Interrupt Request_). As interrupções FIRQ eram sempre tratadas com prioridade maior que IRQ, o que significava que uma interrupção FIRQ não poderia ser interrompida por uma IRQ. 

Com isso, as interrupções do integrador tinham a desvantagem de ter uma latência variável, além de não possuírem um sistema de interrupções aninhadas baseado em prioridades. Para quem desejava um controle temporal fino sobre o sistema, isso era um enorme problema. Esses processadores não tinham ponto flutuante e suportavam o que a ARM chama hoje de Thumb2, que é um conjunto de instruções misto com instruções de 16 e 32 bits. Era necessário escolher se o conjunto a ser executado seria de 16 ou 32bits e existia um procedimento para alternar entre esses conjuntos de instruções. 

Mesmo assim, o ARM7TDMI foi um processador muito popular e provavelmente você usou um deles se teve um iPod Clássico, um Nokia 6110 ou mesmo um Microsoft Zune ou Lego Mindstorms de segunda geração. Inúmeras empresas fizeram produtos com esses processadores, na época fornecidos pela Analog Devices, Atmel, NSP, ST, Mediatek, Nuvoton, Samsung, entre outras. Ok, acabamos de denunciar a nossa idade.

O Cortex M foi a solução da ARM para todos esses problemas. Ele é um processador de 32 bits, com ponto flutuante, que suporta o conjunto de instruções Thumb2 e tem um sistema de interrupções aninhadas baseado em prioridades. Apesar de não possuir mais as FIRQ, é de certa forma notório que as interrupções do núcleo são tratadas de forma diferente das interrupções do integrador, como já descrito na introdução e que parece ter relação com a herança dos processadores ARM7TDMI. Apenas para ficar temporalmente claro, o Cortex M3 foi o primeiro lançamento da linha Cortex M, seguido pelo Cortex M4 e depois o Cortex M0.

Agora, se existe algo confuso no Cortex M4 é como o sistema de interrupções é explicado e discutido na literatura. Alguns fatores contribuem para essa confusão:

- A documentação não deixa claro que as interrupções do Cortex M4 (chamadas de exceções) e as do integrador do microcontrolador (essas sim, interrupções) são tratadas de formas diferentes. Existem registros diferentes para habilitar e desabilitar essas interrupções (vamos chamar tudo de interrupção daqui pra diante) e algumas coisas só se aplicam a interrupções integrador, como o BASEPRI (explicado mais adiante), não ao Cortex M4.
- A quantidade de níveis de prioridade é dependente do integrador, assim como a quantidade de interrupções que o integrador pode usar, existindo apenas o limite de 240 interrupções no Cortex M4. Isso, associado à possibilidade de organizar as prioridades em grupos e subgrupos, dificulta a compreensão do sistema de interrupções. 
- A confusão aumenta ao numerar as interrupções. Existe uma numeração onde interrupções do Cortex M4 são números negativos e as do integrador começam em zero. No fundo, isso tem mais relação com a forma como a enumeração das interrupções é tratada no CMSIS pela funções `NVIC_SetPriority()`, um atalho para deixar o código mais simples, uma vez que o conjunto de registros que trata as interrupções do Cortex M4 está no SCB (_System Control Block_) e as interrupções do integrador são controladas pelo NVIC (_Nested Vectored Interrupt Controller_). 
- Pra piorar, existe uma outra numeração relacionada à tabela de vetores de interrupção que, obviamente, não vai ter número negativos para representar um index para a escolha de um endereço dentro dessa tabela. Assim, uma nova numeração é criada em várias documentações existentes. Junte que o primeiro endereço da tabela é o valor do stack pointer e não um endereço de função de tratamento de interrupção e isso piora ainda mais.
- A cereja do bolo vem do fato de que algo mais prioritário tem valor numérico menor, o que dificulta o entendimento e a comunicação entre desenvolvedores. Essa relação é especialmente perversa ao usar o recurso BASEPRI.

Nessa seção vamos esclarecer esses pontos e discutir as funções de tratamento de interrupções presentes no CMSIS do Cortex M4.

## Interrupções do núcleo Cortex M4

No Cortex M4, o tratamento das interrupções passa a ser feito pelo componentes NVIC, que é o controlador de interrupções aninhadas vetorizadas. Enquanto a origem da interrupções possam variar, vindo do núcleo ou do integrador, o NVIC é o responsável por gerenciar todas interrupções. O fato de serem originadas de locais diferentes muda apenas a forma como essas interrupções são habilitadas e priorizadas, mas o tratamento é sempre feito pelo NVIC.

Como discutido na seção sobre a partida do processador, existe uma tabela de vetores de interrupção que deve ser escrita no endereço de partida do processador, em geral o endereço `0x00000000`. Essa tabela contém os endereços das funções de tratamento de interrupção, que são chamadas quando uma interrupção ocorre. O primeiro endereço da tabela é o valor do stack pointer, que é usado pelo processador para saber onde está o topo da pilha, não sendo um endereço de função de tratamento de interrupção.

Vamos apresentar as interrupções seguindo essa tabela de vetores de interrupção. A primeira coluna é o index dentro dessa tabela. Depois temos o valor usado na enumeração do CMSIS. Informações adicionais são apresentadas, como se a interrupção é mascarável ou não, onde é configurada e uma breve descrição da interrupção.

A seguir é apresentada uma tabela com as interrupções presentes no Cortex M4, indexadas da forma como o código é construído. No fundo, essa tabela deve ser vista, ou seja, a tabela 

| Index da Interrupção | Enumeração do CMSIS | Mascarável? | Configuração de Prioridade | Descrição |
|-------------|-----------|-----------|-----------|-----------|
|  0 |  -- |  -- | -- | Endereço inicial do Stack Pointer |
|  1 |  -- | Não | -- | Endereço da função de Reset |
|  2 | -14 | Não | -- | NMI (Non-Maskable Interrupt) |
|  3 | -13 | Não | -- | HardFault |
|  4 | -12 | Sim | SCB->SHPR1[7:0] | MemManage Fault |
|  5 | -11 | Sim | SCB->SHPR1[15:8] | Bus Fault |
|  6 | -10 | Sim | SCB->SHPR1[23:16] | Usage Fault |
|  7 |  -- | --  | -- | Reservado |
|  8 |  -- | --  | -- | Reservado |
|  9 |  -- | --  | -- | Reservado |
| 10 |  -- | --  | -- | Reservado |
| 11 | -5  | Sim | SCB->SHPR2[31:24]  | SVC |
| 12 | -4  | Sim | SCB->SHPR3[7:0] | Debug Monitor |
| 13 |  -- | --  | -- | Reservado |
| 14 | -2  | Sim | SCB->SHPR3[23:16]| PendSV |
| 15 | -1  | Sim | SCB->SHPR3[31:24]  | SysTick |
| 16-255 | 0-239 | Sim | NVIC->IPR | Interrupções do integrador |




