/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QGuiApplication>
#include <QSurfaceFormat>
#include <QOpenGLContext>
#include <QPropertyAnimation>
#include <QEntity>
#include <QTransform>
#include <QCuboidMesh>
#include <QTorusMesh>
#include <QSphereMesh>
#include <QPhongMaterial>
#include <QObjectPicker>
#include <QPickEvent>

#include "imguimanager.h"
#include "imguiqt3dwindow.h"

#include "gui.h"

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL) {
        fmt.setVersion(3, 2);
        fmt.setProfile(QSurfaceFormat::CoreProfile);
    }
    QSurfaceFormat::setDefaultFormat(fmt);

    ImguiQt3DWindow w;
    w.setFormat(fmt);

    Gui gui;
    ImguiManager guiMgr;
    gui.setManager(&guiMgr);
    guiMgr.setFrameFunc(std::bind(&Gui::frame, &gui, std::placeholders::_1));
    guiMgr.setInputEventSource(&w);
    guiMgr.setOutputInfoFunc([&w]() {
        ImguiManager::OutputInfo outputInfo;
        outputInfo.size = w.size();
        outputInfo.dpr = w.devicePixelRatio();
        outputInfo.guiTag = w.guiTag();
        outputInfo.activeGuiTag = w.activeGuiTag();
        outputInfo.guiTechniqueFilterKey = w.guiTechniqueFilterKey();
        return outputInfo;
    });
    // uncomment to start with gui hidden
    //guiMgr.setEnabled(false);

    w.setCreateSceneFunc([&guiMgr](Qt3DCore::QEntity *parent) {
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
        cube2->addComponent(cubeGeom2);
        cube2->addComponent(cubeTrans2);
        cube2->addComponent(cubeMat);

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

        // an entity that toggles the gui when pressed.
        // replace with a QText2DEntity some day when it actually works. in the meantime a sphere will do.
        Qt3DCore::QEntity *toggleText = new Qt3DCore::QEntity(parent);
        Qt3DExtras::QSphereMesh *toggleTextGeom = new Qt3DExtras::QSphereMesh;
        Qt3DCore::QTransform *toggleTextTrans = new Qt3DCore::QTransform;
        toggleTextTrans->setTranslation(QVector3D(-14, 7, -5));
        toggleTextTrans->setScale(0.5f);
        Qt3DExtras::QPhongMaterial *toggleTextMat = new Qt3DExtras::QPhongMaterial;
        toggleTextMat->setDiffuse(guiMgr.isEnabled() ? Qt::green : Qt::red);
        toggleText->addComponent(toggleTextGeom);
        toggleText->addComponent(toggleTextTrans);
        toggleText->addComponent(toggleTextMat);
        Qt3DRender::QObjectPicker *toggleTextPicker = new Qt3DRender::QObjectPicker;
        QObject::connect(toggleTextPicker, &Qt3DRender::QObjectPicker::pressed, [toggleTextMat, &guiMgr](Qt3DRender::QPickEvent *) {
            guiMgr.setEnabled(!guiMgr.isEnabled());
            toggleTextMat->setDiffuse(guiMgr.isEnabled() ? Qt::green : Qt::red);
        });
        toggleText->addComponent(toggleTextPicker);
    });

    w.resize(1280, 720);
    w.show();

    return app.exec();
}
