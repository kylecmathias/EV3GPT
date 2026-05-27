from dependencies import add_import, DAV2_PATH, DAV2_WEIGHTS
add_import(DAV2_PATH)

import asyncio
import numpy as np
import cv2
import torch
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from depth_anything_v2.dpt import DepthAnythingV2

#TODO: Switch to tensorrt

device = "cuda" if torch.cuda.is_available() else "cpu"
dav2_config = {'vits': {'encoder': 'vits', 'features': 64, 'out_channels': [48, 96, 192, 384]}}

class Vision:
    """Class for the computer vision pipeline"""
    def __init__(self):
        self._executor = ThreadPoolExecutor(max_workers=1)
        self._lock = asyncio.Lock()

        self.latest_depth = None
        self.latest_objects: list[tuple[str, float, int, int, int, int]] = [] #[("name", conf.conf, top_left_x, top_left_y, bound_w, bound_h)]
        self.latest_path = None

        self._dav2 = DepthAnythingV2(**dav2_config['vits'])
        self._dav2.load_state_dict(torch.load(DAV2_WEIGHTS, map_location=device))
        self._dav2 = self._dav2.to(device).eval()

        #TODO: Initialize yolo model and pathfinding model

    async def run_vision(self, frame, frame_num, dist):
        """Main method for running vision models"""
        if frame_num % 2 == 0:
            asyncio.create_task(self._depth_map(frame))

    async def _depth_map(self, frame):
        """Process incoming frame and return normalized heatmap"""
        raw_depth = await asyncio.get_event_loop().run_in_executor(self._executor, self._run_depth, frame)

        self.latest_depth = cv2.applyColorMap(raw_depth, cv2.COLORMAP_INFERNO)

    async def path_finder(self, frame):
        pass

    async def object_detection(self, frame):
        pass 
    
    @torch.no_grad()
    def _run_depth(self, frame):
        """Helper for process incoming frame and return normalized heatmap"""
        depth = self._dav2.infer_image(frame)

        depth_tensor = torch.from_numpy(depth).to(device)

        depth_min, depth_max = depth_tensor.min(), depth_tensor.max()

        if depth_max > depth_min:
            depth_norm = (depth_tensor - depth_min) / (depth_max - depth_min) * 255.0
        else:
            depth_norm = torch.zeros_like(depth_tensor)

        return depth_norm.cpu().numpy().astype(np.uint8)
    
    def _scale_depth_map(self, depth_map, dist):
        pass