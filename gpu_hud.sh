#!/bin/bash

cores=$(nproc)
cores_str="cpu0"

for ((i = 1; i < $cores; i++)); do
   cores_str="${cores_str}+cpu${i}"
done

#### For OpenGL

export GALLIUM_HUD=".dfps:160,frametime,cpu+GPU-load:100=gpu,${cores_str}:100"
# more options:
# VRAM-usage

#### For Vulkan:

# more options:
export VK_LAYER_MESA_OVERLAY_CONFIG="position=top-left"
export VK_INSTANCE_LAYERS=VK_LAYER_MESA_overlay

"$@" &
disown -h %+
