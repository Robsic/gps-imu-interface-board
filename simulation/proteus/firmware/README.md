# Firmware exclusivo para simulação

Estes programas foram escritos para validar a comunicação no Proteus e não são o firmware final do veículo.

## Alvos

- `u1_main`: recebe o pacote ACEINNA `e2`, valida CRC, extrai os campos, exercita as entradas de encoder e imprime o diagnóstico.
- `u2_openimu_emulator`: recebe NMEA do `VGPS`, produz dados sintéticos de IMU e transmite o pacote `e2`.

## Compilação

Requisitos:

- GNU Make;
- toolchain `arm-none-eabi-gcc`;
- GCC nativo para os testes do computador.

O prefixo da toolchain pode ser informado por `TOOLCHAIN`:

```bash
make -C u1_main TOOLCHAIN=arm-none-eabi-
make -C u2_openimu_emulator TOOLCHAIN=arm-none-eabi-
gcc -std=c11 -O2 -Wall -Wextra -Werror tests/fixed_ieee_test.c -o /tmp/fixed_ieee_test
/tmp/fixed_ieee_test
```

Os HEX previamente gerados e utilizados na evidência estão em `hex/`.

## Protocolo

O pacote oficial `e2` utiliza:

- preâmbulo `0x55 0x55`;
- código `0x65 0x32`;
- payload de 123 bytes;
- valores multibyte do payload em little-endian;
- CRC16-CCITT, polinômio `0x1021`, valor inicial `0x1D0F` e sem XOR final.

A versão 2.6 também envia o pacote auxiliar `c6` para diagnóstico do emulador. Ele não altera o pacote oficial `e2`.
