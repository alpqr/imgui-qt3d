Integration of [Dear ImGui](https://github.com/ocornut/imgui) with [Qt 3D](https://doc.qt.io/qt-5/qt3d-index.html) via the Entity-Component system and the framegraph,
meaning that direct usage of OpenGL or other graphics APIs is avoided and no private APIs or engine changes are necessary.

Also a good example for integrating any foreign engine that streams vertex/index data.

Tested with qt3d/5.11. Should work with dev and older branches as well but those have not been tested.
