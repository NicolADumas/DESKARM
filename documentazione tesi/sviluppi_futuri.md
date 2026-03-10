# Sviluppi Futuri e Possibili Miglioramenti

Questo documento delinea la roadmap per l'evoluzione futura del progetto **Dumas Arm (DUMARM)**, identificando le aree critiche di miglioramento sia lato Software/Interfaccia che lato Firmware/Hardware.

---

## 1. Interfaccia Utente e Backend (Software PC)

### A. Supporto Completo CAD e G-Code
Attualmente, l'architettura supporta solo l'importazione di immagini raster (JPG/PNG) e vettoriali semplici (SVG).
*   **Importazione DXF/DWG**: L'interfaccia utente predispone già un pannello (`cad-import-panel`), ma manca il backend Python (`py_import_dxf`) per il parsing delle entità CAD. Lo sviluppo futuro dovrebbe integrare librerie come `ezdxf` per convertire primitive CAD (Linee, Archi, Polilinee, Spline) in traiettorie robot.
*   **G-Code Interpreter**: Implementare un parser G-Code standard (G0, G1, G2, G3) permetterebbe al robot di interfacciarsi con software CAM professionali (es. Inkscape Gcodetools, Fusion 360), rendendolo una macchina CNC a tutti gli effetti.

### B. Ottimizzazione Traiettorie e Post-Processing
*   **Algoritmo di Semplificazione**: La funzione `py_optimize_trajectory` è attualmente un placeholder. L'implementazione dell'algoritmo di **Ramer-Douglas-Peucker** permetterebbe di ridurre drasticamente il numero di punti inviati per linee rette o curve lievi, riducendo il jitter e il carico sulla comunicazione seriale.
*   **Path Sorting Avanzato**: Migliorare l'algoritmo *Nearest Neighbor* attuale con euristiche più avanzate (es. *Ant Colony Optimization* o *Lin-Kernighan*) per minimizzare ulteriormente i movimenti a vuoto (Pen Up) nei disegni complessi.

### C. Computer Vision Avanzata
*   **AI-Based Vectorization**: Sostituire o affiancare l'attuale pipeline OpenCV (Canny/Thresholding + Skeletonize) con modelli di *Deep Learning* (es. U-Net o Pix2Pix) per la conversione "Raster-to-Vector". Questo migliorerebbe significativamente la qualità dei ritratti e dei disegni artistici, gestendo meglio ombre e spessori variabili.
*   **Calibrazione Automatica**: Utilizzare una webcam per rilevare automaticamente l'area di lavoro e la distorsione prospettica, permettendo di disegnare su fogli posizionati liberamente senza allineamento manuale.

### D. Usabilità e Cross-Platform
*   **Web-App Remota**: Disaccoppiare il server Python dalla GUI locale, permettendo di controllare il robot da tablet o smartphone via browser all'interno della stessa rete WiFi.
*   **Simulatore 3D Completo**: Attualmente la visualizzazione è planare (2D). L'integrazione di una vista 3D (usando librerie come `three.js` nel frontend) aiuterebbe a visualizzare i movimenti dell'asse Z e le rotazioni nello spazio in modo più intuitivo.

---

## 2. Firmware e Controllo Embedded

### A. Generazione Traiettorie On-Board (Motion Offloading)
Attualmente, il PC calcola la traiettoria punto per punto a 20-50Hz e la invia in streaming.
*   **Interpolatore Locale**: Spostare la generazione dei profili di moto (S-Curve, Quintic) direttamente sul microcontrollore (STM32). Il PC invierebbe solo i "Waypoint" (target X, Y, V_max), e il firmware gestirebbe l'interpolazione a frequenza molto più alta (es. 1kHz). Questo eliminerebbe i problemi di latenza della seriale e garantirebbe movimenti fluidi anche se il PC rallenta.
*   **Buffer Circolare Ampliato**: Aumentare la dimensione del buffer di ricezione e implementare un protocollo di *Flow Control* hardware (RTS/CTS) per prevenire svuotamenti del buffer durante movimenti complessi.

### B. Controllo Dinamico Avanzato
*   **Feedforward Dinamico Completo**: Reintrodurre il modello dinamico (Matrici M, C, G) in una forma semplificata o pre-calcolata (Lookup Tables) per migliorare la precisione ad alte velocità, compensando le forze inerziali e centrifughe che il semplice PID non può prevedere.
*   **Controllo di Impedenza/Forza**: Se si aggiungessero sensori di corrente, si potrebbe implementare un controllo di forza per permettere al robot di "sentire" la superficie, migliorando la qualità del tratto su superfici irregolari.

### C. Connettività Wireless
*   **Modulo ESP32/WiFi**: Sostituire la connessione USB/Seriale con un bridge WiFi trasparente o nativo. Questo eliminerebbe l'ingombro dei cavi, rendendo il manipolatore un dispositivo IoT portatile.

---

## 3. Evoluzione Hardware (Meccanica ed Elettronica)

### A. Asse Z Proporzionale
*   Attualmente l'asse Z è binario (Su/Giù) tramite servo. Sostituirlo con un attuatore lineare (motore stepper + vite) permetterebbe di controllare la **pressione** della penna o di gestire pennelli che richiedono altezze variabili per modulare lo spessore del tratto.

### B. End-Effector Intercambiabili
*   Progettare un attacco rapido per cambiare strumento: da penna a laser (per incisione leggera), a ventosa (per Pick & Place), a gripper meccanico.

### C. Controllo a Doppio Anello (Dual Loop)
*   **Sfruttamento Encoder Assoluti**: Dato che il robot è già equipaggiato con encoder assoluti sui bracci (oltre a quelli sui motori), il prossimo passo è implementare un'architettura di controllo **Dual Loop**.
    *   **Loop Interno (Velocità/Coppia)**: Chiuso sull'encoder motore per stabilità e reattività.
    *   **Loop Esterno (Posizione)**: Chiuso sull'encoder assoluto del braccio.
*   **Vantaggi**: Questo compenserebbe in tempo reale l'elasticità delle cinghie e il backlash (gioco meccanico) del riduttore, garantendo una precisione "alla punta" nettamente superiore rispetto al solo controllo cinematico attuale.
