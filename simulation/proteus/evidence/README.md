# Evidências

## proteus-validation-v2.6.png

A imagem registra simultaneamente:

- terminal principal em 115200 baud;
- pacotes `E2=OK` com `CRC=OK`;
- contador `PKT` crescente;
- `CRC_ERR=0` e `RX_OVF=0`;
- `EMU=2.6`, `HEM=NW`, `MODE=INS` e `GPS=FIX`;
- terminal GPS em 9600 baud com mensagens NMEA.

Os valores decimais de latitude/longitude exibidos pelo terminal principal não são considerados aprovados, conforme a limitação documentada no README da simulação.
