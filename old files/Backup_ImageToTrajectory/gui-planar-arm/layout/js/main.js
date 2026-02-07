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
        btnStar: document.getElementById('tool-star'),
        btnHeart: document.getElementById('tool-heart'),
        btnSpiral: document.getElementById('tool-spiral'),
        btnCross: document.getElementById('tool-greek-cross'),
        btnRhombus: document.getElementById('tool-rhombus'),
        btnTriangle: document.getElementById('tool-triangle'),
        btnTriangleScalene: document.getElementById('tool-triangle-scalene'),
        btnTriangleRight: document.getElementById('tool-triangle-right'),
        btnTriangleIsosceles: document.getElementById('tool-triangle-isosceles'),

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
        btnAddText: document.getElementById('btn-add-text'),
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

        // CAD Import Panel
        cadPanel: document.getElementById('cad-import-panel'),
        cadPanelHeader: document.getElementById('cad-panel-header'),
        cadFileInput: document.getElementById('cad-file-input'),
        btnSelectCad: document.getElementById('btn-select-cad'),
        cadFileName: document.getElementById('cad-file-name'),
        cadWarning: document.getElementById('cad-warning'),
        btnConvertCad: document.getElementById('btn-convert-cad'),
        cadControls: document.getElementById('cad-controls'),
        cadUnits: document.getElementById('cad-units'),
        cadScale: document.getElementById('cad-scale'),
        cadOffX: document.getElementById('cad-off-x'),
        cadOffY: document.getElementById('cad-off-y'),
        btnAutoFit: document.getElementById('btn-auto-fit'),
        btnConfirmImport: document.getElementById('btn-confirm-import'),
        btnCancelImport: document.getElementById('btn-cancel-import'),

        // Image Tools
        btnAppModeImage: document.getElementById('app-mode-image'),
        sectionImage: document.getElementById('section-image-tools'),
        inputImageFile: document.getElementById('image-upload'),
        imgPreviewContainer: document.getElementById('image-preview-container'),
        imgPreviewImg: document.getElementById('image-preview-img'),
        inputImgWidth: document.getElementById('img-width'),
        inputImgX: document.getElementById('img-x'),
        inputImgY: document.getElementById('img-y'),
        inputImgRotation: document.getElementById('img-rotation'),
        inputImgThreshold: document.getElementById('img-threshold'),
        btnProcessImage: document.getElementById('btn-process-image'),
        btnConfirmImage: document.getElementById('btn-confirm-image'),


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

    setupImageEvents();

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

    ui.btnStar.addEventListener('click', () => {
        setTool(TOOLS.STAR);
        updateToolUI();
    });

    if (ui.btnHeart) {
        ui.btnHeart.addEventListener('click', () => {
            setTool(TOOLS.HEART);
            updateToolUI();
        });
    }

    if (ui.btnSpiral) {
        ui.btnSpiral.addEventListener('click', () => {
            setTool(TOOLS.SPIRAL);
            updateToolUI();
        });
    }

    if (ui.btnCross) {
        ui.btnCross.addEventListener('click', () => {
            setTool(TOOLS.CROSS);
            updateToolUI();
        });
    }

    if (ui.btnRhombus) {
        ui.btnRhombus.addEventListener('click', () => {
            setTool(TOOLS.RHOMBUS);
            updateToolUI();
        });
    }

    if (ui.btnTriangle) {
        ui.btnTriangle.addEventListener('click', () => {
            setTool(TOOLS.TRIANGLE);
            updateToolUI();
        });
    }

    if (ui.btnTriangleScalene) {
        ui.btnTriangleScalene.addEventListener('click', () => {
            setTool(TOOLS.TRIANGLE_SCALENE);
            updateToolUI();
        });
    }

    if (ui.btnTriangleRight) {
        ui.btnTriangleRight.addEventListener('click', () => {
            setTool(TOOLS.TRIANGLE_RIGHT);
            updateToolUI();
        });
    }

    if (ui.btnTriangleIsosceles) {
        ui.btnTriangleIsosceles.addEventListener('click', () => {
            setTool(TOOLS.TRIANGLE_ISOSCELES);
            updateToolUI();
        });
    }

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

    // --- Add Text Button ---
    if (ui.btnAddText) {
        ui.btnAddText.addEventListener('click', addTextToTrajectory);
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

    // Init Callbacks HERE to ensure window.eel is ready
    API.initCallbacks({
        onLog: (msg) => {
            const consoleEl = document.getElementById('console-output');
            if (consoleEl) consoleEl.textContent = msg;
        },

        onDrawPose: (q) => {
            // console.log("DEBUG: JS received pose:", q);
            if (state.manipulator) state.manipulator.q = q;
        },

        onDrawTraces: (points) => {
            if (state.manipulator) {
                // ...
            }
        },

        onGetData: () => {
            return getTrajectoryPayload();
        }
    });

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
    if (ui.btnAppModeImage) ui.btnAppModeImage.classList.toggle('active', mode === 'image');

    ui.sectionDrawing.style.display = (mode === 'drawing') ? 'block' : 'none';
    ui.sectionText.style.display = (mode === 'text') ? 'block' : 'none';
    if (ui.sectionImage) ui.sectionImage.style.display = (mode === 'image') ? 'block' : 'none';

    if (mode === 'drawing') {
        // Force redraw to clear ghost
        if (typeof canvasHandler !== 'undefined' && canvasHandler) canvasHandler.animate();
    } else if (mode === 'text') {
        generatePreview();
    }
}

// Mode listener attachment moved to setupEventListeners()

// --- Text Tool Logic ---

// Default State
state.textMode = 'linear'; // 'linear' | 'curved'
state.generatedTextPatches = [];
state.accumulatedTextPatches = []; // Accumulated patches for multiple texts

// Function to add current text to the accumulated trajectory
async function addTextToTrajectory() {
    const text = ui.inputText.value;
    if (!text) {
        alert('Inserisci del testo prima di aggiungerlo!');
        return;
    }

    const options = getTextOptions();
    console.log("Adding Text to Trajectory:", text);

    try {
        const patches = await API.generateText(text, options);
        if (patches && patches.length > 0) {
            // Accumulate patches instead of replacing
            state.accumulatedTextPatches.push(...patches);
            state.generatedTextPatches = [...state.accumulatedTextPatches];

            // Clear only the text input, keep position/angle for user to modify
            ui.inputText.value = '';

            // Clear the preview (current text only)
            state.textPreview = [];

            console.log(`Text added! Total patches: ${state.accumulatedTextPatches.length}`);

            // Trigger canvas redraw
            if (typeof canvasHandler !== 'undefined' && canvasHandler) {
                canvasHandler.animate();
            }
        }
    } catch (e) {
        console.error("Error adding text:", e);
        alert('Errore durante la generazione del testo.');
    }
}

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
            ui.statusText.style.color = "#666";
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

    // --- HOMING PATH: Add path from current robot position to origin (0,0) ---
    // This prevents the robot from "teleporting" when a new trajectory is started
    if (state.manipulator) {
        const currentQ0 = state.manipulator.q0;
        const currentQ1 = state.manipulator.q1;

        // Only proceed if we have valid joint angles (not null, undefined, or NaN)
        if (Number.isFinite(currentQ0) && Number.isFinite(currentQ1)) {
            // Only add homing path if robot is not already at origin
            if (Math.abs(currentQ0) > 0.01 || Math.abs(currentQ1) > 0.01) {
                // Calculate current position in world coordinates
                const L1 = state.settings.l1 || 0.17;
                const L2 = state.settings.l2 || 0.17;
                const currentX = L1 * Math.cos(currentQ0) + L2 * Math.cos(currentQ0 + currentQ1);
                const currentY = L1 * Math.sin(currentQ0) + L2 * Math.sin(currentQ0 + currentQ1);

                // Home position (q0=0, q1=0) = (L1+L2, 0)
                const homeX = L1 + L2;
                const homeY = 0;

                // Validate calculated positions are valid numbers
                if (Number.isFinite(currentX) && Number.isFinite(currentY) &&
                    Number.isFinite(homeX) && Number.isFinite(homeY)) {
                    // Add homing movement (pen up - no drawing)
                    payload.push({
                        'type': 'line',
                        'points': [[currentX, currentY], [homeX, homeY]],
                        'data': { 'penup': true }
                    });

                    console.log(`Homing path added: (${currentX.toFixed(3)}, ${currentY.toFixed(3)}) -> (${homeX.toFixed(3)}, ${homeY.toFixed(3)})`);
                }
            }
        }
    }

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
    ui.btnStar.classList.toggle('active', state.tool === TOOLS.STAR);
    ui.btnSemicircle.classList.toggle('active', state.tool === TOOLS.SEMICIRCLE);

    // New shapes
    if (ui.btnHeart) ui.btnHeart.classList.toggle('active', state.tool === TOOLS.HEART);
    if (ui.btnSpiral) ui.btnSpiral.classList.toggle('active', state.tool === TOOLS.SPIRAL);
    if (ui.btnCross) ui.btnCross.classList.toggle('active', state.tool === TOOLS.CROSS);
    if (ui.btnRhombus) ui.btnRhombus.classList.toggle('active', state.tool === TOOLS.RHOMBUS);
    if (ui.btnTriangle) ui.btnTriangle.classList.toggle('active', state.tool === TOOLS.TRIANGLE);
    if (ui.btnTriangleScalene) ui.btnTriangleScalene.classList.toggle('active', state.tool === TOOLS.TRIANGLE_SCALENE);
    if (ui.btnTriangleRight) ui.btnTriangleRight.classList.toggle('active', state.tool === TOOLS.TRIANGLE_RIGHT);
    if (ui.btnTriangleIsosceles) ui.btnTriangleIsosceles.classList.toggle('active', state.tool === TOOLS.TRIANGLE_ISOSCELES);
}

function updateUndoRedoUI() {
    ui.btnUndo.disabled = !state.canUndo();
    ui.btnRedo.disabled = !state.canRedo();
}

// Old updateSerialStatus removed (duplicate)
// setSerialUI removed (unused)

// --- API Callbacks ---

// API Callbacks moved to window.load initialization
// API.initCallbacks({ ... });

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



// --- Image Tool Logic ---

state.imagePreviewPatches = []; // Temporary storage for processed lines from backend

function setupImageEvents() {
    if (ui.btnAppModeImage) {
        ui.btnAppModeImage.addEventListener('click', () => setAppMode('image'));
    }

    if (ui.btnProcessImage) {
        ui.btnProcessImage.addEventListener('click', processImage);
    }

    if (ui.btnConfirmImage) {
        ui.btnConfirmImage.addEventListener('click', confirmImage);
    }

    // File Input
    if (ui.inputImageFile) {
        ui.inputImageFile.addEventListener('change', (e) => {
            console.log("Image file selected");
            const file = e.target.files[0];
            if (file) {
                console.log("File:", file.name, file.type);

                // Handle TIFF preview limitation
                if (file.name.toLowerCase().endsWith('.tif') || file.name.toLowerCase().endsWith('.tiff')) {
                    if (ui.imgPreviewContainer) {
                        // Show text instead of image
                        ui.imgPreviewImg.style.display = 'none';
                        // Create or update a message element
                        let msg = document.getElementById('preview-msg');
                        if (!msg) {
                            msg = document.createElement('span');
                            msg.id = 'preview-msg';
                            ui.imgPreviewContainer.appendChild(msg);
                        }
                        msg.textContent = "TIFF Preview not supported. Click PROCESS.";
                        msg.style.display = 'block';
                    }
                    return;
                }

                // Standard Image Preview
                const reader = new FileReader();
                reader.onload = (e) => {
                    console.log("File read complete");
                    if (ui.imgPreviewImg) {
                        ui.imgPreviewImg.src = e.target.result;
                        ui.imgPreviewImg.style.display = 'block';

                        // Hide message if exists
                        const msg = document.getElementById('preview-msg');
                        if (msg) msg.textContent = "Preview";
                    }
                };
                reader.readAsDataURL(file);
            }
        });
    }
}

async function processImage() {
    const fileInput = ui.inputImageFile;
    if (!fileInput || !fileInput.files || !fileInput.files[0]) {
        alert("Please select an image file first.");
        return;
    }

    // UI Elements
    // Use the debug log area instead of just loader
    const loader = document.getElementById('image-loading');
    const debugLog = document.getElementById('image-debug-log');
    const btnProcess = ui.btnProcessImage;

    // Show Loader
    if (loader) loader.classList.remove('hidden');

    if (debugLog) {
        debugLog.classList.remove('hidden');
        debugLog.innerHTML = `<div style="color: var(--accent-color)">> Starting processing...</div>`;
    }

    // Also use button text as loader
    if (btnProcess) {
        btnProcess.setAttribute('data-original-text', btnProcess.textContent);
        btnProcess.textContent = "Processing...";
        btnProcess.disabled = true;
    }

    const file = fileInput.files[0];

    // Check mode
    const isSvg = file.name.toLowerCase().endsWith('.svg');
    const options = {
        mode: isSvg ? 'svg' : 'raster',
        width: parseFloat(ui.inputImgWidth.value) || 0.10,
        x: parseFloat(ui.inputImgX.value) || 0.20,
        y: parseFloat(ui.inputImgY.value) || 0.00,
        rotation: parseFloat(ui.inputImgRotation.value) || 0,
        threshold: parseFloat(ui.inputImgThreshold ? ui.inputImgThreshold.value : 100) || 100
    };

    try {
        let base64Data = "";

        if (isSvg) {
            // Read SVG normally
            base64Data = await new Promise((resolve) => {
                const r = new FileReader();
                r.onload = (e) => resolve(e.target.result);
                r.readAsDataURL(file);
            });
        } else {
            // Resize Raster Images
            if (debugLog) debugLog.innerHTML += `<div>> Resizing image to 1024px...</div>`;
            try {
                base64Data = await resizeImage(file, 1024); // Resize to max 1024px
            } catch (err) {
                console.error("Resize failed", err);
                if (debugLog) debugLog.innerHTML += `<div style="color:red">> Resize failed. Using original.</div>`;
                // Fallback
                base64Data = await new Promise((resolve) => {
                    const r = new FileReader();
                    r.onload = (e) => resolve(e.target.result);
                    r.readAsDataURL(file);
                });
            }
        }

        if (debugLog) debugLog.innerHTML += `<div>> Params: ${options.width}m, Thresh: ${options.threshold}</div>`;
        console.log("Processing Image...", options);
        API.log("Processing Image...");

        // Call Backend with Neutral Parameters for Client-Side Transform
        // We request width=1.0, x=0, y=0, rot=0 to get normalized patches
        const neutralOptions = { ...options, width: 1.0, x: 0, y: 0, rotation: 0 };

        const t0 = performance.now();
        const patches = await window.eel.py_process_image(base64Data, neutralOptions)();
        const t1 = performance.now();

        const timeSeconds = ((t1 - t0) / 1000).toFixed(2);
        if (debugLog) debugLog.innerHTML += `<div style="color: lightgreen">> Done in ${timeSeconds}s.</div>`;
        if (debugLog) debugLog.innerHTML += `<div>> Found ${patches.length} contours.</div>`;

        console.log(`Received ${patches.length} patches.`);
        API.log(`Image Processed: ${patches.length} segments in ${timeSeconds}s.`);

        // Store RAW patches (Normalized)
        state.rawImagePatches = patches;

        // Initial Update
        updateImagePreviewTransforms();

        if (patches.length === 0) {
            if (debugLog) debugLog.innerHTML += `<div style="color: var(--danger-color)">> No paths found.</div>`;
            alert("No contours found! Try lowering the Threshold.");
        } else {
            if (canvasHandler) {
                canvasHandler.animate(); // Force redraw
            }
        }

        if (patches.length === 0) {
            if (debugLog) debugLog.innerHTML += `<div style="color: var(--danger-color)">> No paths found.</div>`;
            alert("No contours found! Try lowering the Threshold.");
        } else {
            if (canvasHandler) {
                canvasHandler.animate(); // Force redraw
                // Optional: auto-move viewport if needed, but for now just static
            }
        }

    } catch (e) {
        console.error("Image Processing Failed:", e);
        if (debugLog) debugLog.innerHTML += `<div style="color: var(--danger-color)">> ERROR: ${e}</div>`;
        alert("Error processing image: " + e);
    } finally {
        // Hide Loader
        if (loader) loader.classList.add('hidden');

        // Reset Button
        if (btnProcess) {
            btnProcess.textContent = btnProcess.getAttribute('data-original-text') || "PROCESS";
            btnProcess.disabled = false;
        }
    }
}

function confirmImage() {
    if (!state.imagePreviewPatches || state.imagePreviewPatches.length === 0) {
        alert("No image processed to confirm.");
        return;
    }

    // Commit to Main Trajectory
    // Convert patches (dicts) to Lines in Trajectory
    const settings = state.settings;

    let count = 0;
    // We treat image as a series of connected or disconnected lines.
    // The patches are usually continuous contours.

    state.imagePreviewPatches.forEach(patch => {
        // patch.points is [[x,y], [x,y], ...]
        // patch.type is 'line' (mostly)

        const pts = patch.points;
        if (pts.length < 2) return;

        // Move to start of patch (Jump)
        // patch.points are in Absolute Meters inside the Workspace
        // We create Points with ACT values.

        for (let i = 0; i < pts.length - 1; i++) {
            const p1 = new Point(0, 0, settings);
            p1.actX = pts[i][0];
            p1.actY = pts[i][1];

            const p2 = new Point(0, 0, settings);
            p2.actX = pts[i + 1][0];
            p2.actY = pts[i + 1][1];

            // First segment of a patch should be a Jump if not continuous with previous
            // But Trajectory class handles additions.
            // If we just add lines, they are segments.
            // We need to know if we PenUp to p1.
            // Logic:
            // If i==0, this is start of a contour. We should PenUp to it.
            // But wait, add_line(p1, p2, isJump) ?
            // add_line(start, end, penUp)
            // Check state.points. If empty, just start.

            const isStartOfContour = (i === 0);
            let penUp = false;

            // If it's the very first point of the contour, we need to jump to it?
            // Not exactly. add_line adds a segment from p1 to p2.
            // Does it imply we are AT p1?
            // If state.points is not empty, we are at last point.
            // So if i=0, we need a Jump from last point to p1.

            if (isStartOfContour) {
                if (state.points.length > 0) {
                    const last = state.points[state.points.length - 1];
                    // Jump line: Last -> P1
                    state.trajectory.add_line(last, p1, true);
                }
                state.points.push(p1);
            }

            // Segment: P1 -> P2 (PenDown)
            state.trajectory.add_line(p1, p2, false);
            state.points.push(p2);
            count++;
        }
    });

    console.log(`Added ${count} lines from image.`);
    API.log(`Image Added: ${count} segments.`);

    // Clear Preview
    state.imagePreviewPatches = [];

    // Save State for Undo
    state.saveState();
    updateUndoRedoUI();

    if (canvasHandler) canvasHandler.animate();

    // Switch to Drawing Mode to see results
    setAppMode('drawing');
    alert("Image trajectory added to workspace!");
}

// Helper to resize image client-side to prevent large payload websocket errors
function resizeImage(file, maxWidth) {
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.onload = (e) => {
            const img = new Image();
            img.onload = () => {
                const canvas = document.createElement('canvas');
                let width = img.width;
                let height = img.height;

                if (width > maxWidth) {
                    height *= maxWidth / width;
                    width = maxWidth;
                }

                canvas.width = width;
                canvas.height = height;
                const ctx = canvas.getContext('2d');
                ctx.drawImage(img, 0, 0, width, height);
                // Return compressed JPEG data URL
                resolve(canvas.toDataURL('image/jpeg', 0.8));
            };
            img.onerror = reject;
            img.src = e.target.result;
        };
        reader.onerror = reject;
        reader.readAsDataURL(file);
    });
}

function updateImagePreviewTransforms() {
    if (!state.rawImagePatches || state.rawImagePatches.length === 0) return;

    // Read UI Params
    const width = parseFloat(ui.inputImgWidth.value) || 0.10;
    const offX = parseFloat(ui.inputImgX.value) || 0.20;
    const offY = parseFloat(ui.inputImgY.value) || 0.00; // Y is UP
    const rotDeg = parseFloat(ui.inputImgRotation.value) || 0;
    const rotRad = (rotDeg * Math.PI) / 180;

    // Apply Transform to each point
    // Original Backend Logic:
    // Scale: width (since raw is normalized to 1)
    // Rotate: around (0,0) center of shape
    // Translate: +offX, +offY 
    // Backend returns raw points centered at (width/2, height/2) usually?
    // Wait, we asked backend to normalize with Width=1.0.
    // Backend logic:
    // scale = width_m / orig_w
    // center_x = (min_x + max_x) / 2
    // tx = (px - center_x) * scale [Zero Center X]
    // ty = - (py - center_y) * scale [Zero Center Y, Flip Y]

    // So RAW data from backend (with Width=1, X=0, Y=0, Rot=0) is:
    // - Centered at (0,0)
    // - Width approx 1.0 (meters)
    // - Y flipped (Robot Coordinates)

    // We just need to Scale (again?), Rotate, Translate.
    // Wait, backend scaled it to 1.0m. We want target Width.
    // So we apply factor: UserWidth / 1.0 = UserWidth.

    const scale = width;
    const matches = [];

    state.rawImagePatches.forEach(patch => {
        const transformedPoints = [];
        patch.points.forEach(pt => {
            const x0 = pt[0];
            const y0 = pt[1];

            // 1. Scale
            const x1 = x0 * scale;
            const y1 = y0 * scale;

            // 2. Rotate
            const x2 = x1 * Math.cos(rotRad) - y1 * Math.sin(rotRad);
            const y2 = x1 * Math.sin(rotRad) + y1 * Math.cos(rotRad);

            // 3. Translate
            const x3 = x2 + offX;
            const y3 = y2 + offY;

            transformedPoints.push([x3, y3]);
        });

        matches.push({
            type: patch.type,
            points: transformedPoints,
            data: patch.data
        });
    });

    state.imagePreviewPatches = matches;
    if (canvasHandler) canvasHandler.animate();
}

// Attach Listeners for Realtime Update
[ui.inputImgWidth, ui.inputImgX, ui.inputImgY, ui.inputImgRotation].forEach(input => {
    if (input) {
        input.addEventListener('input', updateImagePreviewTransforms);
    }
});
