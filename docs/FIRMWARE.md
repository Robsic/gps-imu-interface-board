# Firmware

## Estado atual

O repositório agora contém firmware de **simulação funcional** em [`simulation/proteus/firmware`](../simulation/proteus/firmware/README.md). Ele valida comunicação, protocolo e lógica de teste, mas não deve ser confundido com o firmware de produção do veículo.

Ainda é necessário criar e versionar o firmware de produção, contendo:

- projeto STM32CubeIDE, CMake ou equivalente;
- configuração de clock e periféricos;
- driver real do OpenIMU300ZI;
- recepção e conversão GPS no STM32 físico;
- timers e interrupções para os encoders;
- CAN com identificadores, escalas, heartbeat e estados de falha;
- watchdog, brownout, timeouts e inicialização segura;
- procedimento de build, gravação e depuração;
- testes unitários e de integração.

## Mapa funcional

| Função | Pino STM32 | Direção |
|---|---|---|
| Encoder esquerdo A | `PA0` | entrada |
| Encoder esquerdo B | `PA1` | entrada |
| IMU UART TX | `PA2` | saída |
| IMU UART RX | `PA3` | entrada |
| Encoder direito A | `PB6` | entrada |
| Encoder direito B | `PB7` | entrada |
| Encoder esquerdo Z | `PB8` | entrada |
| Encoder direito Z | `PB9` | entrada |
| CAN RX | `PA11` | entrada |
| CAN TX | `PA12` | saída |

O arquivo legado `IMU_board_.bin` não possui origem e versão suficientemente documentadas; não deve ser usado como firmware de produção sem rastreabilidade.
