import cv2
import numpy as np
import base64
import io
from PIL import Image
from svgpathtools import svg2paths, Path, Line, Arc, CubicBezier, QuadraticBezier
import tempfile
import os

def process_image(file_data_base64, options):
    """
    Process an image (Raster or SVG) and return a trajectory.
    
    Args:
        file_data_base64 (str): Base64 encoded file data suitable for data URI scheme (start with "data:image/...")
        options (dict): Configuration options
            - mode: 'raster' | 'svg'
            - width: float (target width in meters)
            - x: float (offset x in meters)
            - y: float (offset y in meters)
            - rotation: float (degrees)
            - threshold: int (canny threshold for raster)
            - inverted: bool (for raster)
            
    Returns:
        list: List of patches (lines/curves) compatible with the robot GUI format.
    """
    
    try:
        # Decode Base64
        if ',' in file_data_base64:
            header, encoded = file_data_base64.split(',', 1)
        else:
            encoded = file_data_base64
            
        decoded_data = base64.b64decode(encoded)
        
        mode = options.get('mode', 'raster')
        width_m = float(options.get('width', 0.10)) # Target width in meters
        
        raw_paths = [] # List of lists of points [[(x,y), (x,y)], ...]
        
        import time
        t_start = time.time()
        print(f"[DEBUG] Starting Image Processing. Mode: {mode}")
        
        if mode == 'svg':
            raw_paths = _process_svg(decoded_data)
        elif mode == 'vector_bw' or True: # Default to new Vector BW engine for now
            raw_paths = _process_vector_bw(decoded_data, options)
        else:
            raw_paths = _process_raster(decoded_data, options)
            
        t_proc = time.time()
        print(f"[DEBUG] Processing complete in {t_proc - t_start:.4f}s. Found {len(raw_paths)} paths.")
            
        if not raw_paths:
            print("[DEBUG] No paths found.")
            return []
            
        # --- Normalization & Scaling ---
        
        # 1. Find Bounding Box of raw paths
        all_points = [p for path in raw_paths for p in path]
        if not all_points: 
            return []
            
        min_x = min(p[0] for p in all_points)
        max_x = max(p[0] for p in all_points)
        min_y = min(p[1] for p in all_points)
        max_y = max(p[1] for p in all_points)
        
        orig_w = max_x - min_x
        orig_h = max_y - min_y
        
        print(f"[DEBUG] Original BBox: {orig_w:.2f} x {orig_h:.2f}")
        
        if orig_w == 0: orig_w = 0.001
        
        scale = width_m / orig_w
        
        # User Transforms
        off_x = float(options.get('x', 0.20))
        off_y = float(options.get('y', 0.0))
        rot_deg = float(options.get('rotation', 0.0))
        rot_rad = np.radians(rot_deg)
        
        # Center of the original shape for rotation
        center_x = (min_x + max_x) / 2
        center_y = (min_y + max_y) / 2
        
        final_patches = []
        
        for path in raw_paths:
            # Optimize: Skip short paths
            if len(path) < 2: continue
            
            transformed_path = []
            
            for i, (px, py) in enumerate(path):
                # 1. Zero-center
                tx = (px - center_x) * scale
                ty = (py - center_y) * scale 
                
                # Image coordinate system: Y down
                # Robot coordinate system: Y up (usually)
                # Flip Y for image processing to match standard Cartesian
                ty = -ty 
                
                # 2. Rotate
                rx = tx * np.cos(rot_rad) - ty * np.sin(rot_rad)
                ry = tx * np.sin(rot_rad) + ty * np.cos(rot_rad)
                
                # 3. Translate
                fx = rx + off_x
                fy = ry + off_y
                
                transformed_path.append([fx, fy])
                
            final_patches.append({
                'type': 'line', 
                'points': transformed_path,
                'data': {'penup': False}
            })
        
        t_end = time.time()
        print(f"[DEBUG] Total Time: {t_end - t_start:.4f}s. Patches: {len(final_patches)}")
            
        return final_patches
            
    except Exception as e:
        import traceback
        traceback.print_exc()
        print(f"Error processing image: {e}")
        return []

def _process_raster(image_data, options):
    """
    Canny Edge Detection for Raster Images.
    """
    try:
        # Load image from bytes
        nparr = np.frombuffer(image_data, np.uint8)
        img = cv2.imdecode(nparr, cv2.IMREAD_GRAYSCALE)
        
        if img is None:
            return []
            
        # Optimization: Resize if too large (e.g. > 2000px) - INCREASED FOR PRECISION
        max_dim = 2000
        h, w = img.shape
        if h > max_dim or w > max_dim:
            scale = max_dim / max(h, w)
            new_w = int(w * scale)
            new_h = int(h * scale)
            img = cv2.resize(img, (new_w, new_h), interpolation=cv2.INTER_AREA)
            
        # Blur
        img_blur = cv2.GaussianBlur(img, (5, 5), 0)
        
        # Canny
        threshold = int(options.get('threshold', 100))
        edges = cv2.Canny(img_blur, threshold, threshold * 2)
        
        # Find Contours
        contours, hierarchy = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        paths = []
        
        # Sort by length (longest first) to prioritize important shapes
        contours = sorted(contours, key=lambda c: cv2.arcLength(c, True), reverse=True)
        
        # Optimization: Cap total contours to prevent JSON freeze
        max_contours = 200 
        if len(contours) > max_contours:
            contours = contours[:max_contours]
            
        for cnt in contours:
            # Optimization: Filter small noise (arc length < 15 pixels)
            length = cv2.arcLength(cnt, True)
            if length < 15: continue
            
            # Poly Approximation to reduce points
            # HIGH PRECISION: Reduced epsilon from 0.003 to 0.0005
            epsilon = 0.0005 * length 
            approx = cv2.approxPolyDP(cnt, epsilon, True)
            
            # Convert to list of (x,y)
            pts = [ (float(p[0][0]), float(p[0][1])) for p in approx ]
            
            # Close loop if closed
            if len(pts) > 2:
                pts.append(pts[0])
                
            paths.append(pts)
            
        return paths
        
    except Exception as e:
        print(f"Raster Error: {e}")
        return []

def _process_svg(svg_data):
    """
    Parse SVG paths using svgpathtools.
    """
    try:
        with tempfile.NamedTemporaryFile(delete=False, suffix='.svg') as tf:
            tf.write(svg_data)
            tf_path = tf.name
            
        paths, attributes = svg2paths(tf_path)
        
        os.remove(tf_path)
        
        result_paths = []
        
        for path in paths:
            # Estimate length
            length = path.length()
            if length == 0: continue
            
            # Dynamic sampling based on length (1 point every 1 unit) - INCREASED PRECISION
            num_samples = max(int(length / 1.0), 10) 
            
            pts = []
            for i in range(num_samples + 1):
                t = i / num_samples
                c_pt = path.point(t)
                pts.append((c_pt.real, c_pt.imag))
                
            result_paths.append(pts)
            
        return result_paths

    except Exception as e:
        print(f"SVG Error: {e}")
        return []

def _process_vector_bw(image_data, options):
    """
    Step 1: Strict Binary Vectorization (B/W Only)
    """
    try:
        # Load image from bytes
        nparr = np.frombuffer(image_data, np.uint8)
        img = cv2.imdecode(nparr, cv2.IMREAD_GRAYSCALE)
        
        if img is None: return []
            
        # Optimization: Resize for detail (High Res)
        max_dim = 2000
        h, w = img.shape
        if h > max_dim or w > max_dim:
            scale = max_dim / max(h, w)
            new_w = int(w * scale)
            new_h = int(h * scale)
            img = cv2.resize(img, (new_w, new_h), interpolation=cv2.INTER_AREA)

        # 1. Strict Binary Thresholding
        # Use user-defined threshold
        thresh_val = int(options.get('threshold', 127))
        inverted = bool(options.get('inverted', False))
        
        # If Inverted: White lines on Black bg -> We want White to be foreground (255)
        # If Normal: Black lines on White bg -> We want Black to be foreground (0->255)
        # THRESH_BINARY_INV: mask = src > thresh ? 0 : 255 (Dark is fg)
        # THRESH_BINARY: mask = src > thresh ? 255 : 0 (Light is fg)
        
        type_ = cv2.THRESH_BINARY if inverted else cv2.THRESH_BINARY_INV
        _, binary = cv2.threshold(img, thresh_val, 255, type_)

        # Morphological Cleanup (Optional, removes single noise pixels)
        # Controlled by User Toggle
        if options.get('noise_reduction', False):
            kernel = np.ones((2,2), np.uint8)
            binary = cv2.morphologyEx(binary, cv2.MORPH_OPEN, kernel) # Remove noise

        # Step 2: Full Contour Extraction (RETR_TREE)
        # Use RETR_TREE to capture full hierarchy (including holes)
        contours, hierarchy = cv2.findContours(binary, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
        
        paths = []
        for cnt in contours:
             # Filter noise (dust)
             length = cv2.arcLength(cnt, True)
             if length < 2: continue # Was 10, reduced to 2 for micro-details

             # Step 3: Curve Smoothing
             # 1. Initial simplify to get main shape (remove pixel steps)
             epsilon = 0.002 * length 
             approx = cv2.approxPolyDP(cnt, epsilon, True)
             pts = [ (float(p[0][0]), float(p[0][1])) for p in approx ]
             
             # 2. Apply Chaikin Smoothing (Corner Cutting)
             # User defined smoothing iterations
             smooth_iter = int(options.get('smoothing', 2))
             if len(pts) > 2 and smooth_iter > 0:
                 pts = _chaikin_smooth(pts, iterations=smooth_iter)
                 pts.append(pts[0]) # Close loop
                 paths.append(pts)
                 
        # Step 4: Sorting (Optimize Travel)
        paths = _sort_paths(paths)
                 
        return paths

    except Exception as e:
        print(f"Vector BW Error: {e}")
        return []

def _chaikin_smooth(points, iterations=2):
    """
    Chaikin's Corner Cutting Algorithm for curve smoothing.
    Replaces each corner with two new points (at 25% and 75% of the segment).
    """
    if len(points) < 3: return points
    
    smoothed = points
    for _ in range(iterations):
        new_points = []
        for i in range(len(smoothed)):
            p0 = smoothed[i]
            p1 = smoothed[(i + 1) % len(smoothed)] # Wrap around
            
            # Q = 0.75*P0 + 0.25*P1
            # R = 0.25*P0 + 0.75*P1
            
            x0, y0 = p0
            x1, y1 = p1
            
            qx = 0.75 * x0 + 0.25 * x1
            qy = 0.75 * y0 + 0.25 * y1
            
            rx = 0.25 * x0 + 0.75 * x1
            ry = 0.25 * y0 + 0.75 * y1
            
            new_points.append((qx, qy))
            new_points.append((rx, ry))
        smoothed = new_points
        
    return smoothed

def _sort_paths(paths):
    """
    Sorts paths using a Nearest Neighbor Heuristic to minimize Pen-Up travel.
    """
    if not paths: return []
    
    sorted_paths = []
    current_pos = (0,0) # Assume robot starts at origin (or last end)
    
    # Simple Greedy: Find closest start point to current position
    remaining = paths[:]
    
    while remaining:
        best_idx = -1
        min_dist = float('inf')
        
        for i, path in enumerate(remaining):
            start = path[0]
            # Euclidean distance sq
            d = (start[0]-current_pos[0])**2 + (start[1]-current_pos[1])**2
            
            if d < min_dist:
                min_dist = d
                best_idx = i
        
        # Add best path
        next_path = remaining.pop(best_idx)
        sorted_paths.append(next_path)
        
        # Update current pos to end of this path
        current_pos = next_path[-1]
        
    return sorted_paths
