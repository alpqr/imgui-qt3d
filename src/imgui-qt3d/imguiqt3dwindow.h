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

#ifndef IMGUIQT3DWINDOW_H
#define IMGUIQT3DWINDOW_H

#include <QWindow>
#include <QAspectEngine>
#include <functional>

namespace Qt3DRender {
class QCamera;
class QLayer;
class QFilterKey;
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
    Qt3DRender::QFilterKey *guiTechniqueFilterKey() const { return m_guiTechniqueFilterKey; }

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
    Qt3DRender::QFilterKey *m_guiTechniqueFilterKey = nullptr;

    CreateSceneFunc m_createScene = nullptr;
};

#endif
