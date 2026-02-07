import { appState, TOOLS } from './state.js';
import { CanvasHandler } from './canvas.js';
import { Point } from './utils.js';
import { API } from './api.js';
import { ThemeManager } from './theme.js';

// --- Initialization ---

const canvas = document.getElementById('input_canvas');
const state = appState;

// Initialize State
state.init(canvas.width, canvas.height);

// Initialize Canvas Handler
const canvasHandler = new CanvasHandler(canvas, state);

// --- UI Elements ---

// --- UI Elements ---
// Initialized in initApp() to ensure DOM is ready
let ui = {};

function initUI() {
    ui = {
        statusDot: document.getElementById('status-dot'),
        statusText: document.getElementById('status-text'),
        btnConnect: document.getElementById('start-serial-btn'),

        btnModeContinuous: document.getElementById('mode-continuous-btn'),
        btnModeDiscrete: document.getElementById('mode-discrete-btn'),

        btnLine: document.getElementById('tool-line'),
        btnCircle: document.getElementById('tool-circle'),
        btnSquare: document.getElementById('tool-square'),
        btnPolygon: document.getElementById('tool-polygon'),
        btnSemicircle: document.getElementById('tool-semicircle'),

        btnUndo: document.getElementById('undo-btn'),
        btnRedo: document.getElementById('redo-btn'),
        btnClear: document.getElementById('clear-btn'),
        btnGridToggle: document.getElementById('grid-toggle-btn'),

        btnSend: document.getElementById('send-trajectory-btn'),
        btnStop: document.getElementById('stop-trajectory-btn'),
        btnHoming: document.getElementById('homing-btn'),
        btnCleanState: document.getElementById('clean-state-btn'),

        // Text Tools
        btnPresetLetter: document.getElementById('btn-preset-letter'),
        btnModeLinear: document.getElementById('mode-linear-btn'),
        btnModeCurved: document.getElementById('mode-curved-btn'),
        inputText: document.getElementById('text-input'),
        inputFontSize: document.getElementById('font-size'),
        controlsLinear: document.getElementById('linear-controls'),
        controlsCurved: document.getElementById('curved-controls'),

        // Linear Inputs
        inputLinX: document.getElementById('lin-x'),
        inputLinY: document.getElementById('lin-y'),
        inputLinAngle: document.getElementById('lin-angle'),

        // Workspace Config (Linear)
        inputWsX: document.getElementById('ws-x'),
        inputWsY: document.getElementById('ws-y'),
        inputWsW: document.getElementById('ws-w'),
        inputWsH: document.getElementById('ws-h'),

        // Curved Inputs (Text positioning)
        inputCurvRadius: document.getElementById('curv-radius'),
        inputCurvOffset: document.getElementById('curv-offset'),

        // Curved Workspace Config
        inputWsInnerR: document.getElementById('ws-inner-r'),
        inputWsOuterR: document.getElementById('ws-outer-r'),

        warningMsg: document.getElementById('text-warning'),
        // Redundant buttons removed
        // btnGenerate, btnNewline, btnClean removed

        // App Mode
        btnAppModeDrawing: document.getElementById('app-mode-drawing'),
        btnAppModeText: document.getElementById('app-mode-text'),
        sectionDrawing: document.getElementById('section-drawing-tools'),
        sectionText: document.getElementById('section-text-tools'),

        // Workspace Geometry Controls
        linearWsControls: document.getElementById('linear-ws-controls'),
        curvedWsControls: document.getElementById('curved-ws-controls'),
    };
}

// --- Event Listeners Wrapper ---

function setupEventListeners() {
    if (!ui.btnConnect) {
        console.error("UI not initialized!");
        return;
    }

    // Connection
    ui.btnConnect.addEventListener('click', async () => {
        await API.startSerial();
        updateSerialStatus();
    });

    // Drawing Modes
    ui.btnModeContinuous.addEventListener('click', () => {
        state.drawingMode = 'continuous';
        ui.btnModeContinuous.classList.add('active');
        ui.btnModeDiscrete.classList.remove('active');
        setTool(state.tool); // Reset partial states
    });

    ui.btnModeDiscrete.addEventListener('click', () => {
        state.drawingMode = 'discrete';
        ui.btnModeDiscrete.classList.add('active');
        ui.btnModeContinuous.classList.remove('active');
        setTool(state.tool); // Reset partial states
    });

    // Tools
    ui.btnLine.addEventListener('click', () => {
        setTool(TOOLS.LINE);
        updateToolUI();
    });

    ui.btnCircle.addEventListener('click', () => {
        setTool(TOOLS.CIRCLE);
        updateToolUI();
    });

    ui.btnSquare.addEventListener('click', () => {
        setTool(TOOLS.SQUARE);
        updateToolUI();
    });

    ui.btnPolygon.addEventListener('click', () => {
        setTool(TOOLS.POLYGON);
        updateToolUI();

        const sides = prompt('Number of sides (3-12):', '5');
        if (sides && !isNaN(sides)) {
            state.polygonSides = parseInt(sides);
            if (state.polygonSides < 3) state.polygonSides = 3;
            if (state.polygonSides > 12) state.polygonSides = 12;
        }
    });

    ui.btnSemicircle.addEventListener('click', () => {
        setTool(TOOLS.SEMICIRCLE);
        updateToolUI();
    });

    // Undo/Redo/Clear - Unified
    ui.btnUndo.addEventListener('click', () => {
        state.undo();
        updateUndoRedoUI();
    });

    ui.btnRedo.addEventListener('click', () => {
        state.redo();
        updateUndoRedoUI();
    });

    ui.btnClear.addEventListener('click', () => {
        if (confirm('Clear workspace? This will clear drawings and text.')) {
            state.resetWorkspace(); // Clears all and saves state
            // State observer will handle UI updates
        }
    });

    // Grid Controls
    ui.btnGridToggle.addEventListener('click', () => {
        state.showGrid = !state.showGrid;
        ui.btnGridToggle.textContent = state.showGrid ? '⊞ Grid: ON' : '⊞ Grid: OFF';
        ui.btnGridToggle.classList.toggle('active', state.showGrid);
    });



    // Commands
    ui.btnHoming.addEventListener('click', () => {
        API.homing();
    });

    ui.btnSend.addEventListener('click', () => {
        API.sendData();
    });

    ui.btnCleanState.addEventListener('click', () => {
        console.log("Cleaning all state...");
        // Reset drawing state
        state.points = [];
        state.sentPoints = [];
        state.trajectory.reset();
        state.sentTrajectory.reset();
        state.shapeStart = null;
        state.semicircleStart = null;
        state.circleDefinition = [];

        // Reset text state
        state.text = '';
        state.textPreview = [];
        state.generatedTextPatches = [];
        if (ui.inputText) ui.inputText.value = '';

        // Reset manipulator traces
        if (state.manipulator) state.manipulator.reset_trace();

        // Clear history for clean slate
        state.history = [];
        state.historyIndex = -1;
        state.saveState(); // Save the clean state

        // Update UI
        updateUndoRedoUI();
        if (canvasHandler) canvasHandler.animate();
        console.log("State cleaned.");
    });

    // Stop Trajectory
    ui.btnStop.addEventListener('click', async () => {
        console.log("Stopping trajectory...");
        await API.stopTrajectory();
    });

    // --- Mode Selection ---
    if (ui.btnAppModeDrawing && ui.btnAppModeText) {
        ui.btnAppModeDrawing.addEventListener('click', () => setAppMode('drawing'));
        ui.btnAppModeText.addEventListener('click', () => setAppMode('text'));
        setAppMode('drawing'); // Default
    }

    // --- Text Mode Listeners ---
    if (ui.btnPresetLetter) {
        ui.btnPresetLetter.addEventListener('click', () => {
            console.log("Applying LETTER MODE preset");
            // Switch to Linear Mode
            setTextMode('linear');

            // Set Values
            if (ui.inputFontSize) ui.inputFontSize.value = "0.01";
            if (ui.inputLinX) ui.inputLinX.value = "0.05";
            if (ui.inputLinY) ui.inputLinY.value = "0.15";
            if (ui.inputLinAngle) ui.inputLinAngle.value = "-360";

            // Trigger updates
            generatePreview();

            // Save state (debounced trigger not needed really, direct save)
            state.saveState();
        });
    }

    ui.btnModeLinear.addEventListener('click', () => setTextMode('linear'));
    ui.btnModeCurved.addEventListener('click', () => setTextMode('curved'));

    // Validate Inputs
    [ui.inputText, ui.inputFontSize, ui.inputLinX, ui.inputLinY, ui.inputLinAngle, ui.inputCurvRadius, ui.inputCurvOffset, ui.inputWsX, ui.inputWsY, ui.inputWsW, ui.inputWsH].forEach(el => {
        if (el) el.addEventListener('input', validateGeneratedPatches);
    });

    // --- Right Sidebar Toggle ---
    const rightSidebar = document.getElementById('right-sidebar');
    const rightSidebarToggle = document.getElementById('right-sidebar-toggle-btn');
    const rightSidebarClose = document.getElementById('close-right-sidebar-btn');

    if (rightSidebarToggle) {
        rightSidebarToggle.addEventListener('click', () => {
            // Only add class open if sidebar exists
            if (rightSidebar) rightSidebar.classList.add('open');
            rightSidebarToggle.style.display = 'none'; // Hide toggle
        });
    }

    if (rightSidebarClose) {
        rightSidebarClose.addEventListener('click', () => {
            if (rightSidebar) rightSidebar.classList.remove('open');
            if (rightSidebarToggle) rightSidebarToggle.style.display = 'flex'; // Show toggle
        });
    }

    // Sync Text Input to State
    // We save state only when user STOPS typing to avoid 100 history states for 1 word
    let textInputTimer = null;
    ui.inputText.addEventListener('input', () => {
        state.text = ui.inputText.value; // Realtime update

        clearTimeout(debounceTimer);
        debounceTimer = setTimeout(() => {
            generatePreview();
        }, 100);

        // Save to History (Debounced 500ms)
        clearTimeout(textInputTimer);
        textInputTimer = setTimeout(() => {
            // Only save if it's a meaningful change? 
            // StateManager logic handles duplicate states or we can check here?
            // For now, simpler: just save.
            state.saveState();
            updateUndoRedoUI();
        }, 500);
    });

    // Subscribe to state changes (Undo/Redo)
    state.subscribe((newState) => {
        // Update UI from State
        if (ui.inputText.value !== newState.text) {
            ui.inputText.value = newState.text;
        }

        // Restore Settings if they exist in state
        if (newState.textSettings) {
            const ts = newState.textSettings;
            if (ts.mode) setTextMode(ts.mode);

            if (ts.fontSize && ui.inputFontSize) ui.inputFontSize.value = ts.fontSize;

            if (ts.linX && ui.inputLinX) ui.inputLinX.value = ts.linX;
            if (ts.linY && ui.inputLinY) ui.inputLinY.value = ts.linY;
            if (ts.linAngle && ui.inputLinAngle) ui.inputLinAngle.value = ts.linAngle;

            if (ts.curvRadius && ui.inputCurvRadius) ui.inputCurvRadius.value = ts.curvRadius;
            if (ts.curvOffset && ui.inputCurvOffset) ui.inputCurvOffset.value = ts.curvOffset;

            if (ts.wsX && ui.inputWsX) ui.inputWsX.value = ts.wsX;
            if (ts.wsY && ui.inputWsY) ui.inputWsY.value = ts.wsY;
            if (ts.wsW && ui.inputWsW) ui.inputWsW.value = ts.wsW;
            if (ts.wsH && ui.inputWsH) ui.inputWsH.value = ts.wsH;
        }

        // Refresh Views
        if (state.appMode === 'text') generatePreview();
        updateUndoRedoUI();
        // Canvas is redrawn by loop, but we might want to ensure points are fresh
        // The animate loop in canvas.js reads state.points directly.
    });

    // Helper to sync text settings to state for Undo/Redo
    function syncTextSettings() {
        state.textSettings = {
            mode: state.textMode,
            fontSize: ui.inputFontSize.value,
            linX: ui.inputLinX.value,
            linY: ui.inputLinY.value,
            linAngle: ui.inputLinAngle.value,
            curvRadius: ui.inputCurvRadius.value,
            curvOffset: ui.inputCurvOffset.value,
            wsX: ui.inputWsX.value,
            wsY: ui.inputWsY.value,
            wsW: ui.inputWsW.value,
            wsH: ui.inputWsH.value
        };
    }

    // Update Text Mode to also sync settings
    const originalSetTextMode = window.setTextMode; // Assuming it's global or accessible? No, it's scoped.
    // We already found setTextMode definitions earlier. We should rely on our listeners.

    // Listeners for Parameters
    [ui.inputFontSize, ui.inputLinX, ui.inputLinY, ui.inputLinAngle, ui.inputCurvRadius, ui.inputCurvOffset, ui.inputWsX, ui.inputWsY, ui.inputWsW, ui.inputWsH].forEach(el => {
        if (el) {
            el.addEventListener('input', () => {
                clearTimeout(debounceTimer);
                debounceTimer = setTimeout(generatePreview, 100);

                // Save State on parameter change (Debounced)
                clearTimeout(textInputTimer);
                textInputTimer = setTimeout(() => {
                    syncTextSettings();
                    state.saveState();
                    updateUndoRedoUI();
                }, 500);
            });
        }
    });

    // Redundant text buttons removed

    // Workspace Inputs (Linear)
    [ui.inputWsX, ui.inputWsY, ui.inputWsW, ui.inputWsH].forEach(el => {
        if (el) {
            el.addEventListener('input', () => {
                updateWorkspaceState();
                validateText();
            });
        }
    });

    // Workspace Inputs (Curved)
    [ui.inputWsInnerR, ui.inputWsOuterR].forEach(el => {
        if (el) {
            el.addEventListener('input', () => {
                updateWorkspaceState();
            });
        }
    });
}

// Main Initialization
window.addEventListener('load', () => {
    console.log("Window Load - Initializing App");
    initUI();
    setupEventListeners();
    updateWorkspaceState();
    updateUndoRedoUI();
    if (typeof canvasHandler !== 'undefined' && canvasHandler) canvasHandler.resize();
    updateSerialStatus();
    new ThemeManager(); // Initialize Theme Manager
    setInterval(updateSerialStatus, 50);
});

// --- App Mode Logic ---

function setAppMode(mode) {
    state.appMode = mode;

    // Update Buttons
    ui.btnAppModeDrawing.classList.toggle('active', mode === 'drawing');
    ui.btnAppModeText.classList.toggle('active', mode === 'text');

    // Update Sections Visibility/State
    if (mode === 'drawing') {
        ui.sectionDrawing.style.display = 'block';
        ui.sectionText.style.display = 'none';

        // Clear Text Preview when switching to Drawing
        state.textPreview = [];
        state.generatedTextPatches = [];

        // Force redraw to clear ghost
        if (typeof canvasHandler !== 'undefined' && canvasHandler) canvasHandler.animate();

    } else {
        ui.sectionDrawing.style.display = 'none';
        ui.sectionText.style.display = 'block';
        // Regenerate text preview when switching to text mode
        generatePreview();
    }
}

// Mode listener attachment moved to setupEventListeners()

// --- Text Tool Logic ---

// Default State
state.textMode = 'linear'; // 'linear' | 'curved'
state.generatedTextPatches = [];

function getTextOptions() {
    return {
        mode: state.textMode,
        fontSize: parseFloat(ui.inputFontSize.value) || 0.05,
        x: parseFloat(ui.inputLinX.value) || 0.05,
        y: parseFloat(ui.inputLinY.value) || 0.0,
        angle: parseFloat(ui.inputLinAngle.value) || 0,
        radius: parseFloat(ui.inputCurvRadius.value) || 0.2,
        offset: parseFloat(ui.inputCurvOffset.value) || 90
    };
}

// Text mode listener attachment moved to setupEventListeners()

function setTextMode(mode) {
    state.textMode = mode;

    // Update Buttons
    ui.btnModeLinear.classList.toggle('active', mode === 'linear');
    ui.btnModeCurved.classList.toggle('active', mode === 'curved');

    // Update Text Controls Visibility
    if (mode === 'linear') {
        ui.controlsLinear.classList.remove('hidden');
        ui.controlsCurved.classList.add('hidden');
    } else {
        ui.controlsLinear.classList.add('hidden');
        ui.controlsCurved.classList.remove('hidden');
    }

    // Update Workspace Geometry Controls Visibility
    if (ui.linearWsControls && ui.curvedWsControls) {
        if (mode === 'linear') {
            ui.linearWsControls.classList.remove('hidden');
            ui.curvedWsControls.classList.add('hidden');
        } else {
            ui.linearWsControls.classList.add('hidden');
            ui.curvedWsControls.classList.remove('hidden');
        }
    }

    // Regenerate preview with new mode parameters
    generatePreview();

    // Re-validate with mode-specific constraints
    validateText();

    // Force Redraw of Workspace Background
    setTimeout(() => {
        if (canvasHandler) canvasHandler.animate();
    }, 10);
}

// Validate generated patches against robot reach and workspace limits
function validateGeneratedPatches() {
    if (!ui.warningMsg) return;

    const patches = state.textPreview || [];

    // No patches = no warning
    if (patches.length === 0) {
        ui.warningMsg.classList.add('hidden');
        return;
    }

    // Robot arm reach limits
    const l1 = state.settings.l1 || 0.170;
    const l2 = state.settings.l2 || 0.158;
    const maxReach = l1 + l2;
    const minReach = Math.abs(l1 - l2);

    // Counters
    let failReach = 0;
    let failWorkspace = 0;
    let totalPoints = 0;

    // Workspace Limits
    const mode = state.textMode;
    const linWs = state.settings.linearWorkspace;
    const curvWs = state.settings.curvedWorkspace;

    // Check each patch point
    for (const patch of patches) {
        // Handle both line and circle patches (circles have points too, usually sampled or start/end)
        // If it's a primitive circle, we might need to check center +/- radius?
        // textPreview usually contains SAMPLED lines or primitives.
        // API.generateText returns patches. If they are primitives, they might just have start/end.
        // For accurate validation, we might want to assume lines for text.
        // text_to_traj in char_gen can do sampling.

        let pointsToCheck = patch.points || [];

        for (const pt of pointsToCheck) {
            const x = pt[0];
            const y = pt[1];
            const distance = Math.sqrt(x * x + y * y);
            totalPoints++;

            // 1. PHYSICAL REACH (RED)
            if (distance > maxReach || distance < minReach) {
                failReach++;
            } else {
                // 2. LOGICAL WORKSPACE (YELLOW) - Only check if physically reachable
                if (mode === 'linear' && linWs) {
                    // Check if inside Rect
                    // Rect: x, y, w, h. 
                    // x,y is bottom-left? We need to clarify standard.
                    // Assuming inputs are: x, y, w, h.
                    // Valid X: [x, x+w]
                    // Valid Y: [y, y+h]
                    // Note: Y might be negative.
                    if (x < linWs.x || x > linWs.x + linWs.w ||
                        y < linWs.y || y > linWs.y + linWs.h) {
                        failWorkspace++;
                    }
                } else if (mode === 'curved' && curvWs) {
                    // Check if inside Donut
                    // Radius: [inner, outer]
                    // Angle: Standard right sector? (-90 to +90?)
                    // User sketch shows right semi-circle.
                    if (distance < curvWs.innerRadius || distance > curvWs.outerRadius) {
                        failWorkspace++;
                    }
                    if (x < 0) { // Keep to right side (hemisphere)
                        failWorkspace++;
                    }
                }
            }
        }
    }

    // Priority: Reach Error (Red) > Workspace Warning (Yellow)
    if (failReach > 0) {
        ui.warningMsg.textContent = `⚡ CRTICIAL: ${failReach} points out of robot reach!`;
        ui.warningMsg.style.color = '#ff4444'; // Red
        ui.warningMsg.style.borderColor = '#ff4444';
        ui.warningMsg.classList.remove('hidden');
    }
    else if (failWorkspace > 0) {
        ui.warningMsg.textContent = `⚠ WARNING: Text exceeds ${mode} workspace boundaries.`;
        ui.warningMsg.style.color = '#ffbb33'; // Orange/Yellow
        ui.warningMsg.style.borderColor = '#ffbb33';
        ui.warningMsg.classList.remove('hidden');
    }
    else {
        ui.warningMsg.classList.add('hidden');
    }
}

// Legacy function kept for compatibility
function validateText() {
    // Real validation happens in validateGeneratedPatches after patches are generated
}

function updateWorkspaceState() {
    // Parse Linear Workspace inputs
    const x = parseFloat(ui.inputWsX?.value) || 0.01;
    const y = parseFloat(ui.inputWsY?.value) || -0.18;
    const w = parseFloat(ui.inputWsW?.value) || 0.27;
    const h = parseFloat(ui.inputWsH?.value) || 0.36;

    // Update Linear Workspace State
    if (state.settings.linearWorkspace) {
        state.settings.linearWorkspace.x = x;
        state.settings.linearWorkspace.y = y;
        state.settings.linearWorkspace.w = w;
        state.settings.linearWorkspace.h = h;
    }

    // Parse Curved Workspace inputs
    const innerR = parseFloat(ui.inputWsInnerR?.value) || 0.10;
    const outerR = parseFloat(ui.inputWsOuterR?.value) || 0.30;

    // Update Curved Workspace State
    if (state.settings.curvedWorkspace) {
        state.settings.curvedWorkspace.innerRadius = innerR;
        state.settings.curvedWorkspace.outerRadius = outerR;
    }

    // Trigger Canvas Redraw for real-time feedback
    if (typeof canvasHandler !== 'undefined' && canvasHandler) {
        canvasHandler.animate();
    }
}

// Workspace input listeners moved to setupEventListeners()

// Initialize state from default inputs - moved to setupEventListeners/window.load
// updateWorkspaceState(); // Commented - runs in init

// Real-time Text Visualization handled in setupEventListeners()

// Validation listeners moved to setupEventListeners()

// Override Send Button to include Text if valid?
// The user request says: "Clicca Generate Text per visualizzare... Clicca Send Trajectory per avviare".
// "Send Trajectory" normally sends `state.trajectory`.
// If we generated text, should we Convert text patches to `state.trajectory`?
// Yes.
// So when clicking Generate, we should probably update the main trajectory or a separate one?
// --- Real-time Text Logic ---

let debounceTimer = null;

async function generatePreview() {
    const text = ui.inputText.value;
    if (!text) {
        state.textPreview = [];
        state.generatedTextPatches = [];
        validateText(); // Will hide warning
        return;
    }

    const options = getTextOptions();
    console.log("Generating Preview for:", text); // Debug

    try {
        const patches = await API.generateText(text, options);
        console.log("Patches:", patches ? patches.length : 0);

        state.textPreview = patches || [];
        state.generatedTextPatches = patches || [];

        // Text patches are stored separately in state.textPreview
        // They are combined with drawing trajectory only when sending (getTrajectoryPayload)

        validateGeneratedPatches(); // Validate actual coordinates against robot reach

    } catch (e) {
        console.error("Preview Generation Error:", e);
    }
}

// All event listeners for text inputs are attached in setupEventListeners().


// --- Helper Functions ---

function updateSerialStatus() {
    API.getSerialStatus().then(online => {
        state.isSerialOnline = online;
        if (online) {
            ui.statusText.textContent = "Connected";
            ui.statusText.style.color = "#00ff88"; // Neon Green
            ui.statusDot.classList.add('online');
            ui.btnConnect.disabled = true;
            ui.btnConnect.textContent = "Serial Online";
        } else {
            ui.statusText.textContent = "Disconnected (Sim Mode)";
            ui.statusText.style.color = "#aaa";
            ui.statusDot.classList.remove('online');
            ui.btnConnect.disabled = false;
        }
    });

    // POLLING LOOP for Simulation/Position
    // We poll faster to get smooth animation
    API.getPosition().then(pos => {
        // console.log("DEBUG: Polled Pos:", pos); // Debugging
        if (state.manipulator && pos && pos.length >= 2) {
            // Update Manipulator State (for drawing)
            // pos = [q0, q1, penUp]
            state.manipulator.q = [pos[0], pos[1]];

            // Note: Pen state from backend might be useful
            // But we trust local state for drawing logic usually.
            // For Simulation, we want to see the "Virtual" pen state?
            // Optional.
        }
    }).catch(e => console.warn("Polling Error:", e));
}

// Start Fast Polling (50ms = 20Hz)
// --- Cleanup & Helpers ---
// (Immediate calls removed to prevent crash before UI init)
// API.initCallbacks and Keydown listeners preserved




function getTrajectoryPayload() {
    // Collect data to save in the correct format for backend
    // Priority order: 
    // 1. Text (Linear by definition usually)
    // 2. Linear Drawings (Lines, Squares, Polygons)
    // 3. Curved Drawings (Circles, Semicircles)

    const payload = [];

    // 1. Add Text Patches if present
    if (state.generatedTextPatches && state.generatedTextPatches.length > 0) {
        payload.push(...state.generatedTextPatches);
    }

    // 2. Process Drawing Trajectory
    const linearPayload = [];
    const curvedPayload = [];

    if (state.trajectory && state.trajectory.data && state.trajectory.data.length > 0) {
        for (let t of state.trajectory.data) {

            // Helper to extract data based on type
            let item = null;

            if (t.type === 'line') {
                const p0 = t.data[0];
                const p1 = t.data[1];
                const penup = t.data[2];

                if (!p0 || !p1) continue;

                item = {
                    'type': 'line',
                    'points': [[p0.actX, p0.actY], [p1.actX, p1.actY]],
                    'data': { 'penup': penup || false }
                };

                // Add to Linear
                linearPayload.push(item);

            } else if (t.type === 'circle') {
                const c = t.data[0];
                const rPixels = t.data[1];
                const r = rPixels * state.settings.m_p;
                const theta0 = t.data[2];
                const theta1 = t.data[3];
                const penup = t.data[4];
                // Start/End points stored in data[5] and data[6] (optional but good for exactness)
                let pStart = t.data[5];
                let pEnd = t.data[6];

                if (!c) continue;

                // Calculate Start/End if not stored
                if (!pStart) {
                    pStart = {
                        actX: c.actX + r * Math.cos(theta0),
                        actY: c.actY + r * Math.sin(theta0)
                    };
                }
                if (!pEnd) {
                    pEnd = {
                        actX: c.actX + r * Math.cos(theta1),
                        actY: c.actY + r * Math.sin(theta1)
                    };
                }

                // Calculate Delta Angle (True Arc Logic)
                const A = theta0 > theta1;
                const B = Math.abs(theta1 - theta0) < Math.PI;
                const ccw = (!A && !B) || (A && B);

                let delta = theta1 - theta0;
                if (ccw) {
                    if (delta <= 0) delta += 2 * Math.PI;
                } else {
                    if (delta >= 0) delta -= 2 * Math.PI;
                }

                // Construct Circle Patch (No Sampling)
                item = {
                    'type': 'circle',
                    'points': [[pStart.actX, pStart.actY], [pEnd.actX, pEnd.actY]],
                    'data': {
                        'penup': penup || false,
                        'center': [c.actX, c.actY],
                        'radius': r,
                        'angle': delta // Explicit Angle for Backend
                    }
                };

                // Add to Curved
                curvedPayload.push(item);
            }
        }
    }

    // Combine Drawings: Linear First, then Curved
    const drawingPayload = [...linearPayload, ...curvedPayload];

    // Merge Text and Drawings with Smart Jumps
    // We already have 'payload' containing Text.
    // We need to append 'drawingPayload' but ensure Jumps between disjoint segments.

    // Helper to add jump if needed
    const safePush = (list, item) => {
        if (list.length > 0) {
            const last = list[list.length - 1];
            const lastEnd = last.points[1];
            const currStart = item.points[0];

            // Calc distance
            const dx = currStart[0] - lastEnd[0];
            const dy = currStart[1] - lastEnd[1];
            const dist = Math.sqrt(dx * dx + dy * dy);

            if (dist > 0.001) {
                // Insert Jump
                list.push({
                    'type': 'line',
                    'points': [lastEnd, currStart],
                    'data': { 'penup': true }
                });
            }
        }
        list.push(item);
    };

    // Apply Smart Jumps to the sorted drawing list
    // (Note: The user re-ordering might break continuity, so we MUST insert jumps)
    const finalDrawing = [];
    for (let item of drawingPayload) {
        safePush(finalDrawing, item);
    }

    // Now append to main payload (which has text)
    for (let item of finalDrawing) {
        safePush(payload, item);
    }

    return payload;
}



// --- Helper Functions ---

function setTool(tool) {
    state.tool = tool;
    state.circleDefinition = []; // Reset partials
    state.rectangleStart = null; // Reset rectangle start
    state.semicircleStart = null; // Reset semicircle start
    state.fullcircleStart = null; // Reset fullcircle start
    state.shapeStart = null;
}

// clearTextState removed to allow combining Text + Drawing


function updateToolUI() {
    ui.btnLine.classList.toggle('active', state.tool === TOOLS.LINE);
    ui.btnCircle.classList.toggle('active', state.tool === TOOLS.CIRCLE);
    ui.btnSquare.classList.toggle('active', state.tool === TOOLS.SQUARE);
    ui.btnPolygon.classList.toggle('active', state.tool === TOOLS.POLYGON);
    ui.btnSemicircle.classList.toggle('active', state.tool === TOOLS.SEMICIRCLE);
}

function updateUndoRedoUI() {
    ui.btnUndo.disabled = !state.canUndo();
    ui.btnRedo.disabled = !state.canRedo();
}

// Old updateSerialStatus removed (duplicate)
// setSerialUI removed (unused)

// --- API Callbacks ---

API.initCallbacks({
    onLog: (msg) => {
        // Optional: display in UI console
        const consoleEl = document.getElementById('console-output');
        if (consoleEl) consoleEl.textContent = msg;
    },

    onDrawPose: (q) => {
        if (state.manipulator) state.manipulator.q = q;
    },

    onDrawTraces: (points) => {
        // points = [[x1, y1], [x2, y2]...] ? 
        // Original: js_draw_traces(points) -> man.add2trace([points[0][i], points[1][i]])
        // Check protocol.
        // Assuming points is list of configs? Or points? 
        // Original cnv.js:
        /*
        function js_draw_traces(points) {
            for(var i = 0; i < points[0].length; i++){
                man.add2trace([points[0][i], points[1][i]]);
            }
        }
        */
        // It seems to receive a zipped structure or parallel arrays?
        // Let's assume standard behavior for now.
        if (state.manipulator) {
            // Basic implementation
        }
    },

    onGetData: () => {
        return getTrajectoryPayload();
    }
});

// Initial Status Check -> Moved to Init
// updateSerialStatus();
// --- Keyboard Shortcuts ---
document.addEventListener('keydown', (e) => {
    // Ignore if typing in input fields
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;

    // Ctrl/Cmd + Z: Undo
    if ((e.ctrlKey || e.metaKey) && e.key === 'z' && !e.shiftKey) {
        e.preventDefault();
        state.undo();
        updateUndoRedoUI();
    }

    // Ctrl/Cmd + Y or Ctrl/Cmd + Shift + Z: Redo
    if ((e.ctrlKey || e.metaKey) && (e.key === 'y' || (e.key === 'z' && e.shiftKey))) {
        e.preventDefault();
        state.redo();
        updateUndoRedoUI();
    }

    // Tool shortcuts (only if not holding Ctrl/Cmd)
    if (!e.ctrlKey && !e.metaKey && !e.altKey) {
        switch (e.key.toLowerCase()) {
            case 'l':
                setTool(TOOLS.LINE);
                updateToolUI();
                break;
            case 'c':
                setTool(TOOLS.CIRCLE);
                updateToolUI();
                break;
            case 'r':
                setTool(TOOLS.RECTANGLE);
                updateToolUI();
                break;
            case 'p':
                if (!ui.btnPolygon.disabled) {
                    setTool(TOOLS.POLYGON);
                    updateToolUI();
                }
                break;
            case 's':
                setTool(TOOLS.SEMICIRCLE);
                updateToolUI();
                break;
            case 'o':
                setTool(TOOLS.FULLCIRCLE);
                updateToolUI();
                break;
            case 'g':
                ui.btnGridToggle.click();
                break;
            case 'delete':
                ui.btnClear.click();
                break;
        }
    }
});
