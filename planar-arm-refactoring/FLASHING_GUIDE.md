# Guida al Flashing su STM32 con STM32CubeIDE

Questa breve guida spiega come compilare e caricare (flashare) il codice sulla scheda STM32 utilizzando l'ambiente di sviluppo STM32CubeIDE.

## 1. Prerequisiti

*   **Hardware**: Collega la scheda STM32 (es. Nucleo o Discovery) al computer tramite il cavo USB collegato alla porta dell'ST-LINK.
*   **Software**: Assicurati che STM32CubeIDE sia aperto e che il progetto sia caricato correttamente nel "Project Explorer".

## 2. Compilazione del Progetto

Prima di caricare il codice, è necessario compilarlo per generare il file binario.

1.  Seleziona il progetto nel **Project Explorer**.
2.  Clicca sull'icona a forma di **martello** (🔨) nella barra degli strumenti in alto (o premi `Ctrl + B`).
3.  Verifica nella **Console** (in basso) che la compilazione termini senza errori (dovresti vedere "Build Finished. 0 errors, ...").

## 3. Flashing (Caricamento del Codice)

Per caricare il codice sulla scheda:

1.  Assicurati che la scheda sia collegata.
2.  Clicca sull'icona **Run** (il triangolo verde ▶️) nella barra degli strumenti.
    *   *Nota*: La prima volta potrebbe aprirsi una finestra "Edit Configuration". Solitamente le impostazioni di default vanno bene, clicca su **OK** o **Run**.
3.  Alternativamente, puoi usare l'icona **Debug** (l'insetto 🪲) se vuoi eseguire il codice passo-passo per il debug.

## 4. Verifica

Durante il processo di flashing, osserva la **Console**. Dovresti vedere messaggi simili a:

```text
ST-LINK SN=...
...
Download verified successfully 
```

Se vedi questo messaggio, il codice è stato caricato correttamente e la scheda si riavvierà eseguendo il nuovo firmware.
