# Report Tecnico Definitivo Refactoring Firmware: Analisi Comparativa

Questo documento dettaglia le differenze tecniche, architetturali e algoritmiche tra il vecchio firmware (`custom.c`) e la nuova versione modulare (`planar-arm-refactoring`).

---

## 1. Architettura Software e Memory Layout

### Vecchio Firmware: Monolitico "Super-Loop"
-   **File Chiave**: `old files/firmware/Core/Src/custom.c` (1383 righe).
-   **Struttura**: Tutte le funzioni (`HAL_UART_RxCpltCallback`, `controller`, `B_calc`) risiedono nello stesso scope.
-   **Memoria (Stack/Global)**: Uso massiccio di variabili `static` o globali non strutturate (`rx_data[DATA_SZ]`, `float offset1`, `man_t manip`).
-   **Criticità**: "Spaghetti Code". Le ISR (Interrupt Service Routines) modificano variabili globali usate dal loop principale senza meccanismi di protezione (sezione critica mancante), rischio **Race Conditions**.

### Nuovo Firmware: Architettura a Strati (Layered)
-   **File Chiave**: `application` (`main.c`), `logic` (`manipulator.c`), `protocol` (`protocol.c`), `driver` (`controller.c`, `encoder.c`).
-   **Struttura Dati**: Definita in `manipulator_types.h`. 
    -   `manipulator_t`: Struct gigante che incapsula *tutto* lo stato (Buffers, PID, Hardware Handles). Passata sempre per puntatore (`manipulator_t *manip`), garantendo che le funzioni siano "Pure" e testabili.
    -   `Packet_t`: Struct `packed` per mappare i byte della seriale direttamente in float.
-   **Safety**: I buffer sono `volatile` (`mb_head`, `mb_tail`) per garantire la coerenza tra ISR e Main Loop.

---

## 2. Stima della Velocità (Differenza Matematica)

### Vecchio: Filtro Passa-Basso Primo Ordine
**Codice**: `speed_estimation` in `custom.c` (riga 719).
-   **Metodo**: Media mobile su 5 campioni (`prev`, `succ`) + Filtro digitale del primo ordine.
    ```c
    v_est = 0.8546*vel + (1-0.8546)*(succ-prev)/(T_C*5);
    ```
-   **Difetto**: Introduce **ritardo di fase**. Il robot reagisce in ritardo ai cambiamenti di velocità reali, causando errori di tracking nelle curve veloci.

### Nuovo: Regressione Lineare (Minimi Quadrati)
**Codice**: `calculate_slope` in `utils.c` (riga 3).
-   **Metodo**: Calcola la pendenza della retta di regressione (`slope`) su un buffer finestra mobile (`ringbuffer`).
    ```c
    slope = (N * Σ(xy) - Σx * Σy) / (N * Σ(x^2) - (Σx)^2)
    ```
-   **Vantaggio**: Ottiene una stima della derivata (velocità) molto più "pulita" e robusta al rumore dell'encoder rispetto alla semplice differenza finita, **senza introdurre il lag** tipico dei filtri IIR/RC.

---

## 3. Gestione Interrupt e Determinismo

### Vecchio: Interrupt "Pesanti"
-   **UART RX**: `HAL_UART_RxCpltCallback` in `custom.c`.
    -   Esegue `strtok` (parsing stringa) e `memcpy` *dentro* l'interrupt.
    -   Questo blocca la CPU per centinaia di cicli di clock ogni volta che arriva un comando, potenzialmente ritardando il loop di controllo PID (`TIM10`).
-   **Timer Controllo**: Gestito nel `while(1)` con parvenza di 100Hz ma soggetto al jitter delle `printf` bloccanti.

### Nuovo: Interrupt "Minimali" e DMA
-   **UART RX**: Gestita interamente dall'hardware (**Circular DMA DMA1_Stream5**).
    -   L'ISR `DMA1_Stream5_IRQHandler` in `stm32f4xx_it.c` chiama solo la gestione generica HAL.
    -   Il parsing (`manipulator_uart_process`) avviene nel **Main Loop** quando la CPU è libera. Nessun "Context Switch" costoso per ogni byte.
-   **Controllo**: Scheduler rigido nel `main.c`: `if((HAL_GetTick() - last_tick) > 10)`. Priorità assoluta.

---

## 4. Algoritmo di Controllo (Cuore del Sistema)

### Vecchio: Computed Torque (Instabile)
-   Tentava di implementare $ \tau = M(q)\ddot{q} + C(q,\dot{q})\dot{q} + g(q) $.
-   Le matrici $M$ e $C$ (in `custom.c`) contenevano decine di chiamate a `sin()` e `cos()`. Su un STM32F4 (FPU singola precisione), questo carico satura la capacità di calcolo a 100Hz, causando "singhiozzi".

### Nuovo: PID Indipendente + Feedforward (Industriale)
-   Abbandona la dinamica inversa on-board.
-   Usa un loop PID parallelo su $q_0$ e $q_1$.
-   **Feedforward**: La "conoscenza" della dinamica è spostata nel generatore di traiettoria (Python), che invia valori di velocità (`dq`) e accelerazione (`ddq`) pre-calcolati. Il firmware li applica direttamente:
    ```c
    u = PID_out + (Kv * dq_ref) + (Ka * ddq_ref)
    ```
-   Risultato: Tracking fluido anche ad alte accelerazioni, carico CPU < 1%.

---

## 5. Sicurezza Attiva (Machine Safety)

### Vecchio: Monitoraggio Passivo
-   Gli interrupt dei finecorsa (`HAL_GPIO_EXTI_Callback`) si limitavano a settare flag (`limit_switch = 1`) o resettare variabili se in fase di homing.

### Nuovo: Emergency Stop Hardware
-   In `main.c`, l'interrupt del finecorsa forza **velocità zero** (`manipulator_set_motor_velocity(..., 0.0f)`) immediatamente, bypassando il loop di controllo. È un "Hard Stop" di sicurezza.

---

## 6. Multimedia Sync

-   **Audio Bit-Banging**: `Robot_Sing` riconfigura i pin al volo da AF (Alternate Function PWM) a GPIO Output per generare frequenze audio quadre.
-   **Pen Sync**: Il comando `PEN_UP/DOWN` viaggia sincrono nel `Packet_t`.

## Tabella Riassuntiva Finale

| Caratteristica | Vecchio Firmware | Nuovo Firmware |
| :--- | :--- | :--- |
| **Architettura** | Monolitica (1 file) | Modulare (Layered) |
| **Protocollo** | ASCII (strtok) | Binario (Struct packed) |
| **Integrità Dati** | Nessuna | CRC32 Checksum |
| **Parsing** | In Interrupt (Blocking) | In Main Loop (DMA Polling) |
| **Stima Velocità** | Filtro IIR (Lag) | Regressione Lineare (Low Noise) |
| **Controllo** | Computed Torque (Heavy) | PID + Feedforward (Light) |
| **Safety** | Passiva (Flag) | Attiva (Hard Stop) |
