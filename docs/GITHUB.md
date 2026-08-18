# Fluxo de contribuição

## Clonar

```bash
git clone https://github.com/Robsic/gps-imu-interface-board.git
cd gps-imu-interface-board
```

## Atualizar uma branch

```bash
git switch main
git pull --ff-only origin main
git switch -c nome-da-alteracao
```

Faça as alterações, confira `git status` e execute os testes aplicáveis.

```bash
git add .
git commit -m "Descrição objetiva da alteração"
git push -u origin nome-da-alteracao
```

Depois, abra o repositório no GitHub, selecione **Compare & pull request**, revise os arquivos e crie o pull request para `main`.

Não envie senhas, chaves, arquivos temporários ou backups automáticos.
