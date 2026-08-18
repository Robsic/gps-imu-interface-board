# Simulação funcional GPS + OpenIMU + Blue Pill

Esta pasta reúne o ensaio realizado no Proteus 9.1 para validar a cadeia digital do projeto:

```text
VGPS (NMEA) -> U2 (emulador OpenIMU300ZI) -> pacote ACEINNA e2 -> U1 -> diagnóstico
```

O ensaio não substitui a validação elétrica da placa. A segunda Blue Pill é apenas um emulador funcional do OpenIMU300ZI e os encoders são estimulados por loopback.

- data da evidência: 18/08/2026;
- ambiente usado: Proteus Professional 9.1, 64 bits;
- requisito local: componente VSM `BLUEPILL` devidamente instalado e componente nativo `VGPS`.

Bibliotecas ou instaladores de terceiros não são redistribuídos neste repositório.

## Conteúdo

| Caminho | Finalidade |
|---|---|
| `IMU_GPS_VALIDATION_BASE.pdsprj` | Projeto-base do Proteus com o componente Blue Pill |
| `firmware/hex/bluepill_main_aceinna_e2_v2_6.hex` | Firmware da U1 |
| `firmware/hex/openimu300zi_emulator_v2_6.hex` | Firmware da U2 |
| `firmware/u1_main/` | Código-fonte e Makefile da U1 |
| `firmware/u2_openimu_emulator/` | Código-fonte e Makefile da U2 |
| `firmware/tests/` | Teste das rotinas numéricas executado no computador |
| `evidence/proteus-validation-v2.6.png` | Evidência visual do ensaio |

> O arquivo `.pdsprj` disponível é o projeto-base. A montagem final foi completada manualmente no Proteus durante o ensaio; reproduza as ligações descritas abaixo.

## Componentes

1. Duas instâncias do componente `BLUEPILL`: `U1` e `U2`.
2. Um `VGPS`.
3. Dois `VIRTUAL TERMINAL`.

Nas duas Blue Pills:

- OSC Frequency: `8MHz`;
- Clock Scale: `Off`;
- Disassemble Binary Code: `No`.

Carregue os dois arquivos HEX da versão 2.6 juntos.

## Ligações

### GPS e comunicação

| Origem | Destino | Configuração |
|---|---|---|
| `VGPS TX` | Terminal GPS `RXD` | 9600, 8N1 |
| `VGPS TX` | `U2 PA10` | USART1 RX, 9600 |
| `U2 PA2` | `U1 PA3` | pacote ACEINNA, 115200 |
| `U1 PA2` | Terminal principal `RXD` | diagnóstico, 115200, 8N1 |

Deixe `VGPS RX`, os pinos `TXD` dos terminais e todos os `RTS/CTS` sem ligação.

### Loopback dos encoders

| Saída de estímulo | Entrada de contagem |
|---|---|
| `U1 PB10` | `U1 PA0` |
| `U1 PB11` | `U1 PA1` |
| `U1 PB12` | `U1 PB6` |
| `U1 PB13` | `U1 PB7` |
| `U1 PB14` | `U1 PB8` |
| `U1 PB15` | `U1 PB9` |

## Configuração do VGPS

A evidência arquivada utiliza:

- latitude: `54.071125`;
- longitude: `-1.995949` aproximadamente;
- altitude: `100 m`;
- 10 satélites;
- aquisição a quente: 5 s.

As mensagens observadas contêm aproximadamente:

```text
$GPGGA,...,5404.2675,N,00159.7569,W,1,10,4.00,100.0,M,...
$GPRMC,...,A,5404.2675,N,00159.7569,W,...
$GPGSA,A,3,...
```

Conversão esperada da posição:

- `5404.2675,N` = `54.071125 graus`;
- `00159.7569,W` = `-1.995948 graus`;
- altitude = `100.0 m`.

## Resultado observado

O terminal principal apresentou:

```text
E2=OK CRC=OK PKT=<crescente> POS=COMPAT32 EMU=2.6 HEM=NW MODE=INS GPS=FIX
CRC_ERR=0 RX_OVF=0
```

Também foram observados valores variáveis de `RPY`, `ACC`, `L`, `R`, `ZL` e `ZR`.

A mensagem `E2=WAIT` pode aparecer entre pacotes por causa do escalonamento do modelo VSM. Ela não caracteriza falha enquanto os pacotes voltarem a aparecer, `PKT` continuar aumentando e `CRC_ERR/RX_OVF` permanecerem em zero.

## Matriz de validação

| Função | Estado | Evidência/limite |
|---|---|---|
| Geração de NMEA pelo VGPS | Validada em simulação | GGA, RMC e GSA contínuas |
| Fix e hemisférios | Validada em simulação | `GPS=FIX`, `HEM=NW` |
| UART VGPS -> U2 | Validada em simulação | 9600 baud |
| UART U2 -> U1 | Validada em simulação | 115200 baud |
| Pacote ACEINNA `e2` | Validado em simulação | preâmbulo, código, tamanho e payload |
| CRC16-CCITT | Validado em simulação | `CRC=OK`, `CRC_ERR=0` |
| Buffer de recepção | Validado no cenário simulado | `RX_OVF=0` |
| Campos IMU | Validado como transporte | dados sintéticos, sem MEMS real |
| Encoders | Validado como lógica | sinais por loopback |
| Latitude/longitude decimal | Não concluída no VSM | o NMEA está correto, mas o modelo Blue Pill corrompe a aritmética em execução |
| CAN | Não validado | modelo apresenta `CAN=NA(PROTEUS)` |
| Alimentação e integridade elétrica | Não validadas | dependem da placa física |

## Limitações

- O modelo `BLUEPILL` usado é de terceiros.
- A conversão dinâmica da latitude/longitude decimal não é confiável nesse VSM. Não use `LAT=0.000000` ou `LON=2.662615` como coordenadas aprovadas.
- U2 não representa ruído, bias, deriva térmica, calibração, EKF ou atrasos internos do OpenIMU real.
- A simulação não avalia alimentação, transientes, níveis elétricos, EMI, conectores, antena, soldagem ou temperatura.
- O CAN e o transceiver SN65HVD230 devem ser testados em bancada.

## Testes de falha

### Perda do GPS

1. Pare a simulação.
2. Desconecte `VGPS TX -> U2 PA10`.
3. Inicie novamente.
4. Confirme a transição para `GPS=NOFIX`.

### Perda da OpenIMU

1. Pare a simulação.
2. Desconecte `U2 PA2 -> U1 PA3`.
3. Inicie novamente.
4. Confirme `E2=WAIT`.
5. Reconecte e confirme o retorno de `E2=OK`.

## Referências

- [ACEINNA GPS/INS App e pacote e2](https://openimu.readthedocs.io/en/latest/apps/ins.html)
- [ACEINNA OpenIMU UART Messaging](https://openimu.readthedocs.io/en/latest/software/UARTmessaging.html)
