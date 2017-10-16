Integration of [Dear ImGui](https://github.com/ocornut/imgui) with [Qt 3D](https://doc.qt.io/qt-5/qt3d-index.html) via the Entity-Component system and the framegraph,
meaning that direct usage of OpenGL or other graphics APIs is avoided and no private APIs or engine changes are necessary.

Also a good example for integrating any foreign engine that streams vertex/index data.

Tested with qt3d/dev.

Input support is work in progress. Only mouse is supported for now.

To get proper drawcall ordering (back to front), apply the included patch (0001-Base-BackToFront-sorting-key-on-z-value-only.patch) to Qt 3D.
