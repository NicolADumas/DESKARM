# Algoritmi Fondamentali del Frontend JavaScript — GUI DUMARM

> **Legenda:** Gli algoritmi marcati con `*` sono considerati **critici** per l'architettura di controllo robotico, in quanto impattano direttamente il comportamento fisico del manipolatore, costituiscono il ponte tra il dominio digitale e quello fisico, proteggono la correttezza dei dati, oppure sono matematicamente irreversibili (un errore causerebbe movimenti scorretti o blocco del sistema).

---

## Tabella Completa degli Algoritmi

| # | Algoritmo | File | Dominio |
|---|-----------|------|---------|
| 1 | *FLSC + Nearest Neighbor (Ottimizzazione Traiettorie) | `trajectory_optimizer.js` | Ricerca Operativa |
| 2 | *Douglas-Peucker (Semplificazione Vettoriale) | `utils_drawing.js` | Grafica Vettoriale |
| 3 | *Cinematica Diretta — DK(q) | `manipulator.js` | Robotica |
| 4 | *Conversione Sistemi di Riferimento (rel↔abs) | `utils.js` | Geometria Applicata |
| 5 | Curve Parametriche (Cuore, Spirale, Stella...) | `utils_drawing.js` | Matematica Pura |
| 6 | *Memento Pattern — Undo/Redo Stack | `state.js` | Design Pattern |
| 7 | *Cerchio da 3 Punti — find_circ | `utils.js` | Geometria Analitica |
| 8 | Matrice di Rotazione 2D | `utils_drawing.js` | Algebra Lineare |
| 9 | Luminanza Percettiva HSP | `theme.js` | Color Science |
| 10 | *Distanza Punto-Retta (Eraser Hit Detection) | `canvas.js` | Geometria Analitica |
| 11 | *XNOR Arco/Direzione (Antiorario vs Orario) | `trajectory.js` | Logica Booleana |
| 12 | Observer/Subscriber Pattern (Reattività UI) | `state.js` | Design Pattern |
| 13 | Game Loop via requestAnimationFrame | `canvas.js` | Rendering |
| 14 | Debounce Input (Preview Testo in Tempo Reale) | `main.js` | Ottimizzazione I/O |
| 15 | Proximity Search — Snap al Punto Vicino | `canvas.js` | Ricerca Spaziale |
| 16 | Deep Clone via Serializzazione JSON | `state.js` | Gestione Memoria |
| 17 | Quantizzazione — Snap to Grid | `utils_drawing.js` | Segnali Discreti |
| 18 | Rimappatura Cromatica Canali HEX | `theme.js` | Color Science |
| 19 | *Payload Assembly + Pen-Up Injection | `main.js` | Protocollo Dati |
| 20 | *Proportional Resize con Riscalatura Coordinate | `canvas.js` | Grafica Adattiva |

---

## Descrizioni Dettagliate

### 1. * FLSC + Nearest Neighbor
**File:** `trajectory_optimizer.js`  
**Dominio:** Ricerca Operativa  

Implementa una strategia di ottimizzazione delle traiettorie in due fasi:
1. **FLSC** (*First Linear, Second Curved*): separa tutti i tratti geometrici in linee dritte e curve, eseguendo prima tutte le lineari (più rapide e precise per il robot) e poi le curve.  
2. **Nearest Neighbor Greedy**: all'interno di ciascun gruppo, ordina i tratti minimizzando i movimenti a vuoto (penna alzata) selezionando sempre il tratto il cui punto di partenza è più vicino alla posizione corrente dell'effettore.

---

### 2. * Douglas-Peucker
**File:** `utils_drawing.js`  
**Dominio:** Grafica Vettoriale  

Algoritmo ricorsivo di riduzione dei vertici. Traccia un segmento immaginario tra l'inizio e la fine di una polilinea e calcola la distanza perpendicolare di ogni punto intermedio da tale segmento. Se la distanza massima è inferiore a una soglia di tolleranza, tutti i punti interni vengono eliminati. Altrimenti, il punto più distante diventa un nuovo punto di "divisione" e il processo si ripete su ciascun sottosegmento. Riduce drasticamente il numero di punti da trasmettere via seriale ad Arduino senza perdita visiva percepibile.

---

### 3. * Cinematica Diretta — DK(q)
**File:** `manipulator.js`  
**Dominio:** Robotica  

Implementa la cinematica diretta di un manipolatore planare a 2 link. Dati gli angoli θ₁ e θ₂ dei due giunti rotoidali (restituiti dal backend Python via seriale) e le lunghezze L₁, L₂ dei bracci, calcola le coordinate cartesiane assolute dell'end-effector:

```
p1 = [L1·cos(θ1), L1·sin(θ1)]
p2 = [p1.x + L2·cos(θ1+θ2), p1.y + L2·sin(θ1+θ2)]
```

Le coordinate vengono poi convertite in pixel relativi tramite `abs2rel` per il rendering grafico.

---

### 4. * Conversione Sistemi di Riferimento (rel↔abs)
**File:** `utils.js`  
**Dominio:** Geometria Applicata  

Due funzioni inverse che mantengono la coerenza tra il sistema di riferimento del canvas (pixel, origine in alto a sinistra) e il sistema di riferimento fisico del robot (metri, origine al centro del piano di lavoro, asse Y invertito):

- `rel2abs(x, y)`: `x_a = (x - origin.x) * m_p` , `y_a = -(y - origin.y) * m_p`  
- `abs2rel(x, y)`: `x_p = x / m_p + origin.x` , `y_p = -y / m_p + origin.y`  

Il fattore `m_p` (metri per pixel) è tipicamente `(0.272 × 2) / 700 ≈ 0.000777 m/px`.

---

### 5. Curve Parametriche
**File:** `utils_drawing.js`  
**Dominio:** Matematica Pura  

Generazione di forme complesse tramite equazioni parametriche in funzione di `t ∈ [0, 2π]`:
- **Cuore**: `x = 16·sin³(t)`, `y = -(13·cos(t) - 5·cos(2t) - 2·cos(3t) - cos(4t))`
- **Spirale di Archimede**: `r(t) = (t / t_max) · r_max`
- **Stella**: alternanza tra raggio esterno e interno tramite `i % 2 === 0`

---

### 6. * Memento Pattern — Undo/Redo Stack
**File:** `state.js`  
**Dominio:** Design Pattern  

Implementa il pattern Memento con due stack LIFO (`history` e `redoHistory`). Ad ogni conferma di forma, uno snapshot profondo (via JSON serialize/parse) dell'intera traiettoria viene spinto su `history`. `undo()` trasferisce la cima di `history` su `redoHistory` e ripristina il penultimo stato; `redo()` fa l'inverso. Il deep-clone via JSON garantisce l'indipendenza degli snapshot da modifiche future.

---

### 7. * Cerchio da 3 Punti — find_circ
**File:** `utils.js`  
**Dominio:** Geometria Analitica  

Data una sequenza di punti cliccati dall'utente, ricostruisce i parametri dell'arco sottostante:
- **Raggio**: `r = |k - a| / 2`
- **Centro**: `c = a + (k - a) · 0.5`
- **Punto finale**: `p = c + (b - c).set(r)` (proiettato sul cerchio)
- **Angoli**: `θ₀ = atan2(c-a) + π`, `θ₁ = atan2(c-p) + π`

---

### 8. Matrice di Rotazione 2D
**File:** `utils_drawing.js`  
**Dominio:** Algebra Lineare  

Applicata a ogni vertice delle forme geometriche per supportare la rotazione arbitraria:

```
x' = x·cos(θ) - y·sin(θ)
y' = x·sin(θ) + y·cos(θ)
```

---

### 9. Luminanza Percettiva HSP
**File:** `theme.js`  
**Dominio:** Color Science  

Stima della luminosità percepita dall'occhio umano tramite l'equazione HSP (*Highly Sensitive Poo*), che tiene conto della diversa sensibilità dei fotorecettori ai tre canali cromatici:

```
L = √(0.299·R² + 0.587·G² + 0.114·B²)
```

Usata per scegliere automaticamente il colore del testo (bianco o nero) garantendo contrasto leggibile su qualsiasi sfondo personalizzato.

---

### 10. * Distanza Punto-Retta (Eraser Hit Detection)
**File:** `canvas.js`  
**Dominio:** Geometria Analitica  

Calcola la distanza perpendicolare di un punto P dal segmento (A, B):

```
d = |Δy·Px - Δx·Py + Bx·Ay - By·Ax| / √(Δx² + Δy²)
```

Se `d < soglia` il tratto viene considerato "colpito" dalla gomma ed eliminato dalla traiettoria.

---

### 11. * XNOR Arco/Direzione
**File:** `trajectory.js`  
**Dominio:** Logica Booleana  

Determina la direzione corretta (oraria/antioraria) dell'arco per il rendering, evitando wrap-around angolare:

```javascript
const A = theta_0 > theta_1;
const B = Math.abs(theta_1 - theta_0) < Math.PI;
const ccw = (!A && !B) || (A && B); // XNOR
```

---

### 12. Observer/Subscriber Pattern
**File:** `state.js`  
**Dominio:** Design Pattern  

Implementazione lightweight del pattern Observer. I moduli si registrano tramite `subscribe(callback)`. Ogni modifica allo stato invoca `notifyListeners()`, che esegue tutti i callback registrati, aggiornando l'interfaccia in modo reattivo senza dipendenze circolari tra i file.

---

### 13. Game Loop via requestAnimationFrame
**File:** `canvas.js`  
**Dominio:** Rendering  

Loop di rendering continuo sincronizzato al refresh del monitor (tipicamente 60Hz) tramite `requestAnimationFrame`. Rispetto a `setInterval`, il browser può sospendere automaticamente il loop quando la tab non è visibile, risparmiando risorse CPU.

---

### 14. Debounce Input
**File:** `main.js`  
**Dominio:** Ottimizzazione I/O  

Pattern di ritardo controllato: ogni pressione di tasto resetta un timer. Solo al termine di un periodo di inattività (es. 300ms) la funzione costosa (`generatePreview`) viene effettivamente eseguita, evitando valanghe di chiamate API inutili.

---

### 15. Proximity Search — Snap al Punto Vicino
**File:** `canvas.js`  
**Dominio:** Ricerca Spaziale  

Ricerca lineare O(n) su tutti i punti esistenti. Ad ogni movimento del mouse, calcola la distanza Euclidea da ciascun punto salvato e, se la distanza è inferiore a un raggio di snap, sovrascrive le coordinate del cursore con quelle del punto vicino, facilitando la chiusura precisa delle forme.

---

### 16. Deep Clone via Serializzazione JSON
**File:** `state.js`  
**Dominio:** Gestione Memoria  

```javascript
const snapshot = JSON.parse(JSON.stringify(oggetto));
```

Crea una copia completamente indipendente dell'oggetto senza riferimenti condivisi, essenziale per l'immutabilità degli snapshot nello stack Undo/Redo.

---

### 17. Quantizzazione — Snap to Grid
**File:** `utils_drawing.js`  
**Dominio:** Segnali Discreti  

```javascript
Math.round(value / gridSize) * gridSize
```

Proietta qualsiasi coordinata continua sul multiplo di `gridSize` più vicino, garantendo allineamento preciso alla griglia di sfondo.

---

### 18. Rimappatura Cromatica Canali HEX
**File:** `theme.js`  
**Dominio:** Color Science  

Manipolazione aritmetica diretta sui canali RGB di un colore HEX: parsing con `parseInt(cc, 16)`, somma/sottrazione di un offset, saturazione in `[0, 255]` e riconversione in Base 16. Usata per derivare automaticamente colori secondari (bordi, bottoni) dal colore base scelto.

---

### 19. * Payload Assembly + Pen-Up Injection
**File:** `main.js`  
**Dominio:** Protocollo Dati  

Converte le strutture dati interne del canvas (oggetti `Point`, archi, testo) nel formato array piatto `[x, y, penUp]` atteso dal backend Python. Inietta automaticamente comandi di "penna alzata" tra forme non connesse fisicamente, prevenendo tratti indesiderati sul foglio.

---

### 20. * Proportional Resize con Riscalatura Coordinate
**File:** `canvas.js`  
**Dominio:** Grafica Adattiva  

Al ridimensionamento della finestra, calcola il rapporto di scala `ratio = W_new / W_old` e moltiplica tutte le coordinate relative di tutti i punti salvati per tale ratio, preservando visivamente la posizione dei disegni indipendentemente dalla risoluzione del monitor.

---

*Documento generato automaticamente — GUI DUMARM / POLITECNICO DI BARI*
