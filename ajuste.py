import pcbnew

board = pcbnew.GetBoard()

largura_sinal_mm = 0.3
largura_alimentacao_mm = 0.6

largura_sinal = pcbnew.FromMM(largura_sinal_mm)
largura_alimentacao = pcbnew.FromMM(largura_alimentacao_mm)

count_sinal = 0
count_alim = 0
count_gnd_ignorado = 0

for trilha in board.GetTracks():
    if isinstance(trilha, pcbnew.PCB_VIA):
        continue
        
    nome_net = trilha.GetNetname()
    if not nome_net:
        continue
        
    nome_curto = nome_net.split('/')[-1].strip()
    
    if nome_curto.upper().startswith('GND'):
        count_gnd_ignorado += 1
        continue
        
    elif nome_curto.startswith('+'):
        trilha.SetWidth(largura_alimentacao)
        count_alim += 1
        
    else:
        trilha.SetWidth(largura_sinal)
        count_sinal += 1

pcbnew.Refresh()

print("=" * 50)
print("CORRECAO APLICADA COM SUCESSO!")
print("-" * 50)
print("Trilhas de GND ignoradas (intactas): {}".format(count_gnd_ignorado))
print("Trilhas de ALIMENTACAO (+) -> {} mm: {}".format(largura_alimentacao_mm, count_alim))
print("Trilhas de SINAL -> {} mm: {}".format(largura_sinal_mm, count_sinal))
print("=" * 50)


