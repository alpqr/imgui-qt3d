Integration of [Dear ImGui](https://github.com/ocornut/imgui) with [Qt 3D](https://doc.qt.io/qt-5/qt3d-index.html) via the Entity-Component system and the framegraph,
meaning that direct usage of OpenGL or other graphics APIs is avoided and no private APIs or engine changes are necessary.

Also a good example for integrating any foreign engine that streams vertex/index data.

Tested with qt3d/dev. Note [QTBUG-63785](https://bugreports.qt.io/browse/QTBUG-63785) (harmless for now, may be a problem for input, remains to be seen).

Input support is missing atm and is coming soon.
