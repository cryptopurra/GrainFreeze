# Come mettere GrainFreeze su GitHub (e compilarlo gratis)

GitHub Actions compila il plugin su Windows, macOS e Linux a ogni push, senza
consumare crediti Claude. Segui questi passi una volta sola.

## 1. Crea la repository
- Vai su https://github.com/new
- Nome: `GrainFreeze`, visibilità **Public**, **non** aggiungere README/License
  (ci sono già). Crea.

## 2. Carica i file
Due modi — scegli quello che preferisci.

### A) Da riga di comando (consigliato)
Estrai lo zip, apri un terminale nella cartella `GrainFreeze` e:

```bash
git init
git add .
git commit -m "Initial commit: GrainFreeze VST3 scaffold"
git branch -M main
git remote add origin https://github.com/<TUO_UTENTE>/GrainFreeze.git
git push -u origin main
```

### B) Dal sito (senza terminale)
Nella pagina della repo vuota → "uploading an existing file" → trascina **tutto
il contenuto** della cartella `GrainFreeze` (inclusa la cartella nascosta
`.github`). Commit.
> Nota: se trascini dal browser, assicurati che la cartella `.github/workflows/`
> finisca caricata — alcuni sistemi nascondono le cartelle che iniziano con un
> punto. Il metodo A non ha questo problema.

## 3. La build parte da sola
Appena pushi su `main`, apri la tab **Actions** della repo: vedrai il workflow
"Build GrainFreeze" girare su tre OS. Quando finisce (verde), clicca sul run e
scarica gli **Artifacts**:
- `GrainFreeze-Windows` → il `.vst3` da mettere in Cubase
- `GrainFreeze-macOS` → `.vst3` / `.component` / `.app`
- `GrainFreeze-Linux` → `.vst3` / standalone

## 4. Installa in Cubase (Windows)
Copia `GrainFreeze.vst3` in:
`C:\Program Files\Common Files\VST3\`
Riavvia Cubase o rifai la scansione plugin.

## Se la build fallisce
Normale al primo giro con JUCE. Nella tab Actions apri il run rosso, copia il
messaggio di errore del compilatore e incollamelo qui: lo sistemo e ripushi.
