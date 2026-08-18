# Hardware e interfaces

## Visão geral

A placa integra:

- entrada nominal de 12 V;
- conversores para 5 V e 3,3 V;
- Blue Pill/STM32F103;
- transceiver CAN SN65HVD230;
- duas interfaces diferenciais A/B/Z para encoders;
- conectores para GPS e OpenIMU300ZI;
- interfaces de programação e depuração.

## Conectores

| Conector | Função |
|---|---|
| `J1` | alimentação, GND, CANH e CANL |
| `J2` | encoder associado às redes `*_Esq` |
| `J3` | encoder associado às redes `*_Dir` |
| `J4` | GPS, alimentação e UART |
| `J5` | IMU, UARTs, SWD, reset e alimentação |
| `JTAG_IMU1` | SWD da IMU |

Os textos visuais `J_Dir` e `J_Esq` precisam ser conferidos: os nomes de rede indicam J2 como esquerdo e J3 como direito.

## Pinos principais do STM32

| Função | Pino |
|---|---|
| Encoder esquerdo A/B | `PA0/PA1` |
| Comunicação principal com IMU | `PA2/PA3` |
| Encoder direito A/B | `PB6/PB7` |
| Índices esquerdo/direito | `PB8/PB9` |
| CAN RX/TX | `PA11/PA12` |

## Estado de fabricação

O relatório DRC arquivado contém 20 ocorrências e a placa ainda não deve ser tratada como liberada para fabricação. Consulte [VALIDATION.md](VALIDATION.md).
