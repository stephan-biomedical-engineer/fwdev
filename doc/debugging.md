<!--- cSpell:enable --->

# Depuração de firmware

> [!NOTE]
> :brain:

Todo mundo erra. É mais fácil admitir isso e tomar as precauções para eliminar rapidamente os erros. Em ambientes profissionais de desenvolvimento a "falha" de software, comumente chamada de "bug" ou "erro", pode ser mitigada de diversas formas, a depender de quanto investimento se tem disponível. Por exemplo, existem ambientes com desenvolvimento feito aos pares (dois programadores)e com código sendo revisto por outros, permitindo que o código seja analisado por várias pessoas antes de ser testado e colocado em produção. Também é muito comum a criação de código em "salas limpas", ambientes livres de interrupções para que o foco seja mantido no trabalho em desenvolvimento. A recente onda de IAs tem permitido também o uso de ferramentas automatizadas de revisão e sugestão de correção. Cada estratégia de desenvolvimento tem o seu custo.

Independente do seu orçamento, é interessante trabalhar com o objetivo de minimizar as falhas cometidas, tentando encontrar a maioria delas ainda no seu teste inicial de bancada. Para isso, é preciso lançar mão de ferramentas de depuração eficientes, técnicas de desenvolvimento apropriadas e muita análise. Nesse capítulo vamos discutir formas de trabalhar como foco em depuração, evitando propagar falhas no processo de desenvolvimento de software embarcado. O tópico é extenso e passa por boas abstrações de hardware, logs, testes automatizados e até mesmo uma discussão sobre _system calls_.

## Abstração para depuração

Um módulo de depuração é um elementos essencial para o desenvolvimento. Ele deve permitir que o desenvolvedor consiga imprimir mensagens de depuração, realizar dumps de memória e até mesmo gerar logs de eventos. Esses logs são essenciais para entender o que está acontecendo no firmware, onde o acesso ao hardware é limitado e a interação com o usuário é mínima.

Assim é apropriado criar um módulo portável de debug, ou simplesmente "dbg". O papel dele, como o nome sugere, é permitir a impressão de mensagens de depuração e dumps de dados. Em sistemas embarcados podemos fazer esses logs serem enviados via serial ou via JTAG/SWD. Num porte para PC um caminho possível seria o console ou mesmo a geração de arquivos. 

Adicionalmente, o módulo "dbg" vai permitir a introdução do conceito associado a _system calls_.

### Interface de depuração

Para a interface de depuração, vamos criar um arquivo de inclusão como definido a seguir. O módulo será denominado de "utl", de utilidades, uma vez que ele deve ser totalmente portável, apenas reusando interfaces de hardware, como uma possível UART. O que se pretende com o módulo é criar um mecanismo simples que permita ligar e desligar logs relacionados a partes específicas do programa em construção, direcionando esses logs para a saída escolhida.

> [!NOTE]
> :warning:

Vale a pena deixar aqui uma observação: esse módulo vai fazer uso de funções e macros com parâmetros variáveis, algo comumente condenado em aplicações embarcadas críticas por poder levar a usos indevidos de memória (alocação, estouros de pilha, etc). Enquanto entendemos o motivo óbvio, a ideia é que esses logs não façam parte do firmware final, sendo apenas para desenvolvimento e podendo ser integralmente desabilitados. Se o símbolo `UTL_DBG_DISABLED` for definido, o módulo não irá gerar nenhum log ou função associada, ignorando todas as chamadas. Assim, o código final não terá nenhum impacto no firmware, mas durante o desenvolvimento ele será muito útil.

> [!NOTE]
> :exploding_head:

Vamos aproveitar também para aprender um pouco mais sobre recursos avançados do pré-processador da linguagem C, como o operador **#** que transforma tokens em strings (_stringification_), o operador **##** de concatenação de tokens (_token pasting_) e a técnica **X macro**. Esses recursos são muito úteis para criar um código mais limpo e fácil de manter.

O arquivo `utl_log.h` é apresentado a seguir, inteiro. Ele será explicado a seguir.

https://github.com/marcelobarrosufu/fwdev/blob/3e74e216c87708848738c58acf72a1d1d5f7da11/source/utl/utl_dbg.h#L1-L62

Por usar muitas macros (e X macros !), o arquivo é relativamente complexo para um iniciante na linguagem C. Mas ele traz muita informação e conhecimento, valendo a pena investir algum tempo entendendo os seus detalhes.

O princípio básico de funcionamento desse arquivo (detalhes adiante) é que uma variável inteira onde cada bit está relacionado ao controle de impressão de mensagens de log para um determinado módulo de depuração. Assim, cada módulo pode ser ativado e desativado individualmente. 

Para a impressão, é criada uma macro que segue a mesma lógica de um printf() da linguagem C, denominada `UTL_DBG_PRINTF()`. A diferença é que essa macro recebe como primeiro argumento a qual módulo o log se refere. Se esse módulo estiver com logs habilitados, esse log será impresso. A outra macro é a `UTL_DBG_DUMP()`, usada principalmente para realizar dumps em formato parecido com o aplicativo `hexdump` do Linux. Essas chamadas são feitas via macro por dois motivos. O primeiro é poder desligar totalmente a macro, redefinindo-a quando o símbolo `UTL_DBG_DISABLED` for definido. O segundo é para poder coletar informações do nome do arquivo e da linha onde a macro foi chamada, algo que não é possível fazer diretamente com uma função.

A primeira ação nesse arquivo é definir a lista dos módulos existentes e qual posição do bit ele irá usar na variável de controle. Isso foi criado com uma macro X e poderia ter sido feito de forma mais simples, como a seguir:

```C
#pragma once

#define UTL_DBG_MOD_APP 0
#define UTL_DBG_MOD_SER 1
#define UTL_DBG_MOD_ADC 2
// novo define
```

Bastaria ir adicionando novos nomes em sequência à medida que seu firmware evolui, aumentando o valor da posição. No arquivo `utl_dbg.c` (apresentado posteriormente), o array que mantem os nomes dos módulos também precisaria ser atualizado a cada alteração na lista de módulos, adicionando-se novos nomes de módulos no array `utl_log_mod_name[]`:

```C
const uint8_t* utl_log_mod_name[] = {
    (uint8_t*) "UTL_DBG_MOD_APP",
    (uint8_t*) "UTL_DBG_MOD_SER",
    (uint8_t*) "UTL_DBG_MOD_ADC",
    // nova string
};
```

Todos nós sabemos onde isso acaba: em erro ! Manter código que demanda alterações em várias partes é geralmente um pesadelo e fica muito fácil cometer erros. Uma forma de contornar esse problema é o uso de [X macros](https://en.wikipedia.org/wiki/X_macro). Com a X macro fica possível realizar alterações em apenas um lugar, mantendo a integridade do código. 

Para entender como isso funciona é preciso ficar claro como o pré-processador da linguagem C trabalha. Quando você cria definições personalizadas, o compilador não realiza nenhum operação sobre elas, não faz nenhuma expansão recursivas ou algo similar. Qualquer avaliação só será feita no momento real do uso da definição.

No código, existe uma definição de uma macro chamada `XMACRO_DBG_MODULES`. Essa macro é associada a uma outra macro, chamada de `X(mod,index)`, com dois parâmetros: o nome do módulo e a index (posição) do bit relacionado. Note que a macro não foi definida ainda e tudo bem, afinal `XMACRO_DBG_MODULES` também não foi usada, logo não existe a necessidade de expansão ou avaliação das macros envolvidas.

Assim, o que se tem até agora é que `XMACRO_DBG_MODULES` será trocada, futuramente, pelas avaliações da macro X. Eu gosto de ver essa parte como uma base de dados, onde as linhas são os registros dados por macros X e os campos são os argumentos separados por vírgula na macro X. Lembre-se que a barra invertida "\" serve apenas para que o pré-processador entender que, apesar de se ter continuado a macro na linha de baixo, tudo está numa linha só. No fundo, seria equivalente a escrever como a seguir:

```C
#define XMACRO_DBG_MODULES  X(UTL_DBG_MOD_APP, 0)  X(UTL_DBG_MOD_SER, 1)  X(UTL_DBG_MOD_ADC, 2)
```

Obviamente, a barra invertida traz mais legibilidade e facilidade de adição.

No momento em que o desenvolvedor pretende avaliar essa base de dados, ele lança mão da definição da macro X e uso da macro `XMACRO_DBG_MODULES`, forçando o pré-processador a avaliar tudo que estava pendente. Para entender isso, é melhor uma análise por partes. Primeiro, veja a declaração da macro X:

```C
#define X(MOD, INDEX) MOD = INDEX,
```

Aqui, ela está apenas dizendo ao pré-processador que, onde quer que `X(mod,index)` seja chamada, que é para colocar no lugar `MOD = INDEX,`. Ou seja, se você usar `X(TESTE,10)` no seu código, isso irá gerar `TESTE = 10,`. Só que isso fica mais interessante ao pedir a avaliação de toda a base de dados criada através da macro `XMACRO_DBG_MODULES`. É uma definição seguida imediatamente de uma avaliação quando temos a seguinte construção:

```C
#define X(MOD, INDEX) MOD = INDEX,
    XMACRO_DBG_MODULES
```

Note que é necessário mudar de linha ou a definição da macro X não teria terminado como esperado. O trecho acima irá gerar o seguinte bloco:

```C copy
UTL_DBG_MOD_APP = 0,
UTL_DBG_MOD_SER = 1,
UTL_DBG_MOD_ADC = 2,
```

No entanto, esse bloco está ao redor de uma declaração de enumeração. Ou seja, o trecho abaixo:

```C copy
typedef enum utl_dbg_modules_e
{
#define X(MOD, INDEX) MOD = INDEX,
    XMACRO_DBG_MODULES
#undef X
} utl_dbg_modules_t;
```

irá gerar o seguinte código quando o pré-processador avaliar tudo:

```C copy
typedef enum utl_dbg_modules_e
{
UTL_DBG_MOD_APP = 0,
UTL_DBG_MOD_SER = 1,
UTL_DBG_MOD_ADC = 2,
} utl_dbg_modules_t;
```

Mágico, não ? Uma ação importante é remover a definição da macro X logo após o uso de forma a permitir que outras interações com a base de dados seja feita. A chamada `#undef X` realiza essa função.

Na hora de criar o array de nomes de módulos de forma automática, a macro pode ser novamente reavaliada, no trecho a seguir:

```C copy
const uint8_t* utl_log_mod_name[] = {
#define X(MOD, INDEX) (uint8_t*) #MOD,
    XMACRO_DBG_MODULES
#undef X
};
```

Para entender o que vai acontecer é preciso explicar mais um aspecto do pré-processador conhecido como _stringification_, no caso realizado pelo operador "#" que aparece antes de `MOD`. A _stringification_ faz o que o nome diz, isto é, gera uma string a partir de um token. Ou seja, MOD não será usado literalmente mas sim substituído pela token equivalente e como uma string. Se `MOD` vale `UTL_DBG_MOD_APP`, `#MOD` gera `UTL_DBG_MOD_APP`. 

Com isso fica mais fácil entender que o pré-processador irá gerar o seguinte código:

```C
const uint8_t* utl_log_mod_name[] = {
(uint8_t*) "UTL_DBG_MOD_APP",
(uint8_t*) "UTL_DBG_MOD_SER",
(uint8_t*) "UTL_DBG_MOD_ADC",
};
```

Note que o campo `INDEX` não foi usado dentro da definição da macro X por não ter utilidade nesse caso e que depois foi feita também uma remoção da macro X via undef. 

Para finalizar, duas observações adicionais sobre o pré-processador. A primeira é sobre a concatenação automática de strings. Por exemplo, se você faz algo como

```C
#define TXT  "ABC"  "123"
```

o pré-processador vai juntar as strings acima e TXT irá valer "ABC123". Além disso, `__FILE__` e `__LINE__` são automaticamente substituídas pelo nome completo do arquivo e da linha em pré-processamento naquele momento. Esse é um recurso do ANSI-C e aqui fica clara a razão de se ter usado uma macro e não uma função pois se uma função fosse usada a linha e o arquivo seriam outros. 

A macro `UTL_LOG_HEADER` faz a composição da informação de localização do log, agregando arquivo e linha à string de formatação do usuário. Para entender, imagine o seguinte uso da macro `UTL_DBG_PRINTF()`:

```C
UTL_DBG_PRINTF(UTL_DBG_MOD_APP,"%d -> %d",10,20)
```

Isso irá gerar a seguinte saída (imaginando que `UTL_DBG_PRINTF()` foi usada na linha 123 do arquivo `source/port_uart.c` e que o módulo está habilitado):

```C
[UTL_DBG_MOD_APP][source/port_uart.c:123] 10 -> 20
```

Novamente, existem alguns detalhes nesse processo, é melhor ir por partes. Veja a definição da macro `UTL_LOG_HEADER()`:

```C
#define UTL_LOG_HEADER(mod, fmt, file, line) "[%s][%s:%d] " fmt, (char*) utl_dbg_mod_name_get(mod), file, line
```

Sabendo que `utl_dbg_mod_name_get(mod)` devolve uma string com o nome do módulo, uma chamada dessa macro com os parâmetros usados anteriormente faria o pré-processador trabalhar e gerar o seguinte resultado:

```C
UTL_LOG_HEADER(UTL_DBG_MOD_APP, "%d -> %d", __FILE__,__LINE__) => "[%s][%s:%d] %d -> %d", (char*) utl_dbg_mod_name_get(UTL_DBG_MOD_APP), "source/port_uart.c", 123
```

Perceba que foram adicionadas as informações de formatação do nome do módulo, arquivo e linha ao conjunto de especificações de impressão do usuário, gerando uma string única. Além disso, os valores de arquivo e linha corrente foram substituídos (lembre-se que está tudo na mesma linha, no fundo, devido à barra invertida). No entanto, dois elementos ainda estão obscuros: o operador **##** presente dentro da chamada do printf() e a macro `__VA_ARGS__`. 

Apesar de não recomendado para uma aplicação final, como já dito anteriormente, em ANSI-C é possível criar uma macro com parâmetros variáveis através de uma extensão conhecida como _variadic macro_, parecido (mas não igual) ao que se tem nativo da linguagem C para funções [funções com argumentos variáveis](https://www.gnu.org/software/c-intro-and-ref/manual/html_node/Variable-Number-of-Arguments.html) (_variadic functions_). 

No caso, a macro `__VA_ARGS__` representa a lista de parâmetros passada. Já o operador **##** apenas juntas as duas coisas em uma só. Ou seja, teremos o seguinte na composição final da macro de debug:

```C
printf("[%s][%s:%d] %d -> %d", (char*) utl_dbg_mod_name_get(UTL_DBG_MOD_APP), "source/port_uart.c", 123, 10, 20);
```

É isso. O "do { } while(0)" é somente uma forma de fazer um bloco em C que executa apenas uma vez, poderia ter sido feito de outra forma. O código apenas fica mais resiliente a possíveis erros de expansão (veja uma discussão no [stack overflow](https://pt.stackoverflow.com/questions/80669/por-que-usar-do-while-0) sobre isso). 

Para finalizar, como o nome do arquivo pode ser relativamente longo, a função `utl_dbg_base_name_get()` é usada para extrair apenas o nome do arquivo, sem o caminho completo. 

A implementação do módulo `utl_dbg.c` é relativamente simples, dada a seguir. Perceba que a variável `utl_dbg_mods_activated` controla os módulos ativos no momento, como um campo de bits, como já mencionado.

https://github.com/marcelobarrosufu/fwdev/blob/243c10ce686eb3b34d220e4a2df49a3d603d68e2/source/utl/utl_dbg.c#L1-L72

## System Calls

Toda essa discussão nos leva a um novo ponto: onde afinal termina o `printf()` ? No PC, isso vai virar uma impressão no console, ou no arquivo representado por `stdout`. Mas, e num sistema embarcados ? Nele não temos os tradicionais arquivos de entrada e saída, `stdin` e `stdout`, qual será o resultado dessa operação ? 

Para entender isso é preciso discutir um pouco sobre system calls e como realizar os redirecionamento para que consigamos ter as impressões desejadas.







<!-- >=> exe: callback para interrupção, abstrair outro modulo, -->