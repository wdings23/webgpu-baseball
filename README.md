# webgpu-dawn-example

WebGPU implementation of simple mesh viewer with zooming, panning, rotation, and hide/reveal functionalty. Native WebGPU uses Chromium's Dawn that has support for MultiDrawIndirect, rendering meshes in one call. Fallback for emscripten build is loop through drawIndexedIndirect with the number of meshes.

# Get Dawn
git submodule add https://dawn.googlesource.com/dawn

# Build the app with CMake.
cmake -B build -DCURL_LIBRARY=<curl_lib_path> -DCURL_INCLUDE_DIR=<curl_include_path> && cmake --build build -j4

Make sure the app has can see the backend library, ie. vulkan.1.dll, libcurl.dll, etc.
Make sure the app can also see the dawn dll file.
Modify Limits.cpp in dawn if necessary

# Build the app with Emscripten.
emsdk_sh && emcmake cmake -B build-web && cmake --build build-web -j4

# Mesh Assets are built separately, converting OBJ files to binary format for faster loading. It uses a local version of obj2binary application.

# Controls
Keyboard Button
    W - Move forward
    A - Strafe left
    S - Move backward
    D - Strafe right
    H - Hide selected mesh (mesh with yellow highlight)
    J - Reveal last hidden mesh

Mouse
    Left click - selects mesh, rotate camera if held down with mouse movement 
    Right Click - pans the camera if held down with mouse movement 

# http server for assets
npx http-server --cors -p 8080

# http server for web version
npx http-server -p 8000

# local url
http://localhost:8000/app.html

# web version (youtube clip)
[![video](https://img.youtube.com/vi/8E8v86ZudF4/0.jpg)](https://www.youtube.com/watch?v=8E8v86ZudF4)


# native version (youtube clip)
[![web version](https://img.youtube.com/vi/CzTR04N-yy4/0.jpg)](https://www.youtube.com/watch?v=CzTR04N-yy4)

