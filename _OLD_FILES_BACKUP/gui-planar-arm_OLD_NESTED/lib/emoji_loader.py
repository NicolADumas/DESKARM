import numpy as np
from svgpathtools import parse_path, Line, Arc, CubicBezier, QuadraticBezier
import math

# Sample SVG Path Data
# In a full implementation, this could load from a folder of .svg files
import json
import os

# Stamp Categories
STAMP_DB = {
    "SMILE": {
        "😊": "M100,160 Q128,190 156,160 M128,256 A128,128 0 1,0 128,0 A128,128 0 1,0 128,256 M80,100 A10,10 0 1,1 80,120 A10,10 0 1,1 80,100 M176,100 A10,10 0 1,1 176,120 A10,10 0 1,1 176,100"
    },
    "MANI": {},
    "PERSONE": {},
    "ANIMALI": {
        "🐶": "M50,130 Q30,60 80,40 Q130,20 180,40 Q230,60 210,130 Q240,180 130,220 Q20,180 50,130 M80,40 L60,10 L100,30 M180,40 L200,10 L160,30 M100,100 A10,10 0 1,1 100,120 A10,10 0 1,1 100,100 M160,100 A10,10 0 1,1 160,120 A10,10 0 1,1 160,100",
        "🐱": "M50,150 L30,50 L80,90 Q128,70 176,90 L226,50 L206,150 Q226,200 128,240 Q30,200 50,150 M80,130 A10,10 0 1,1 80,150 A10,10 0 1,1 80,130 M176,130 A10,10 0 1,1 176,150 A10,10 0 1,1 176,130 M40,160 L10,150 M40,170 L10,180 M216,160 L246,150 M216,170 L246,180",
        "🐟": "M220,128 Q180,64 100,64 Q40,64 20,128 Q40,192 100,192 Q180,192 220,128 L250,90 L250,166 Z M180,100 A10,10 0 1,1 180,120 A10,10 0 1,1 180,100"
    },
    "SPORT": {},
    "PLACES": {},
    "TRASPORTI": {},
    "FESTE": {
        "❤️": "M 10,30 A 20,20 0,0,1 50,30 A 20,20 0,0,1 90,30 Q 90,60 50,90 Q 10,60 10,30 z",
        "⭐": "M20.388,10.918L32,12.118l-8.735,7.749L25.914,31.4l-9.893-6.088L6.127,31.4l2.695-11.533L0,12.118 l11.547-1.2L16.026,0.6L20.388,10.918z",
        "⚡": "M8.69666 0.040354C8.90859 0.131038 9.03105 0.354856 8.99315 0.582235L8.09019 6.00001H12.4999C12.6893 6.00001 12.8625 6.10701 12.9472 6.2764C13.0318 6.44579 13.0136 6.6485 12.8999 6.8L6.89997 14.8C6.76166 14.9844 6.5152 15.0503 6.30327 14.9596C6.09134 14.869 5.96888 14.6451 6.00678 14.4178L6.90974 8.99999H2.49999C2.31061 8.99999 2.13747 8.89299 2.05278 8.7236C1.96808 8.55421 1.98636 8.3515 2.09999 8.2L8.09996 0.200037C8.23827 0.0156255 8.48473 -0.0503301 8.69666 0.040354ZM3.49999 8H7.49996C7.64694 8 7.78647 8.06466 7.88147 8.17681C7.97647 8.28895 8.01732 8.43722 7.99316 8.58219L7.33026 12.5596L11.4999 7H7.49996C7.35299 7 7.21346 6.93534 7.11846 6.82319C7.02346 6.71105 6.98261 6.56278 7.00677 6.41781L7.66967 2.44042L3.49999 8Z"
    },
    "CIBO": {},
    "PIANTE": {},
    "AUDIO": {},
    "STRUMENTI MUSICALI": {},
    "FRUTTA": {}
}

CUSTOM_STAMPS_FILE = "custom_stamps.json"

def load_custom_stamps():
    if not os.path.exists(CUSTOM_STAMPS_FILE):
        return {}
    try:
        with open(CUSTOM_STAMPS_FILE, 'r', encoding='utf-8') as f:
            return json.load(f)
    except Exception as e:
        print(f"Error loading custom stamps: {e}")
        return {}

def save_custom_stamp(category, icon, path_data):
    stamps = load_custom_stamps()
    if category not in stamps:
        stamps[category] = {}
    stamps[category][icon] = path_data
    
    try:
        with open(CUSTOM_STAMPS_FILE, 'w', encoding='utf-8') as f:
            json.dump(stamps, f, ensure_ascii=False, indent=2)
        return {"success": True}
    except Exception as e:
        return {"success": False, "error": str(e)}

def get_available_emojis():
    # Merge Default DB with Custom Stamps
    custom = load_custom_stamps()
    
    merged = {}
    all_stamps = {}

    # 1. Process Default DB
    for cat, items in STAMP_DB.items():
        merged[cat] = items.copy()
        all_stamps.update(items)

    # 2. Process Custom
    for cat, items in custom.items():
        if cat not in merged:
            merged[cat] = {}
        merged[cat].update(items)
        all_stamps.update(items)
    
    # 3. Create Final Ordered Dict with "All" first
    final_db = {"All": all_stamps}
    final_db.update(merged)
            
    return final_db

def generate_emoji_trajectory(emoji_char, target_size=0.05):
    """
    Parses SVG path for emoji and returns list of primitives.
    target_size: Desired width/height (meters) in workspace (default 5cm)
    """
    # Search for emoji in all categories
    path_data = None
    for category in STAMP_DB:
        if emoji_char in STAMP_DB[category]:
            path_data = STAMP_DB[category][emoji_char]
            break
            
    if not path_data:
        return {"success": False, "error": "Stamp not found"}
    
    try:
        path = parse_path(path_data)
        
        # Calculate Bounding Box to normalize
        xmin, xmax, ymin, ymax = path.bbox()
        width = xmax - xmin
        height = ymax - ymin
        
        if width == 0 or height == 0:
             return {"success": False, "error": "Invalid path dimensions"}

        scale = target_size / max(width, height)
        
        # Center offset (to center at 0,0)
        cx = (xmin + xmax) / 2
        cy = (ymin + ymax) / 2
        
        trajectory_data = []
        
        # Parse segments
        for segment in path:
            if isinstance(segment, Line):
                trajectory_data.append({
                    "type": "line",
                    "points": [
                        transform_point(segment.start, scale, cx, cy),
                        transform_point(segment.end, scale, cx, cy)
                    ]
                })
            elif isinstance(segment, Arc):
                 # Convert Arc to center/radius/angles or discretize
                 # Robot trajectory add_circle needs center, radius, theta0, theta1
                 # svgpathtools Arc: start, end, radius, rotation, large_arc, sweep
                 # We can calculate geometric center and angles.
                 
                 # Note: svgpathtools complex numbers are x + iy.
                 # Y axis in SVG is usually down. Robot Y is Up? 
                 # We handle Y flip in frontend or here. Let's flip Y here to match Cartesian.
                 
                 # Simplifying: Flatten arcs to lines for robustness if complex
                 # OR: attempt full arc support.
                 
                 # Let's discretize everything to small lines for maximum compatibility first version
                 NUM_POINTS = 10
                 for i in range(NUM_POINTS):
                     t1 = i / NUM_POINTS
                     t2 = (i + 1) / NUM_POINTS
                     p1 = segment.point(t1)
                     p2 = segment.point(t2)
                     trajectory_data.append({
                        "type": "line",
                        "points": [
                            transform_point(p1, scale, cx, cy),
                            transform_point(p2, scale, cx, cy)
                        ]
                    })

            elif isinstance(segment, (QuadraticBezier, CubicBezier)):
                # Flatten Bezier
                NUM_POINTS = 10
                for i in range(NUM_POINTS):
                     t1 = i / NUM_POINTS
                     t2 = (i + 1) / NUM_POINTS
                     p1 = segment.point(t1)
                     p2 = segment.point(t2)
                     trajectory_data.append({
                        "type": "line",
                        "points": [
                            transform_point(p1, scale, cx, cy),
                            transform_point(p2, scale, cx, cy)
                        ]
                    })
                    
        return {
            "success": True, 
            "data": trajectory_data,
            "bbox": {
                "min": [-target_size/2, -target_size/2], 
                "max": [target_size/2, target_size/2]
            }
        }

    except Exception as e:
        return {"success": False, "error": str(e)}

def transform_point(complex_pt, scale, cx, cy):
    # Convert complex -> [x, y]
    # Center and Scale
    
    # SVG Y is down (usually), Cartesian Y is up.
    # To flip Y: (y - cy) * -1
    
    x = (complex_pt.real - cx) * scale
    y = (complex_pt.imag - cy) * scale * -1 # Flip Y
    
    return [x, y]
