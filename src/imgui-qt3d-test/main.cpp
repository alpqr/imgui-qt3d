#include <QGuiApplication>
#include <QPropertyAnimation>
#include <QEntity>
#include <QTransform>
#include <QCuboidMesh>
#include <QTorusMesh>
#include <QPhongMaterial>

#include "imguimanager.h"
#include "imguiqt3dwindow.h"

#include "gui.h"

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    Gui gui;
    ImguiManager guiMgr;
    guiMgr.setFrameFunc(std::bind(&Gui::frame, &gui));

    ImguiQt3DWindow w;
    guiMgr.setDisplaySizeFunc([&w]() { return w.size() * w.devicePixelRatio(); });
    guiMgr.setActiveGuiTagFunc([&w]() { return w.activeGuiTag(); });

    w.setCreateSceneFunc([&w, &guiMgr](Qt3DCore::QEntity *parent) {
        guiMgr.initialize(parent);

        Qt3DCore::QEntity *cube = new Qt3DCore::QEntity(parent);
        Qt3DExtras::QCuboidMesh *cubeGeom = new Qt3DExtras::QCuboidMesh;
        cubeGeom->setXExtent(2);
        cubeGeom->setYExtent(2);
        cubeGeom->setZExtent(2);
        Qt3DCore::QTransform *cubeTrans = new Qt3DCore::QTransform;
        cubeTrans->setTranslation(QVector3D(18, 5, -10));
        Qt3DExtras::QPhongMaterial *cubeMat = new Qt3DExtras::QPhongMaterial;
        cube->addComponent(cubeGeom);
        cube->addComponent(cubeTrans);
        cube->addComponent(cubeMat);

        Qt3DCore::QEntity *cube2 = new Qt3DCore::QEntity(parent);
        // or, why have a cube when we can have a torus instead
        Qt3DExtras::QTorusMesh *cubeGeom2 = new Qt3DExtras::QTorusMesh;
        cubeGeom2->setRadius(5);
        cubeGeom2->setMinorRadius(1);
        cubeGeom2->setRings(100);
        cubeGeom2->setSlices(20);
        Qt3DCore::QTransform *cubeTrans2 = new Qt3DCore::QTransform;
        cubeTrans2->setTranslation(QVector3D(-15, -4, -20));
        Qt3DExtras::QPhongMaterial *cubeMat2 = new Qt3DExtras::QPhongMaterial;
        cube2->addComponent(cubeGeom2);
        cube2->addComponent(cubeTrans2);
        cube2->addComponent(cubeMat2);

        auto rotAnim = [](QObject *obj, const QByteArray &name, float start, float end, int duration) {
            QPropertyAnimation *anim = new QPropertyAnimation(obj, name);
            anim->setStartValue(start);
            anim->setEndValue(end);
            anim->setDuration(duration);
            anim->setLoopCount(-1);
            anim->start();
        };
        rotAnim(cubeTrans, "rotationX", 0.0f, 360.0f, 5000);
        rotAnim(cubeTrans2, "rotationY", 0.0f, 360.0f, 5000);
    });

    w.resize(1280, 720);
    w.show();

    return app.exec();
}
