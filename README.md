# Zoomer Application (Windows)

A lightweight and fast Windows application designed for zooming screenshots using AI upscaling and advanced post-processing via custom shaders.

## Features

* **Pure C Implementation:** Written completely in C for maximum performance and low memory footprint.
* **Hardware-Accelerated Graphics:** Utilizes OpenGL (via GLFW) for smooth rendering and real-time shader effects.
* **AI Upscaling:** Powered by ONNX Runtime with DirectML execution provider for hardware-accelerated AI enhancement on Windows.
* **Shader Post-Processing:** Customizable shaders for high-quality image filtering and post-processing adjustments.
* **Windows-Native:** Heavily optimized and tailored specifically for the Windows environment using native Win32 APIs.

## Requirements

* **Operating System:** Windows 10 / 11 (64-bit)
* **Graphics:** GPU with DirectX 12 support (required for DirectML) and OpenGL 3.3+ compatibility.
