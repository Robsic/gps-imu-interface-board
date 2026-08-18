# Validação do projeto

## Resumo

| Área | Estado |
|---|---|
| GPS/NMEA e UART no Proteus | Validado funcionalmente |
| Pacote ACEINNA `e2` e CRC | Validado funcionalmente |
| Campos sintéticos de IMU | Transporte validado |
| Lógica simulada dos encoders | Validada |
| Conversão decimal da posição no VSM | Não concluída |
| CAN | Não validado |
| Alimentação e integridade elétrica | Não validadas |
| Sensores e encoders reais | Não validados |
| DRC para fabricação | Reprovado/pedente de correções |

O procedimento e as evidências do Proteus estão em [`simulation/proteus`](../simulation/proteus/README.md).

## DRC fornecido

O relatório `DRC.txt`, de 30/07/2026, registra:

| Categoria | Quantidade |
|---|---:|
| Pontes de máscara de solda | 12 |
| Violações de clearance | 5 |
| Texto espelhado | 1 |
| Serigrafia sobre cobre | 2 |
| Total | 20 |

O relatório também registra zero pads desconectados, mas isso não elimina as violações restantes.

## Antes da fabricação

1. Corrigir as violações de máscara e clearance.
2. Corrigir texto e serigrafia.
3. Conferir J2/J3 e todos os footprints customizados.
4. Corrigir os caminhos não portáveis das bibliotecas.
5. Executar ERC e DRC novamente.
6. Gerar Gerbers somente após revisão com zero erro crítico.

## Depois da placa montada

### Alimentação

1. Ensaiar inicialmente sem Blue Pill, GPS, IMU e encoders.
2. Verificar resistência entre as linhas de alimentação e GND.
3. Alimentar com fonte limitada em corrente.
4. Medir 5 V e 3,3 V, ripple, corrente e temperatura.
5. Instalar os módulos progressivamente.

### Comunicação e sensores

- validar UART com os módulos físicos;
- confirmar a conversão NMEA para latitude/longitude;
- testar fix, perda e recuperação do GPS;
- medir bias, ruído, deriva, orientação e calibração da IMU;
- testar A/B/Z dos dois encoders em ambos os sentidos;
- testar CAN, bitrate, IDs, terminação, erros e bus-off;
- executar teste prolongado e falhas por desconexão.

## Critério de comunicação para a reunião

A conclusão correta é: a lógica digital e o protocolo foram validados em simulação, enquanto a liberação elétrica, mecânica e de fabricação depende da placa física e da correção do DRC.
