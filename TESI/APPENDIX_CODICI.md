# Appendice Tecnologica: Codici Sorgente e Algoritmi Chiave

Questa appendice raccoglie le implementazioni software più significative del progetto, suddivise per area funzionale (Sistema Globale, Disegno, Testo, Immagini, Architettura, UI, QA, Futuro).

---

## A. SISTEMA GLOBALE (Backend & Controllo)

### A.1 Cinematica Inversa (Python)
**File:** `lib/trajpy.py`
**Funzione:** `ik`
**Descrizione:**
Risolve la posizione dei giunti ($q_1, q_2$) dato un target cartesiano $(x, y)$. Gestisce singolarità e selezione della configurazione ottimale.

```python
def ik(x:float, y:float, z:float = 0, theta:float = None, sizes:dict[float] = {'l1':0.170 ,'l2':0.158}, limits:dict[float] = None, seed_q:np.ndarray = None) -> np.ndarray:
    a1 = sizes['l1']
    a2 = sizes['l2']
    solutions = []

    # 1. Calcolo Soluzione Standard (Teorema del Coseno)
    cos_q2 = (x**2+y**2-a1**2-a2**2)/(2*a1*a2)
    
    # Clamping per stabilità numerica
    if cos_q2 > 1.0: cos_q2 = 1.0
    if cos_q2 < -1.0: cos_q2 = -1.0
    
    q2_std = acos(cos_q2)
    q1_std = atan2(y,x)-atan2(a2*sin(q2_std), a1+a2*cos(q2_std))
    solutions.append((q1_std, q2_std))
        
    # 2. Calcolo Soluzione Alternativa (Gomito Opposto)
    if abs(q2_std) > 1e-6:
        q2_alt = -q2_std
        q1_alt = atan2(y,x)-atan2(a2*sin(q2_alt), a1+a2*cos(q2_alt))
        solutions.append((q1_alt, q2_alt))

    # 3. Selezione Ottimale (Nearest Neighbor)
    if seed_q is not None:
        best_sol = None
        min_dist = float('inf')
        curr_q1 = seed_q[0,0]
        curr_q2 = seed_q[1,0]
        
        for (q1, q2) in solutions:
            dist = (q1 - curr_q1)**2 + (q2 - curr_q2)**2
            if dist < min_dist:
                min_dist = dist
                best_sol = (q1, q2)
        
        if best_sol: return np.array([[best_sol[0], best_sol[1], z]]).T
            
    return np.array([[solutions[0][0], solutions[0][1], z]]).T
```

### A.2 Profili di Moto Dinamici (Python)
**File:** `lib/trajpy.py` (Helper in gui)
**Funzione:** `get_profile_law`
**Descrizione:**
Calcola la legge oraria normalizzata $s(t)$ per profili avanzati come S-Curve e Quintic, garantendo movimenti fluidi.

```python
def get_profile_law(profile: str, distance: float, max_acc: float, max_vel: float) -> tuple[Callable[[float], float], float]:
    dist = abs(distance)
    
    if profile == 'quintic':
        # Profilo Polinomiale di 5° Grado (Minimum Jerk)
        # tf = sqrt(5.77 * s / a_max)
        tf_acc = sqrt(5.7735 * dist / max_acc)
        tf_vel = 1.875 * dist / max_vel
        tf = max(tf_acc, tf_vel)
        
        # Legge s(t) = 10t^3 - 15t^4 + 6t^5
        s_func = lambda t: ((t/tf)**3 * (10 - 15*(t/tf) + 6*(t/tf)**2)) if tf > 0 else 1.0
        return (s_func, tf)

    return (lambda t: t/tf, tf)
```

### A.3 Protocollo di Comunicazione (Python)
**File:** `lib/binary_protocol.py`
**Descrizione:**
Codifica i punti della traiettoria in pacchetti binari con CRC32 per integrità dei dati.

```python
def encode_trajectory_point(q0, q1, dq0, dq1, ddq0, ddq1, pen_up) -> bytes:
    cmd = 0x01 # CMD_TRAJECTORY
    payload = struct.pack('<ffffffB', q0, q1, dq0, dq1, ddq0, ddq1, 1 if pen_up else 0)
    
    checksum_data = struct.pack('B', cmd) + payload
    crc = zlib.crc32(checksum_data) & 0xFFFFFFFF
    
    header = struct.pack('BB', 0xA5, 0x5A)
    return header + checksum_data + struct.pack('<I', crc)
```

---

## B. DISEGNO (Frontend & Interazione)

### B.1 Ghost Robot (Javascript)
**File:** `layout/js/manipulator.js`
**Funzione:** `draw_pose`
**Descrizione:**
Visualizzazione in tempo reale su Canvas.

```javascript
draw_pose(ctx) {
    const origin = this.settings['origin'];
    ctx.strokeStyle = "#ff8800"; 
    ctx.moveTo(origin.x, origin.y);
    ctx.lineTo(this.p[0], this.p[1]); // Gomito
    ctx.lineTo(this.end_eff[0], this.end_eff[1]); // Polso
    ctx.stroke();

    // End Effector: Ciano (Up) vs Rosso (Down)
    const effColor = this.penUp ? '#00e5ff' : '#ff4444';
    this.drawJoint(ctx, this.end_eff[0], this.end_eff[1], effColor);
}
```

### B.2 Generazione Forme (Javascript)
**File:** `layout/js/utils_drawing.js`
**Algoritmo:** `calculateStar`

```javascript
export function calculateStar(center, radius, points, innerRatio = 0.5, rotation = 0, settings) {
    const starPoints = [];
    const totalPoints = points * 2; 
    const angleStep = Math.PI / points; 

    for (let i = 0; i < totalPoints; i++) {
        const r = (i % 2 === 0) ? radius : radius * innerRatio;
        const angle = i * angleStep + rotation;

        const x = center.relX + r * Math.cos(angle);
        const y = center.relY + r * Math.sin(angle);
        starPoints.push(new Point(x, y, settings));
    }
    return starPoints;
}
```

---

## C. TESTO (Typography)

### C.1 Font Vettoriali (Python)
**File:** `lib/char_gen.py`
**Descrizione:** Conversione stringhe in primitive geometriche.

```python
def text_to_traj(text: str, start_pos: tuple, font_size: float, char_spacing: float):
    cursor_x, cursor_y = start_pos
    traj_patches = []
    
    for char in text:
        primitives = get_char_strokes(char)
        for prim in primitives:
            # Trasformazione in Coordinate Mondo
            world_points = []
            for p in prim['points']:
                wx = cursor_x + p[0] * font_size * 0.8 
                wy = cursor_y + p[1] * font_size
                world_points.append((wx, wy))
            
            traj_patches.append({'type': 'polyline', 'points': world_points})
            
        cursor_x += (font_size * 0.8) + char_spacing
    return traj_patches
```

### C.2 Testo Curvo (Python)
**File:** `lib/transform.py`
**Descrizione:** Mappatura Polare per testo radiale.

```python
def apply_curved_transform(trajectory, radius, start_angle_deg=90):
    transformed = []
    start_angle_rad = math.radians(start_angle_deg)
    
    for point in trajectory:
        current_r = radius + point['y']
        angle_offset = point['x'] / radius
        theta = start_angle_rad - angle_offset 
        
        x_curved = current_r * math.cos(theta)
        y_curved = current_r * math.sin(theta)
        transformed.append({'x': x_curved, 'y': y_curved, 'z': point['z']})
        
    return transformed
```

---

## D. IMMAGINI (Computer Vision)

### D.1 Elaborazione Raster (Python)
**File:** `lib/image_processor.py`
**Funzione:** `_process_raster`

```python
def _process_raster(image_data, options):
    img = cv2.imdecode(np.frombuffer(image_data, np.uint8), cv2.IMREAD_GRAYSCALE)
    edges = cv2.Canny(img, threshold, threshold * 2)
    contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    
    paths = []
    for cnt in contours:
        if cv2.arcLength(cnt, True) < 15: continue
        # Douglas-Peucker Simplification
        epsilon = 0.0005 * cv2.arcLength(cnt, True)
        approx = cv2.approxPolyDP(cnt, epsilon, True)
        paths.append([ (p[0][0], p[0][1]) for p in approx ])
        
    return paths
```

---

## E. ARCHITETTURA SW (Integrazione)

### E.1 Entry Point e Bridge Frontend (Python)
**File:** `main.py`
**Descrizione:**
Punto di ingresso dell'applicazione che inizializza il server Eel e avvia il thread.

```python
if __name__ == "__main__":
    SETTINGS['ser_started'] = scm.ser_init(SERIAL_PORT)
    serial_manager.start_monitor()
    eel.init("layout") 
    eel.start("index.html", block=True)
```

### E.2 Gestione Multithreading (Python)
**File:** `serial_manager.py`
**Classe:** `SerialManager`
**Descrizione:**
Implementa un thread "Daemon" che monitora la seriale.

```python
class SerialManager:
    def start_monitor(self):
        self.monitor_thread = threading.Thread(target=self._monitor_loop, daemon=True)
        self.monitor_thread.start()

    def _monitor_loop(self):
        while not self.stop_event.is_set():
            if SETTINGS['ser_started']:
                # Lettura feedback ...
                if feedback: state.firmware.q0 = feedback['q0']
            
            # Aggiornamento Asincrono GUI
            eel.js_draw_pose([state.firmware.q0, state.firmware.q1, state.firmware.penup])
            sleep(0.005)
```

### E.3 Automazione Git (Batch)
**File:** `AGGIORNA_ONLINE.bat`

```batch
@echo off
git add .
set /p commitMsg="Messaggio: "
git commit -m "%commitMsg%"
git push origin main
```

---

## F. INTERFACCIA UTENTE (Front-End & UX)

### F.1 Stile "Modern Glass" (CSS)
**File:** `layout/css/sidebar_right.css`
**Descrizione:**
Esempio di styling CSS per i pannelli laterali con effetto traslucido, gradienti e transizioni fluide (Glassmorphism Light).

```css
.right-sidebar {
    position: fixed;
    right: -220px;
    width: 210px;
    /* Gradiente Moderno Scuro */
    background: linear-gradient(180deg, var(--bg-secondary) 0%, var(--bg-tertiary) 100%);
    border-left: 1px solid rgba(255, 255, 255, 0.1);
    box-shadow: -4px 0 20px rgba(0, 0, 0, 0.3);
    transition: right 0.3s cubic-bezier(0.4, 0, 0.2, 1);
    z-index: 100;
}
```

---

## G. ANALISI AVANZATA IMMAGINI

### G.1 Analisi Complessità e Scheletrizzazione (Python)
**File:** `lib/image_processor.py`
**Funzione:** `analyze_complexity` e `_skeletonize`
**Descrizione:**
Algoritmo ibrido che decide se usare il semplice outline (Canny) o una scheletrizzazione morfologica (Zhang-Suen approssimato) basandosi sulla densità dei pixel.

```python
def analyze_complexity(binary):
    edge_pixels = cv2.countNonZero(binary)
    density = edge_pixels / total_pixels
    dist_map = cv2.distanceTransform(binary, cv2.DIST_L2, 5)
    max_half_width = np.max(dist_map)
    
    # Se molto denso (>12%) o linee sottili, usa Skeleton
    if density > 0.12 or max_half_width < 2.0:
        return 'skeleton'
    return 'outline'

def _skeletonize(img):
    # Scheletrizzazione Morfologica iterativa
    skel = np.zeros(img.shape, np.uint8)
    element = cv2.getStructuringElement(cv2.MORPH_CROSS, (3,3))
    
    while True:
        eroded = cv2.erode(img, element)
        temp = cv2.dilate(eroded, element)
        temp = cv2.subtract(img, temp)
        skel = cv2.bitwise_or(skel, temp)
        img = eroded.copy()
        if cv2.countNonZero(img) == 0: break
    return skel
```

---

## H. GESTIONE QUALITÀ (Test Automation)

### H.1 Test Funzionale Scaling (Python)
**File:** `tests/test_scaling.py`
**Descrizione:**
Script di test automatizzato che genera un'immagine sintetica e verifica che l'algoritmo di `process_image` rispetti le dimensioni target richieste (0.2m x 0.3m) con tolleranza < 1cm.

```python
def test_processing():
    # Creazione Immagine Dummy 2:1
    img_data = create_dummy_image()
    
    # Richiesta Scaling Non-Uniforme (Distorsione Intenzionale)
    options = { 'width': 0.20, 'height': 0.30 } # Target
    
    patches = image_processor.process_image(img_data, options)
    
    # Verifica Dimensioni Reali
    w_real = max(xs) - min(xs)
    h_real = max(ys) - min(ys)
    
    if abs(h_real - 0.20) < 0.01 and abs(w_real - 0.30) < 0.01:
        print("PASS: Dimensions match targets.")
    else:
        print(f"FAIL: {h_real} vs 0.20 / {w_real} vs 0.30")
```

---

## I. SVILUPPI FUTURI (AI & Deep Learning)

### I.1 Integrazione Modelli Neurali (Python)
**File:** `download_dexined.py`
**Descrizione:**
Infrastruttura predisposta per l'uso di Reti Neurali (DexiNed - Dense Extreme Inception Network) per una edge detection di qualità "artistica", superiore agli algoritmi classici.

```python
def main():
    # Download Automatico Modello ONNX (HuggingFace)
    dexined_url = "https://huggingface.co/opencv/edge_detection_dexined/.../dexined.onnx"
    dexined_path = os.path.join(models_dir, "dexined.onnx")
    
    if not os.path.exists(dexined_path):
        print("Scaricamento DexiNed Model (Professional Grade)...")
        # SSL Bypass per reti universitarie
        ssl._create_default_https_context = ssl._create_unverified_context
        # ... download logic ...
```
