import { Point, find_circ } from './utils.js';
import { appState, TOOLS } from './state.js';
import { calculateRectangle, calculatePolygon, calculateStar, snapPointToGrid } from './utils_drawing.js';

export class CanvasHandler {
    constructor(canvas, state) {
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');
        this.state = state; // App State Singleton

        this.mouseX = 0;
        this.mouseY = 0;
        this.hoveredPoint = null; // Track point near cursor for connection
        this.snapRadius = 15; // Pixels distance for point snapping

        this.resize(); // Handle HiDPI scaling
        this.initEvents();
    }

    initEvents() {
        // Use arrow function to bind 'this'
        this.canvas.addEventListener('click', (e) => this.handleClick(e));
        this.canvas.addEventListener('mousemove', (e) => this.handleMouseMove(e));

        // Listen for resize
        window.addEventListener('resize', () => this.resize());

        // Start Loop
        this.animate();
    }

    resize() {
        const parent = this.canvas.parentElement;
        const width = parent.clientWidth;
        const height = parent.clientHeight;

        // Canvas element size (fit container with small margin)
        const size = Math.min(width, height) - 20;

        const dpr = window.devicePixelRatio || 1;

        // Check if size actually changed to avoid unnecessary updates
        const oldSize = parseFloat(this.canvas.style.width) || size;
        const scaleRatio = size / oldSize;

        this.canvas.width = size * dpr;
        this.canvas.height = size * dpr;

        this.ctx.scale(dpr, dpr);

        this.canvas.style.width = size + 'px';
        this.canvas.style.height = size + 'px';

        // Internal Padding Logic
        const padding = 40; // Internal padding in pixels
        const usableSize = size - (padding * 2);
        const radius = usableSize / 2;

        // Update Origin to center
        this.state.settings.origin.x = size / 2;
        this.state.settings.origin.y = size / 2;

        // Scale: Robot diameter (0.328 * 2) -> Usable Diameter
        this.state.settings.m_p = (0.328 * 2) / usableSize;

        // Store visual radius for drawing
        this.workspaceRadius = radius;

        // --- Recalculate Relative Coordinates for consistency ---
        this.updateCoordinates(scaleRatio);
    }

    updateCoordinates(scaleRatio) {
        // Update simple points
        if (this.state.points) {
            this.state.points.forEach(p => p.updateRelative());
        }
        if (this.state.sentPoints) {
            this.state.sentPoints.forEach(p => p.updateRelative());
        }

        // Helper to update trajectory data
        const updateTraj = (traj) => {
            if (!traj || !traj.data) return;
            traj.data.forEach(item => {
                if (item.type === 'line') {
                    // [p0, p1, raised]
                    item.data[0].updateRelative();
                    item.data[1].updateRelative();
                } else if (item.type === 'circle') {
                    // [c, r, theta0, theta1, raised, a, p]
                    item.data[0].updateRelative(); // Center
                    item.data[1] *= scaleRatio;     // Radius (scalar pixel value)
                    if (item.data[5]) item.data[5].updateRelative(); // a
                    if (item.data[6]) item.data[6].updateRelative(); // p
                }
            });
        };

        if (this.state.trajectory) updateTraj(this.state.trajectory);
        if (this.state.sentTrajectory) updateTraj(this.state.sentTrajectory);

        // Also update shape start if drawing
        if (this.state.shapeStart) this.state.shapeStart.updateRelative();

        // Force redraw
        if (this.state.appMode === 'text') {
            // For text, just regenerate preview if needed (it uses raw coords mostly)
            // But if we have cached patches with Points...
        }
    }

    handleMouseMove(e) {
        const rect = this.canvas.getBoundingClientRect();
        this.mouseX = e.clientX - rect.left;
        this.mouseY = e.clientY - rect.top;

        // Update hovered point for connection feedback (All modes now)
        if (this.state.appMode === 'drawing') {
            this.hoveredPoint = this.findNearbyPoint(this.mouseX, this.mouseY);
        } else {
            this.hoveredPoint = null;
        }
    }

    findNearbyPoint(x, y) {
        const points = this.state.points;
        for (let i = points.length - 1; i >= 0; i--) {
            const p = points[i];
            const dx = x - p.relX;
            const dy = y - p.relY;
            if (dx * dx + dy * dy < this.snapRadius * this.snapRadius) {
                return p;
            }
        }
        return null;
    }

    handleClick(e) {
        let x = this.mouseX;
        let y = this.mouseY;
        const settings = this.state.settings;
        // Anchor Snapping (Priority in United Mode)
        let snapped = false;
        if (this.state.drawingMode === 'continuous' && this.state.points.length > 0) {
            for (let p of this.state.points) {
                const dx = x - p.relX;
                const dy = y - p.relY;
                if (dx * dx + dy * dy < 144) { // 12px snap radius
                    x = p.relX;
                    y = p.relY;
                    snapped = true;
                    break;
                }
            }
        }

        // Apply grid snapping (if not snapped to anchor)
        if (!snapped && this.state.snapToGrid) {
            const snappedPoint = snapPointToGrid(new Point(x, y, settings), this.state.gridSize, settings);
            x = snappedPoint.relX;
            y = snappedPoint.relY;
        }

        const distSq = Math.pow(x - settings.origin.x, 2) + Math.pow(y - settings.origin.y, 2);
        const maxRadius = this.workspaceRadius || (this.canvas.height / 2);

        if (distSq > maxRadius * maxRadius) return;

        // Strict Mode Check
        if (this.state.appMode !== 'drawing') return;

        if (this.state.points.length === 0 && this.state.sentPoints.length > 0) {
            this.state.sentPoints = [];
            this.state.sentTrajectory.reset();
        }

        const currentTool = this.state.tool;
        const currentPoint = new Point(x, y, settings);
        const mode = this.state.drawingMode; // 'continuous' or 'discrete'

        if (currentTool === TOOLS.LINE) {
            if (mode === 'discrete') {
                // Discrete Line: Start -> End (Independent Segments)
                // Check if we should snap to an existing point
                const nearbyPoint = this.findNearbyPoint(x, y);
                const targetPoint = nearbyPoint ? nearbyPoint : currentPoint;

                if (!this.state.shapeStart) {
                    // Click 1: Start Point
                    // Jump from previous if exists and not snapping to it
                    if (this.state.points.length > 0 && !nearbyPoint) {
                        const last = this.state.points[this.state.points.length - 1];
                        this.state.trajectory.add_line(last, targetPoint, true);
                    }
                    this.state.shapeStart = targetPoint;
                    // Only add point if not snapping to existing
                    if (!nearbyPoint) {
                        this.state.points.push(targetPoint);
                    }
                } else {
                    // Click 2: End Point
                    const start = this.state.shapeStart;
                    this.state.trajectory.add_line(start, targetPoint, false); // Connected = pen down
                    // Only add point if not snapping to existing
                    if (!nearbyPoint) {
                        this.state.points.push(targetPoint);
                    }
                    this.state.shapeStart = null;
                    this.state.saveState();
                }
            } else {
                // Continuous Line: Polyline
                this.state.points.push(currentPoint);
                if (this.state.points.length > 1) {
                    const p0 = this.state.points[this.state.points.length - 2];
                    this.state.trajectory.add_line(p0, currentPoint, this.state.penUp);
                    this.state.saveState();
                }
            }

        } else if (currentTool === TOOLS.SEMICIRCLE) {

            // Auto-Start logic only in Continuous mode
            let autoStart = (mode === 'continuous' && this.state.points.length > 0);

            if (!this.state.semicircleStart) {
                if (autoStart) {
                    // Auto-chain: Last point is Start. Current click is END.
                    this.state.semicircleStart = this.state.points[this.state.points.length - 1];
                    // Proceed to process 'End' (Current Point) below
                } else {
                    // Manual Start (Discrete or First Point)
                    if (this.state.points.length > 0) {
                        const last = this.state.points[this.state.points.length - 1];
                        this.state.trajectory.add_line(last, currentPoint, true);
                    }
                    this.state.semicircleStart = currentPoint;
                    this.state.points.push(currentPoint);
                    return; // Wait for second click
                }
            }

            // If we are here, semicircleStart is set (either manually or auto)
            const start = this.state.semicircleStart;
            const end = currentPoint;

            const cx = (start.relX + end.relX) / 2;
            const cy = (start.relY + end.relY) / 2;
            const center = new Point(cx, cy, settings);

            const dx = end.relX - start.relX;
            const dy = end.relY - start.relY;
            const radius = Math.sqrt(dx * dx + dy * dy) / 2;

            const startAngle = Math.atan2(start.relY - cy, start.relX - cx);
            const endAngle = Math.atan2(end.relY - cy, end.relX - cx);

            this.state.trajectory.add_circle(
                center, radius, startAngle, endAngle,
                this.state.penUp, start, end
            );

            this.state.points.push(end);
            this.state.semicircleStart = null;
            this.state.saveState();

        } else if ([TOOLS.CIRCLE, TOOLS.SQUARE, TOOLS.POLYGON, TOOLS.STAR].includes(currentTool)) {
            // These tools are always Center-Based (Discrete Interaction)
            // But we must handle the 'Connect' vs 'Jump' logic

            if (!this.state.shapeStart) {
                // Center Click
                if (this.state.points.length > 0) {
                    const last = this.state.points[this.state.points.length - 1];

                    // Unified Logic: 
                    // Continuous (United) -> Connect (Pen Down = false)
                    // Discrete (Separated) -> Jump (Pen Up = true)
                    const isJump = (this.state.drawingMode === 'discrete');

                    this.state.trajectory.add_line(last, currentPoint, isJump);
                }
                this.state.shapeStart = currentPoint;
                this.state.points.push(currentPoint);

            } else {
                // Size/Rotation Click
                const center = this.state.shapeStart;
                const corner = currentPoint;

                const dx = corner.relX - center.relX;
                const dy = corner.relY - center.relY;
                const radius = Math.sqrt(dx * dx + dy * dy);
                const rotation = Math.atan2(dy, dx);

                if (currentTool === TOOLS.CIRCLE) {
                    const startAngle = rotation;
                    const endAngle = rotation + 2 * Math.PI;

                    const startP = new Point(
                        center.relX + radius * Math.cos(startAngle),
                        center.relY + radius * Math.sin(startAngle),
                        settings
                    );

                    // Move Center -> StartP (Pen Up)
                    this.state.trajectory.add_line(center, startP, true);

                    this.state.trajectory.add_circle(
                        center, radius, startAngle, endAngle,
                        this.state.penUp, startP, startP
                    );

                    this.state.points.push(startP);

                } else {
                    let shapePoints = [];

                    if (currentTool === TOOLS.STAR) {
                        // Default 5 points, inner radius 0.4
                        shapePoints = calculateStar(center, radius, 5, 0.4, rotation, settings);
                    } else {
                        const sides = (currentTool === TOOLS.SQUARE) ? 4 : (this.state.polygonSides || 5);
                        shapePoints = calculatePolygon(center, radius, sides, rotation, settings);
                    }

                    // Move Center -> Vertex 0
                    this.state.trajectory.add_line(center, shapePoints[0], true);

                    for (let i = 0; i < shapePoints.length; i++) {
                        const p_start = shapePoints[i];
                        const p_end = shapePoints[(i + 1) % shapePoints.length];
                        this.state.trajectory.add_line(p_start, p_end, this.state.penUp);
                    }

                    this.state.points.push(shapePoints[0]);
                }

                this.state.shapeStart = null;
                this.state.saveState();
            }
        }

        if (this.state.manipulator) this.state.manipulator.reset_trace();
    }

    drawBackground() {
        const width = parseFloat(this.canvas.style.width);
        const height = parseFloat(this.canvas.style.height);

        const ctx = this.ctx;
        const origin = this.state.settings.origin;

        // Clear
        ctx.clearRect(0, 0, width, height);

        // --- Workspace Limits ---
        const radius = this.workspaceRadius || (height / 2);

        // Mode-Specific Background
        const mode = this.state.textMode || 'linear'; // default

        // Common Border Style
        ctx.lineWidth = 2.5; // REQUESTED: 2.5px

        if (mode === 'linear') {
            // Linear Workspace (Green Rectangle)
            // Draw Full Reachable (Faint)
            const reachColor = getComputedStyle(document.documentElement).getPropertyValue('--reach-circle-color').trim() || '#333';
            ctx.beginPath();
            ctx.arc(origin.x, origin.y, radius, 0, 2 * Math.PI);
            ctx.strokeStyle = reachColor;
            ctx.lineWidth = 3.5; // Thicker reach circle
            ctx.stroke();

            // Green Rect
            ctx.fillStyle = 'rgba(0, 200, 81, 0.3)'; // Green transparent
            ctx.strokeStyle = '#00c851';
            ctx.lineWidth = 2.5; // REQUESTED: 2.5px

            // Get from State
            const ws = this.state.settings.linearWorkspace || { x: 0.15, y: -0.15, w: 0.20, h: 0.30 };
            const mp = this.state.settings.m_p;

            const x1 = ws.x / mp;
            // y is bottom-left relative to origin (Cartesian)
            // Canvas Y = origin.y - (y + h) (Top-Left)
            const w_pix = ws.w / mp;
            const h_pix = ws.h / mp;

            const rx = origin.x + x1;
            const ry = origin.y - (ws.y + ws.h) / mp;

            ctx.fillRect(rx, ry, w_pix, h_pix);
            ctx.strokeRect(rx, ry, w_pix, h_pix);

        } else {
            // Curved Workspace (Green Donut Sector)
            ctx.beginPath();
            // Inner/outer radius from state (dynamic)
            const cws = this.state.settings.curvedWorkspace || { innerRadius: 0.10, outerRadius: 0.30 };
            const innerR_m = cws.innerRadius;
            const outerR_m = cws.outerRadius;

            const mp = this.state.settings.m_p;
            const innerR = innerR_m / mp;
            const outerR = outerR_m / mp;

            // Right side sector (-90 to +90 degrees)
            ctx.arc(origin.x, origin.y, outerR, -Math.PI / 2, Math.PI / 2, false);
            ctx.arc(origin.x, origin.y, innerR, Math.PI / 2, -Math.PI / 2, true); // inner reversed
            ctx.closePath();

            ctx.fillStyle = 'rgba(0, 200, 81, 0.3)'; // Green
            ctx.fill();
            ctx.strokeStyle = '#00c851';
            ctx.lineWidth = 2.5; // REQUESTED: 2.5px
            ctx.stroke();
        }

        // Axis Lines
        ctx.beginPath();

        // Dynamic Axis Color
        const axisColor = getComputedStyle(document.documentElement).getPropertyValue('--axis-color').trim() || '#404040';
        ctx.strokeStyle = axisColor;
        ctx.lineWidth = 3.0; // REQUESTED: 3.0px

        ctx.moveTo(0, origin.y);
        ctx.lineTo(width, origin.y);
        ctx.moveTo(origin.x, 0);
        ctx.lineTo(origin.x, height);
        ctx.stroke();
    }

    drawPoint(p, color = '#00e5ff') { // Default to accent color (cyan) for visibility
        this.ctx.beginPath();
        this.ctx.fillStyle = color;
        this.ctx.arc(p.relX, p.relY, 4, 0, 2 * Math.PI);
        this.ctx.fill();
    }

    drawGrid() {
        if (!this.state.showGrid) return;

        const ctx = this.ctx;
        const width = parseFloat(this.canvas.style.width);
        const height = parseFloat(this.canvas.style.height);
        const gridSize = this.state.gridSize;

        ctx.strokeStyle = 'rgba(128, 128, 128, 0.4)'; // Gray for all themes
        ctx.lineWidth = 1;

        // Vertical lines
        for (let x = 0; x < width; x += gridSize) {
            ctx.beginPath();
            ctx.moveTo(x, 0);
            ctx.lineTo(x, height);
            ctx.stroke();
        }

        // Horizontal lines
        for (let y = 0; y < height; y += gridSize) {
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(width, y);
            ctx.stroke();
        }
    }

    drawCoordinates() {
        const ctx = this.ctx;
        const settings = this.state.settings;

        // Convert mouse position to world coordinates
        const worldX = (this.mouseX - settings.origin.x) * settings.m_p;
        const worldY = -(this.mouseY - settings.origin.y) * settings.m_p; // Y is flipped

        // Draw background box
        const padding = 10;
        const boxX = padding;
        const boxY = padding;
        const text = `X: ${worldX.toFixed(3)}m  Y: ${worldY.toFixed(3)}m`;

        ctx.font = '14px monospace';
        const metrics = ctx.measureText(text);
        const boxWidth = metrics.width + 20;
        const boxHeight = 30;

        ctx.fillStyle = 'rgba(0, 0, 0, 0.7)';
        ctx.fillRect(boxX, boxY, boxWidth, boxHeight);

        ctx.strokeStyle = '#00e5ff';
        ctx.lineWidth = 1;
        ctx.strokeRect(boxX, boxY, boxWidth, boxHeight);

        // Draw text
        ctx.fillStyle = '#00e5ff';
        ctx.fillText(text, boxX + 10, boxY + 20);
    }

    drawToolPreview() {
        const ctx = this.ctx;
        const settings = this.state.settings;
        const points = this.state.points;
        const mouseX = this.mouseX;
        const mouseY = this.mouseY;

        // --- TEXT MODE PREVIEW ---
        // (Text preview is now handled by drawTextPreview directly in animate loop)
        // We only handle active tool previews here (Ghost lines, shapes, etc.)

        // --- DRAWING MODE PREVIEW ---

        if (this.state.appMode === 'drawing') {
            if (points.length === 0 && !this.state.semicircleStart && !this.state.shapeStart) return;

            let lastP = null;
            if (points.length > 0) lastP = points[points.length - 1];

            const currentTool = this.state.tool;

            if (currentTool === TOOLS.LINE && lastP) {
                ctx.beginPath();
                ctx.strokeStyle = 'rgba(0, 0, 0, 0.5)';
                ctx.setLineDash([5, 5]);
                ctx.lineWidth = 2;
                ctx.moveTo(lastP.relX, lastP.relY);
                ctx.lineTo(mouseX, mouseY);
                ctx.stroke();
                ctx.setLineDash([]);
            }
            else if (currentTool === TOOLS.SEMICIRCLE) {
                let start = this.state.semicircleStart;
                // Infer start in continuous mode
                if (!start && this.state.drawingMode === 'continuous' && points.length > 0) {
                    start = points[points.length - 1];
                }

                if (start) {
                    // Preview Arc
                    const cx = (start.relX + mouseX) / 2;
                    const cy = (start.relY + mouseY) / 2;
                    const dx = mouseX - start.relX;
                    const dy = mouseY - start.relY;
                    const r = Math.sqrt(dx * dx + dy * dy) / 2;
                    const a1 = Math.atan2(start.relY - cy, start.relX - cx);
                    const a2 = Math.atan2(mouseY - cy, mouseX - cx);

                    // Use same CCW logic as trajectory drawing
                    const A = a1 > a2;
                    const B = Math.abs(a2 - a1) < Math.PI;
                    const ccw = (!A && !B) || (A && B);

                    ctx.beginPath();
                    ctx.strokeStyle = 'rgba(0, 0, 0, 0.7)'; // Black
                    ctx.setLineDash([5, 5]);
                    ctx.lineWidth = 2;
                    ctx.arc(cx, cy, r, a1, a2, ccw);
                    ctx.stroke();
                    ctx.setLineDash([]);
                }
            }
            else if (this.state.shapeStart && [TOOLS.CIRCLE, TOOLS.SQUARE, TOOLS.POLYGON, TOOLS.STAR].includes(currentTool)) {
                const center = this.state.shapeStart;
                const dx = mouseX - center.relX;
                const dy = mouseY - center.relY;
                const r = Math.sqrt(dx * dx + dy * dy);
                const rot = Math.atan2(dy, dx);

                ctx.beginPath();
                ctx.strokeStyle = 'rgba(0, 0, 0, 0.7)'; // Black
                ctx.setLineDash([5, 5]);
                ctx.lineWidth = 4;

                if (currentTool === TOOLS.CIRCLE) {
                    ctx.arc(center.relX, center.relY, r, 0, 2 * Math.PI);
                } else {
                    let pts = [];
                    if (currentTool === TOOLS.STAR) {
                        pts = calculateStar(center, r, 5, 0.4, rot, settings);
                    } else {
                        const sides = (currentTool === TOOLS.SQUARE) ? 4 : (this.state.polygonSides || 5);
                        pts = calculatePolygon(center, r, sides, rot, settings);
                    }

                    if (pts.length > 0) {
                        ctx.moveTo(pts[0].relX, pts[0].relY);
                        for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i].relX, pts[i].relY);
                        ctx.closePath();
                    }
                }
                ctx.stroke();
                ctx.setLineDash([]);

                // Draw Center
                ctx.beginPath();
                ctx.fillStyle = 'rgba(0, 229, 255, 0.5)'; // Cyan
                ctx.arc(center.relX, center.relY, 3, 0, 2 * Math.PI);
                ctx.fill();
            }
        }
    }

    drawTextPreview() {
        const ctx = this.ctx;
        // Draw Text Preview if it exists (regardless of mode)
        if (this.state.textPreview && this.state.textPreview.length > 0) {
            const mp = this.state.settings.m_p;
            const origin = this.state.settings.origin;

            ctx.lineWidth = 2;

            for (let patch of this.state.textPreview) {
                if (patch.type === 'line') {
                    const p0 = patch.points[0];
                    const p1 = patch.points[1];

                    // Convert world meters to canvas pixels
                    const x0 = origin.x + p0[0] / mp;
                    const y0 = origin.y - p0[1] / mp;
                    const x1 = origin.x + p1[0] / mp;
                    const y1 = origin.y - p1[1] / mp;

                    ctx.beginPath();
                    ctx.moveTo(x0, y0);
                    ctx.lineTo(x1, y1);
                    if (patch.data.penup) {
                        ctx.strokeStyle = 'rgba(255, 165, 0, 0.3)'; // Fainter orange for jumps in mixed mode
                        ctx.setLineDash([5, 5]);
                        ctx.lineWidth = 1.0;
                    } else {
                        // Use a distinct color for Text vs Drawing? Or same?
                        // Black is good.
                        ctx.strokeStyle = '#000000';
                        ctx.setLineDash([]);
                        ctx.lineWidth = 2.0;
                    }
                    ctx.stroke();
                }
            }
            ctx.setLineDash([]);
        }
    }

    drawImportPreview() {
        if (!this.state.importPreview || this.state.importPreview.length === 0) return;

        const ctx = this.ctx;
        const mp = this.state.settings.m_p;
        const origin = this.state.settings.origin;

        ctx.lineWidth = 2; // Preview width
        // Yellow color for import preview
        ctx.strokeStyle = '#ffff00';

        for (let item of this.state.importPreview) {
            // Each item has points in raw coordinates already transformed by UI logic?
            // Main.js transforms them: p[0]*scale + offX.
            // These are Cartesian (meters).
            // We need to convert to Canvas Pixels.

            if (item.type === 'line') {
                // points: [[x,y], [x,y]]
                const p0 = item.points[0];
                const p1 = item.points[1];

                const x0 = origin.x + p0[0] / mp;
                const y0 = origin.y - p0[1] / mp;
                const x1 = origin.x + p1[0] / mp;
                const y1 = origin.y - p1[1] / mp;

                ctx.beginPath();
                ctx.moveTo(x0, y0);
                ctx.lineTo(x1, y1);
                ctx.stroke();

            } else if (item.type === 'circle') {
                // points: [start, end]
                // data: {center: [x,y], radius: r, angle: delta...}

                const d = item.data;
                const c = d.center;
                const r = d.radius;

                const cx = origin.x + c[0] / mp;
                const cy = origin.y - c[1] / mp;
                const cr = r / mp;

                // Canvas Angle is reversed Y?
                // DXF angles are CCW from X-axis.
                // Canvas X is right, Y is down.
                // Correct conversion: theta_canvas = -theta_dxf?
                // Let's rely on item.points[0] (Start) to infer start angle.

                const pStart = item.points[0];
                // Calculate angles on Canvas
                // Canvas coords of start point
                const sx = origin.x + pStart[0] / mp;
                const sy = origin.y - pStart[1] / mp;

                // atan2(y - cy, x - cx)
                let a1 = Math.atan2(sy - cy, sx - cx);

                // If full circle (same points and large angle)
                // But wait, angle is delta.
                // End angle?
                // angle > 0 (CCW in cartesian).
                // In canvas (Y down), CCW is actually reversed direction visually if we use cartesian angles directly?
                // Let's assume standard behavior:
                // draw_circle needs start/end in canvas frame.
                // If delta is +PI/2 (CCW), in Canvas Y-Flip it becomes CW?
                // Yes. +Y is Down.

                // Actually `trajectory.add_circle` deals with this.
                // Here we just want a visual preview.
                // Let's assume CCW in Cartesian = CW in Canvas (since Y is flipped).
                // So end angle = start_angle - delta

                const delta = d.angle; // Radians
                // DXF Arc is CCW.
                // Canvas Arc with ACW=false is standard clockwise usually?
                // context.arc(x, y, radius, startAngle, endAngle, counterclockwise)
                // default ccw=false (DRAW CLOCKWISE).
                // Cartesian CCW -> Canvas Inverse.
                // Let's draw counter-clockwise: true.
                // Wait, +Angle in Canvas is Clockwise (from 3 o'clock towards 6 o'clock).
                // Cartesian +Angle is Counter-Clockwise (from 3 o'clock towards 12 o'clock).
                // So Cartesian CCW 0->90 is Canvas 0->-90 (or 0->270).
                // So: start_angle_canvas = -start_angle_cartesian.
                // end_angle_canvas = -end_angle_cartesian.
                // And draw with counterclockwise = true (which means decreasing angle i.e. going "up" which is "CCW" visually on screen?)
                // Actually, let's just use the points.

                let a2 = a1 - delta; // In canvas frame, positive delta cartesian is negative delta canvas (?)

                ctx.beginPath();
                ctx.arc(cx, cy, cr, a1, a2, true); // true = CounterClockwise in Canvas? 
                // (CCW in canvas = decreasing angle = moving up from right = Cartesian CCW). 
                // So yes, true. And a2 = a1 - delta ? No.
                // Cartesian 0 -> +90 (CCW).
                // Canvas 0 -> +90 is CW (Down).
                // Canvas 0 -> -90 is CCW (Up).
                // So Cartesian Theta -> Canvas -Theta.
                // correct.

                ctx.stroke();
            }
        }
    }

    animate() {
        const ctx = this.ctx;
        const width = parseFloat(this.canvas.style.width); // Use scaled size
        const height = parseFloat(this.canvas.style.height);

        // Clear entire canvas for redraw
        ctx.clearRect(0, 0, width, height);

        this.drawBackground();

        // Draw Grid (if enabled)
        this.drawGrid();

        // Draw Sent Data (Ghost)
        if (this.state.sentTrajectory) this.state.sentTrajectory.draw(ctx);
        for (let p of this.state.sentPoints) this.drawPoint(p, '#666666');

        // Draw Import Preview (Yellow Ghost)
        this.drawImportPreview();

        // Draw Active Stamp (Magenta Ghost)
        this.drawActiveStamp();

        // Draw Current Data

        // Draw Current Data
        if (this.state.trajectory) this.state.trajectory.draw(ctx);
        for (let p of this.state.points) this.drawPoint(p);

        // Draw Manipulator
        if (this.state.manipulator) {
            this.state.manipulator.draw_pose(ctx);
            // this.state.manipulator.draw_traces(ctx); // Performance heavy
        }

        // Text Preview (Persistent Layer)
        this.drawTextPreview();

        // Tool Preview (Active Interaction)
        this.drawToolPreview();

        // Text Preview (Generated Patches) - Redundant, now handled by drawToolPreview
        // if (this.state.textPreview && this.state.textPreview.length > 0) { ... }


        // Draw Anchors in both modes (but with different highlighting in Separated)
        this.drawAnchors();

        // Draw Coordinates (always on top)
        this.drawCoordinates();

        requestAnimationFrame(() => this.animate());
    }

    drawAnchors() {
        const ctx = this.ctx;
        const points = this.state.points;
        const radius = 4;

        for (let p of points) {
            const isHovered = this.hoveredPoint === p;

            ctx.beginPath();

            if (isHovered) {
                // Draw SQUARE for hoverable point (connection available)
                ctx.fillStyle = '#ffaa00'; // Orange/Yellow
                ctx.strokeStyle = '#fff';
                ctx.lineWidth = 2;
                const size = radius * 2;
                ctx.fillRect(p.relX - size / 2, p.relY - size / 2, size, size);
                ctx.strokeRect(p.relX - size / 2, p.relY - size / 2, size, size);
            } else {
                // Draw CIRCLE for normal point
                ctx.fillStyle = '#00ffcc'; // Bright Cyan
                ctx.strokeStyle = '#000';
                ctx.lineWidth = 1;
                ctx.arc(p.relX, p.relY, radius, 0, 2 * Math.PI);
                ctx.fill();
                ctx.stroke();
            }
        }
    }
    // Draw Active Stamp Ghost (Interactive Preview)
    drawActiveStamp() {
        if (!this.state.activeStamp) return;

        const ctx = this.ctx;
        const s = this.state.activeStamp;
        // console.log("Drawing Active Stamp:", s.char, s.pathData.length); // DEBUG (commented to avoid spam, uncomment if needed)

        ctx.save();
        ctx.strokeStyle = '#ff00ff'; // Magenta for Preview
        ctx.fillStyle = 'rgba(255, 0, 255, 0.1)';
        ctx.lineWidth = 1.5;

        // Settings for coordinate conversion
        const settings = this.state.settings;

        ctx.beginPath();
        for (let item of s.pathData) {
            // Points are relative to center (0,0) of the stamp
            const p1 = item.points[0];
            const p2 = item.points[1];

            // Transform P1
            const realX1 = (p1[0] * s.scale) + s.x;
            const realY1 = (p1[1] * s.scale) + s.y;

            // Transform P2
            const realX2 = (p2[0] * s.scale) + s.x;
            const realY2 = (p2[1] * s.scale) + s.y;

            // Convert to Canvas Pixels
            // Assuming we have a helper or manual conversion.
            // CanvasHandler doesn't export the simple conversion logic, but Point class uses settings.

            // Let's create temp Points just for conversion? Inefficient but clean.
            // Or replicate conversion logic: (X - Ox) * m_p * -1 ? wait Y is inverted.

            // Use Point class for consistency
            // Fix: Point constructor assumes PIXELS (Relative). We have METERS (Actual).
            // We must use setters to trigger conversion.
            const pt1 = new Point(0, 0, settings);
            pt1.actX = realX1;
            pt1.actY = realY1;

            const pt2 = new Point(0, 0, settings);
            pt2.actX = realX2;
            pt2.actY = realY2;

            ctx.moveTo(pt1.relX, pt1.relY);
            ctx.lineTo(pt2.relX, pt2.relY);
        }
        ctx.stroke();

        // Draw Center Crosshair
        // Center
        const cX = s.x;
        const cY = s.y;
        const cPt = new Point(0, 0, settings);
        cPt.actX = cX;
        cPt.actY = cY;

        ctx.beginPath();
        ctx.strokeStyle = '#ffff00';
        ctx.moveTo(cPt.relX - 5, cPt.relY);
        ctx.lineTo(cPt.relX + 5, cPt.relY);
        ctx.moveTo(cPt.relX, cPt.relY - 5);
        ctx.lineTo(cPt.relX, cPt.relY + 5);
        ctx.stroke();

        ctx.restore();
    }
}
