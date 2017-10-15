#ifndef IMGUIQT3DWINDOW_H
#define IMGUIQT3DWINDOW_H

#include <QWindow>
#include <QAspectEngine>
#include <functional>

namespace Qt3DRender {
class QCamera;
class QLayer;
}

// Provides a window with a Qt3D scene and a framegraph with a gui pass. This
// is just an example since the framegraph has to be adapted in arbitrary ways
// in real projects.
class ImguiQt3DWindow : public QWindow
{
public:
    ImguiQt3DWindow();

    typedef std::function<void(Qt3DCore::QEntity *parent)> CreateSceneFunc;
    void setCreateSceneFunc(CreateSceneFunc f) { m_createScene = f; }

    Qt3DRender::QLayer *guiTag() const { return m_guiTag; }
    Qt3DRender::QLayer *activeGuiTag() const { return m_activeGuiTag; }

protected:
    void exposeEvent(QExposeEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    void createAspectEngine();
    void createFramegraph();

    QScopedPointer<Qt3DCore::QAspectEngine> m_aspectEngine;
    Qt3DCore::QEntity *m_rootEntity = nullptr;
    Qt3DRender::QCamera *m_sceneCamera = nullptr;
    Qt3DRender::QCamera *m_guiCamera = nullptr;
    Qt3DRender::QLayer *m_guiTag = nullptr;
    Qt3DRender::QLayer *m_activeGuiTag = nullptr;

    CreateSceneFunc m_createScene = nullptr;
};

#endif
