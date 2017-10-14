#include "imguiqt3dwindow.h"

#include <QEntity>
#include <QRenderAspect>
#include <QInputAspect>
#include <QAnimationAspect>
#include <QLogicAspect>

#include <QRenderSettings>
#include <QRenderSurfaceSelector>
#include <QViewport>
#include <QCameraSelector>
#include <QClearBuffers>
#include <QTechniqueFilter>
#include <QFilterKey>
#include <QNoDraw>
#include <QLayerFilter>
#include <QLayer>
#include <QCamera>

ImguiQt3DWindow::ImguiQt3DWindow()
{
    setSurfaceType(QSurface::OpenGLSurface);
    createAspectEngine();
}

void ImguiQt3DWindow::createAspectEngine()
{
    m_aspectEngine.reset(new Qt3DCore::QAspectEngine);
    m_aspectEngine->registerAspect(new Qt3DRender::QRenderAspect);
    m_aspectEngine->registerAspect(new Qt3DInput::QInputAspect);
    m_aspectEngine->registerAspect(new Qt3DAnimation::QAnimationAspect);
    m_aspectEngine->registerAspect(new Qt3DLogic::QLogicAspect);
}

void ImguiQt3DWindow::exposeEvent(QExposeEvent *)
{
    if (isExposed() && !m_rootEntity) {
        createFramegraph();
        if (m_createScene)
            m_createScene(m_rootEntity);
        m_aspectEngine->setRootEntity(Qt3DCore::QEntityPtr(m_rootEntity));
    }
}

void ImguiQt3DWindow::resizeEvent(QResizeEvent *)
{
    if (m_sceneCamera)
        m_sceneCamera->setAspectRatio(width() / float(height()));

    if (m_guiCamera) {
        m_guiCamera->setRight(width() * devicePixelRatio());
        m_guiCamera->setBottom(height() * devicePixelRatio());
    }
}

void ImguiQt3DWindow::createFramegraph()
{
    m_rootEntity = new Qt3DCore::QEntity;
    m_rootEntity->setObjectName(QLatin1String("root"));

    Qt3DRender::QRenderSettings *frameGraphComponent = new Qt3DRender::QRenderSettings(m_rootEntity);

    // RenderSurfaceSelector -> Viewport -> (ClearBuffers -> NoDraw) | (CameraSelector -> TechniqueFilter -> LayerFilter) | (CameraSelector -> TechniqueFilter -> LayerFilter)

    Qt3DRender::QRenderSurfaceSelector *targetSel = new Qt3DRender::QRenderSurfaceSelector;

    Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport(targetSel);
    viewport->setNormalizedRect(QRectF(0, 0, 1, 1));

    Qt3DRender::QClearBuffers *clearBuffers = new Qt3DRender::QClearBuffers(viewport);
    clearBuffers->setBuffers(Qt3DRender::QClearBuffers::ColorDepthBuffer);
    clearBuffers->setClearColor(Qt::lightGray);
    new Qt3DRender::QNoDraw(clearBuffers);

    m_guiTag = new Qt3DRender::QLayer; // all gui entities are tagged with this
    m_activeGuiTag = new Qt3DRender::QLayer; // active gui entities - added/removed to entities dynamically by imguimanager

    // The normal scene, filter for 'forward' which is set by the "default" materials in Qt3DExtras.
    // Change the key (or switch to RenderPassFilter, extend for multi-pass, etc.) when using custom materials.
    Qt3DRender::QCameraSelector *cameraSel = new Qt3DRender::QCameraSelector(viewport);
    m_sceneCamera = new Qt3DRender::QCamera;
    m_sceneCamera->setProjectionType(Qt3DRender::QCameraLens::PerspectiveProjection);
    m_sceneCamera->setFieldOfView(60);
    m_sceneCamera->setPosition(QVector3D(0, 0, 10));
    m_sceneCamera->setViewCenter(QVector3D(0, 0, 0));
    m_sceneCamera->setNearPlane(10);
    m_sceneCamera->setFarPlane(5000);
    m_sceneCamera->setAspectRatio(width() / float(height()));
    cameraSel->setCamera(m_sceneCamera);
    Qt3DRender::QTechniqueFilter *tfilter = new Qt3DRender::QTechniqueFilter(cameraSel);
    Qt3DRender::QFilterKey *tfilterkey = new Qt3DRender::QFilterKey;
    tfilterkey->setName(QLatin1String("renderingStyle"));
    tfilterkey->setValue(QLatin1String("forward"));
    tfilter->addMatch(tfilterkey);
    // Skip all gui entities in this pass.
    Qt3DRender::QLayerFilter *lfilter = new Qt3DRender::QLayerFilter(tfilter);
    lfilter->setFilterMode(Qt3DRender::QLayerFilter::DiscardAnyMatchingLayers);
    lfilter->addLayer(m_guiTag);

    // The gui pass.
    cameraSel = new Qt3DRender::QCameraSelector(viewport);
    m_guiCamera = new Qt3DRender::QCamera;
    m_guiCamera->setProjectionType(Qt3DRender::QCameraLens::OrthographicProjection);
    m_guiCamera->setLeft(0);
    m_guiCamera->setRight(width() * devicePixelRatio());
    m_guiCamera->setTop(0);
    m_guiCamera->setBottom(height() * devicePixelRatio());
    m_guiCamera->setNearPlane(-1);
    m_guiCamera->setFarPlane(1);
    cameraSel->setCamera(m_guiCamera);
    tfilter = new Qt3DRender::QTechniqueFilter(cameraSel);
    tfilterkey = new Qt3DRender::QFilterKey;
    tfilterkey->setName(QLatin1String("gui"));
    tfilterkey->setValue(QLatin1String("1"));
    tfilter->addMatch(tfilterkey);
    // Only include entities for the _active_ gui in this pass.
    lfilter = new Qt3DRender::QLayerFilter(tfilter);
    lfilter->addLayer(m_activeGuiTag);

    targetSel->setSurface(this);
    frameGraphComponent->setActiveFrameGraph(targetSel);
    m_rootEntity->addComponent(frameGraphComponent);
}
