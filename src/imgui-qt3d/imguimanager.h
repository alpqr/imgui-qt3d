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

#ifndef IMGUIMANAGER_H
#define IMGUIMANAGER_H

#include <functional>
#include <QSize>
#include <QEntity>

class ImguiInputEventFilter;

namespace Qt3DCore {
class QTransform;
}

namespace Qt3DRender {
class QBuffer;
class QTexture2D;
class QMaterial;
class QLayer;
class QGeometryRenderer;
class QShaderProgram;
class QScissorTest;
class QParameter;
class QFilterKey;
class QDepthTest;
class QNoDepthMask;
class QBlendEquation;
class QBlendEquationArguments;
class QCullFace;
class QColorMask;
}

class ImguiManager {
public:
    ~ImguiManager();

    typedef std::function<void(Qt3DCore::QEntity *)> FrameFunc;
    void setFrameFunc(FrameFunc f) { m_frame = f; }

    void setInputEventSource(QObject *src);

    struct OutputInfo {
        QSize size;
        qreal dpr;
        Qt3DRender::QLayer *guiTag;
        Qt3DRender::QLayer *activeGuiTag;
        Qt3DRender::QFilterKey *guiTechniqueFilterKey;
    };
    typedef std::function<OutputInfo()> OutputInfoFunc;
    void setOutputInfoFunc(OutputInfoFunc f) { m_outputInfoFunc = f; }

    void initialize(Qt3DCore::QEntity *rootEntity);
    void releaseResources();

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    float scale() const { return m_scale; }
    void setScale(float scale);

private:
    struct CmdListEntry;
    void resizePool(CmdListEntry *e, int newSize);
    Qt3DRender::QMaterial *buildMaterial(Qt3DRender::QScissorTest **scissor);
    void updateGeometry(CmdListEntry *e, int idx, uint elemCount, int vertexCount, int indexCount, const void *indexOffset);
    void update3D();
    void updateInput();

    FrameFunc m_frame = nullptr;
    OutputInfoFunc m_outputInfoFunc = nullptr;
    OutputInfo m_outputInfo;
    QObject *m_inputEventSource = nullptr;
    ImguiInputEventFilter *m_inputEventFilter = nullptr;
    bool m_inputInitialized = false;
    bool m_enabled = true;
    float m_scale = 1.0f;

    Qt3DCore::QEntity *m_rootEntity = nullptr;
    Qt3DRender::QTexture2D *m_atlasTex = nullptr;
    struct SharedRenderPassData {
        bool valid = false;
        Qt3DRender::QShaderProgram *progES2;
        Qt3DRender::QShaderProgram *progGL3;
        Qt3DRender::QFilterKey *techniqueFilterKey;
        Qt3DRender::QDepthTest *depthTest;
        Qt3DRender::QNoDepthMask *noDepthWrite;
        Qt3DRender::QBlendEquation *blendFunc;
        Qt3DRender::QBlendEquationArguments *blendArgs;
        Qt3DRender::QCullFace *cullFace;
        Qt3DRender::QColorMask *colorMask;
        QVector<Qt3DCore::QNode *> enabledToggle;
    } rpd;

    struct CmdEntry {
        Qt3DCore::QEntity *entity = nullptr;
        Qt3DRender::QMaterial *material = nullptr;
        Qt3DRender::QGeometryRenderer *geomRenderer = nullptr;
        Qt3DCore::QTransform *transform = nullptr;
        Qt3DRender::QScissorTest *scissor = nullptr;
        Qt3DRender::QParameter *texParam = nullptr;
    };

    struct CmdListEntry {
        Qt3DRender::QBuffer *vbuf = nullptr;
        Qt3DRender::QBuffer *ibuf = nullptr;
        QVector<CmdEntry> cmds;
        int activeSize = 0;
    };

    QVector<CmdListEntry> m_cmdList;
};

#endif
