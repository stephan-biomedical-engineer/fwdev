# Aulas

- Ambiente de desenvolvimento e debug
- Criando abstrações de hardware
- Abstrações para cpu, uart, gpio, timer
- Estratégias de construção de firmware (loop, corotinas, scheduler, rtos)
- Entendendo a partida do firmware
- Entendendo o cortex M4
- Criando um escalonador de tarefas
- Criando um sistema de eventos via pensv
- RTOS e troca de contexto
- Criando um RTOS mínimo

# Exercícios

## Configurando o ambiente

1. Escolha um sistema operacional para o desenvolvimento. Linux e Windows são as opções mais comuns. Instale as ferramentas necessárias, como compiladores, editores de texto, cmake, git e make.
2. Instale o STM32CubeIDE e o STM32CubeProgrammer.
3. Faça um fork do repositório do curso [repositório do curso](https://github.com/marcelobarrosufu/fwdev/tree/main).
3. Compile e execute o exemplo do hal_dbg, para validar que tudo está operacional.
4. Crie um novo projeto no STM32CubeIDE, utilizando o STM32F411RE como microcontrolador. Configure o projeto para que ele consiga utilizar o hal_dbg. Num primeiro momento, redirecione a saída para o ITM.

