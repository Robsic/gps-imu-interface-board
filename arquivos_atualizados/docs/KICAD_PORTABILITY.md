# Portabilidade do projeto KiCad — Placa IMU + GPS com Blue Pill (STM32)

Projeto KiCad (`IMU_board.kicad_sch` / `IMU_board.kicad_pcb`) para placa de IMU/GPS baseada em **OpenIMU300ZI** + **STM32 Blue Pill** + regulador **LM1117-3.3**.
Este README torna o projeto **portável para `git clone`** (sem dependências globais) e documenta as bibliotecas locais.

> Projeto pai: [`../README.md`](../README.md) · Datasheets: [`../Imu datasheet connector/`](../Imu%20datasheet%20connector/) · Biblioteca STM32: [`../Kicad-STM32-master/`](../Kicad-STM32-master/)

---

## 1. Estrutura e bibliotecas locais

Todas as libs não-padrão já estão **dentro do diretório do projeto** e são referenciadas via `${KIPRJMOD}` (pasta do `IMU_board.kicad_pro`). Após `git clone` o KiCad carrega automaticamente.

```text
gps-imu-interface-board/
├── sym-lib-table          # 4 libs Legacy via ${KIPRJMOD}
├── fp-lib-table           # 4 libs KiCad via ${KIPRJMOD}
├── Kicad-STM32-master/    # YAAJ BluePill/BlackPill — symbols + footprints + 3D
│   ├── Symbols/YAAJ_BluePill_Part_Like_SWD_Breakout.lib
│   ├── Footprints/
│   ├── LM1117IMPX-3.3_NOPB(1)/   # footprint do LM1117-3.3 (nome com parênteses)
│   └── Packages3d/
├── imu_connector_female_2x10.pretty/   # conector IMU 2×10 fêmea
├── conn_gps_female_2x20.pretty/        # conector GPS 2×20 fêmea
├── RBS_stm32f103.pretty/                # footprint RBS STM32F103 (alternativo)
├── conn_02x10_top_bottom_imu.lib       # símbolo conector IMU 2×10
├── imu_conector.lib / imu_conector.dcm # símbolo conector IMU (legado)
├── RBS_stm32f103.lib / .dcm             # símbolo STM32F103RET6 (legado)
├── rbs02_components.lib                 # BD-STM32-AD etc.
└── IMU_board-cache.lib                  # cache legado (mantido p/ compatibilidade)
```

### sym-lib-table (corrigido para relativo)

```lisp
(lib (name "imu_conn") (type "Legacy") (uri "${KIPRJMOD}/conn_02x10_top_bottom_imu.lib") ...)
(lib (name "Blue_pill") (type "Legacy") (uri "${KIPRJMOD}/Kicad-STM32-master/Symbols/YAAJ_BluePill_Part_Like_SWD_Breakout.lib") ...)
(lib (name "YAAJ_BluePill_Part_Like_SWD_Breakout") (type "Legacy") (uri "${KIPRJMOD}/Kicad-STM32-master/Symbols/YAAJ_BluePill_Part_Like_SWD_Breakout.lib") ...)
(lib (name "IMU_board-cache") (type "Legacy") (uri "${KIPRJMOD}/IMU_board-cache.lib") ...)
```

Antes havia `IMU_GPS_Blue_Pill/` duplicado em `imu_conn` e caminho absoluto `/home/ryan/.../IMU_board-cache.lib` — agora relativo.

### fp-lib-table

```lisp
(lib (name "Blue_pill") (type "KiCad") (uri "${KIPRJMOD}/Kicad-STM32-master/Footprints") ...)
(lib (name "LM117IMPX3.3") (type "KiCad") (uri "${KIPRJMOD}/Kicad-STM32-master/LM1117IMPX-3.3_NOPB(1)") ...)
(lib (name "imu_connector_female_2x10") (type "KiCad") (uri "${KIPRJMOD}/imu_connector_female_2x10.pretty") ...)
(lib (name "conn_gps_female_2x20") (type "KiCad") (uri "${KIPRJMOD}/conn_gps_female_2x20.pretty") ...)
```

> **Aviso nome com `()`**: `LM1117IMPX-3.3_NOPB(1)` contém parênteses. Funciona, mas pode quebrar scripts `git` em Windows. Se der erro, renomeie a pasta para `LM1117IMPX-3.3_NOPB_1` e atualize `fp-lib-table:4`.

---

## 2. Como usar após `git clone` — zero configuração

**Requisitos:** KiCad 7+ (testado 10.0.4).

```bash
git clone https://github.com/Robsic/gps-imu-interface-board.git
cd gps-imu-interface-board
kicad IMU_board.kicad_pro   # ou File → Open Project
```

O KiCad carrega `sym-lib-table` / `fp-lib-table` locais automaticamente (`Preferences → Manage Symbol/Footprint Libraries → Project Specific`).

- Não mexa em `Kicad-STM32-master/` — já é a lib YAAJ oficial (GitHub `yet-another-average-joe/Kicad-STM32`).
- `IMU_board.kicad_pcb` e conectores `2×10` / `2×20` resolvem sem instalação global.

**PCB não foi alterado eletricamente** nesta migração (só `sym-lib-table:3,6`). Resíduos `D:/Users/admin/...` em `Kicad-STM32-master/Footprints/*.kicad_mod:114` e `IMU_board.kicad_pcb:6957` são legados do YAAJ e não afetam fabricação.

---

## 3. Adicionar nova biblioteca (ex.: Molex / Bourns para bornes)

Para manter o padrão de `treadle` e `Steer/Estercamento`:

```bash
cp -a /home/ryan/Documentos/Robsic\ Carrinho/pedais/treadle/libs/Connector_Molex ./libs/Connector_Molex
# em fp-lib-table adicione:
# (lib (name "Connector_Molex") (type "KiCad") (uri "${KIPRJMOD}/libs/Connector_Molex/Connector_Molex.pretty") ...)
git add libs/Connector_Molex fp-lib-table
```

Mapeamento Molex usado nos outros projetos (se precisar aqui):
- 2 pinos → `Connector_Molex:Molex_Micro-Fit_3.0_Molex_Micro-Fit_3.0_Header_Vertical_1x02_P3.00mm_Snap-in_Plastic_Peg_TH_436500215` (fallback genérico `...1x02...`)
- 3 pinos → `...1x03...436500315`
- 4 pinos → `...1x04...436500415`

---

## 4. Primeiro `git push` (se ainda não é repo)

```bash
cd IMU_GPS_Blue_Pill
git init
git add sym-lib-table fp-lib-table Kicad-STM32-master/ *.pretty/ *.lib *.dcm IMU_board.kicad_* README.md .gitignore
git status  # confirme: grep -r "/home/ryan" sym-lib-table_fp-lib-table deve ser vazio
git commit -m "fix: torna sym-lib-table portável via \${KIPRJMOD} e documenta libs locais"
git branch -M main && git remote add origin <URL> && git push -u origin main
```

`.gitignore` já ignora `*.lck`, `*-backups/`, `fp-info-cache`.

---

## 5. Checklist de portabilidade

- [ ] `grep -r "/home/ryan" sym-lib-table fp-lib-table` vazio
- [ ] `ls Kicad-STM32-master/Symbols/YAAJ_BluePill_Part_Like_SWD_Breakout.lib` existe
- [ ] Abrir `IMU_board.kicad_sch` → ERC 0 erros de lib
- [ ] Abrir `IMU_board.kicad_pcb` → DRC sem "missing footprint"
