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

#include "imguimanager.h"
#include "imguiqt3dwindow.h"
#include <imgui.h>

#include <QMouseEvent>

#include <QImage>
#include <QTexture>
#include <QAbstractTextureImage>
#include <QTextureImageDataGenerator>
#include <QBuffer>
#include <QMaterial>
#include <QEffect>
#include <QTechnique>
#include <QRenderPass>
#include <QShaderProgram>
#include <QFilterKey>
#include <QTechniqueFilter>
#include <QGraphicsApiFilter>
#include <QLayer>
#include <QParameter>
#include <QGeometryRenderer>
#include <QGeometry>
#include <QAttribute>
#include <QDepthTest>
#include <QBlendEquation>
#include <QBlendEquationArguments>
#include <QNoDepthMask>
#include <QCullFace>
#include <QScissorTest>
#include <QFrameAction>

class TextureImageDataGen : public Qt3DRender::QTextureImageDataGenerator
{
public:
    TextureImageDataGen(const QImage &image) : m_image(image) { }

    Qt3DRender::QTextureImageDataPtr operator()()
    {
        Qt3DRender::QTextureImageDataPtr textureData = Qt3DRender::QTextureImageDataPtr::create();
        textureData->setImage(m_image);
        return textureData;
    }
    bool operator==(const Qt3DRender::QTextureImageDataGenerator &other) const
    {
        const TextureImageDataGen *otherFunctor = functor_cast<TextureImageDataGen>(&other);
        return otherFunctor && otherFunctor->m_image == m_image;
    }

    QT3D_FUNCTOR(TextureImageDataGen)

private:
    QImage m_image;
};

class TextureImage : public Qt3DRender::QAbstractTextureImage
{
public:
    TextureImage(const QImage &image) { m_gen = QSharedPointer<TextureImageDataGen>::create(image); }

private:
    Qt3DRender::QTextureImageDataGeneratorPtr dataGenerator() const override { return m_gen; }

    Qt3DRender::QTextureImageDataGeneratorPtr m_gen;
};

void ImguiManager::initialize(Qt3DCore::QEntity *rootEntity)
{
    m_rootEntity = rootEntity;

    Qt3DLogic::QFrameAction *frameUpdater = new Qt3DLogic::QFrameAction;
    QObject::connect(frameUpdater, &Qt3DLogic::QFrameAction::triggered, [this]() {
        if (!m_frame)
            return;
        if (m_window) {
            const QSize size = m_window->size() * m_window->devicePixelRatio();
            ImGuiIO &io = ImGui::GetIO();
            io.DisplaySize.x = size.width();
            io.DisplaySize.y = size.height();
        }
        updateInput();
        ImGui::NewFrame();
        m_frame();
        ImGui::Render();
        update3D();
    });

    m_rootEntity->addComponent(frameUpdater);

    ImGuiIO &io = ImGui::GetIO();
    unsigned char *pixels;
    int w, h;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);

    m_atlasTex = new Qt3DRender::QTexture2D;
    m_atlasTex->setFormat(Qt3DRender::QAbstractTexture::RGBA8_UNorm);
    m_atlasTex->setWidth(w);
    m_atlasTex->setHeight(h);
    m_atlasTex->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
    m_atlasTex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);

    QImage wrapperImg((const uchar *) pixels, w, h, QImage::Format_RGBA8888);
    m_atlasTex->addTextureImage(new TextureImage(wrapperImg));
}

void ImguiManager::resizePool(CmdListEntry *e, int newSize)
{
    Qt3DRender::QLayer *guiTag = m_window->guiTag();
    Qt3DRender::QLayer *activeGuiTag = m_window->activeGuiTag();
    Q_ASSERT(guiTag && activeGuiTag);

    const int oldSize = e->cmds.count();
    if (newSize > oldSize) {
        e->cmds.resize(newSize);
        for (int i = oldSize; i < newSize; ++i) {
            Qt3DCore::QEntity *entity = new Qt3DCore::QEntity(m_rootEntity);
            entity->addComponent(guiTag);
            entity->addComponent(buildMaterial(&e->cmds[i].scissor));
            Qt3DRender::QGeometryRenderer *geomRenderer = new Qt3DRender::QGeometryRenderer;
            entity->addComponent(geomRenderer);
            e->cmds[i].entity = entity;
            e->cmds[i].geomRenderer = geomRenderer;
        }
    }

    // make sure only entities from the first newSize entries are tagged as active gui
    if (e->activeSize > newSize) {
        for (int i = newSize; i < e->activeSize; ++i) {
            e->cmds[i].entity->removeComponent(activeGuiTag);
            updateGeometry(e, i, 0, 0, 0, nullptr);
        }
    } else if (e->activeSize < newSize) {
        for (int i = e->activeSize; i < newSize; ++i)
            e->cmds[i].entity->addComponent(activeGuiTag);
        // up to the caller to do updateGeometry for [0..newSize-1]
    }

    e->activeSize = newSize;
}

void ImguiManager::updateGeometry(CmdListEntry *e, int idx, uint elemCount, int vertexCount, int indexCount, const void *indexOffset)
{
    Qt3DRender::QGeometryRenderer *gr = e->cmds[idx].geomRenderer;
    Qt3DRender::QGeometry *g = gr->geometry();

    if (!g) {
        gr->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);
        g = new Qt3DRender::QGeometry;

        for (int i = 0; i < 4; ++i)
            g->addAttribute(new Qt3DRender::QAttribute);

        const int vsize = 3; // assumes ImDrawVert was overridden in imconfig.h
        Q_ASSERT(sizeof(ImDrawVert) == (vsize + 2) * sizeof(float) + sizeof(ImU32));

        const QVector<Qt3DRender::QAttribute *> attrs = g->attributes();
        Qt3DRender::QAttribute *attr = attrs[0];
        attr->setBuffer(e->vbuf);
        attr->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());
        attr->setVertexBaseType(Qt3DRender::QAttribute::Float);
        attr->setVertexSize(vsize);
        attr->setCount(vertexCount);
        attr->setByteOffset(0);
        attr->setByteStride(sizeof(ImDrawVert));

        attr = attrs[1];
        attr->setBuffer(e->vbuf);
        attr->setName(Qt3DRender::QAttribute::defaultTextureCoordinateAttributeName());
        attr->setVertexBaseType(Qt3DRender::QAttribute::Float);
        attr->setVertexSize(2);
        attr->setCount(vertexCount);
        attr->setByteOffset(vsize * sizeof(float));
        attr->setByteStride(sizeof(ImDrawVert));

        attr = attrs[2];
        attr->setBuffer(e->vbuf);
        attr->setName(Qt3DRender::QAttribute::defaultColorAttributeName());
        attr->setVertexBaseType(Qt3DRender::QAttribute::UnsignedByte);
        attr->setVertexSize(4);
        attr->setCount(vertexCount);
        attr->setByteOffset((vsize + 2) * sizeof(float));
        attr->setByteStride(sizeof(ImDrawVert));

        attr = attrs[3];
        attr->setBuffer(e->ibuf);
        attr->setAttributeType(Qt3DRender::QAttribute::IndexAttribute);
        attr->setVertexBaseType(sizeof(ImDrawIdx) == 2 ? Qt3DRender::QAttribute::UnsignedShort : Qt3DRender::QAttribute::UnsignedInt);
        attr->setVertexSize(1);
        attr->setCount(indexCount);
        attr->setByteOffset((quintptr) indexOffset);
        attr->setByteStride(0);

        gr->setGeometry(g);
    } else {
        // only update the potentially changing properties afterwards
        const QVector<Qt3DRender::QAttribute *> attrs = g->attributes();
        Q_ASSERT(attrs.count() == 4);

        Qt3DRender::QAttribute *attr = attrs[0];
        attr->setBuffer(e->vbuf);
        attr->setCount(vertexCount);

        attr = attrs[1];
        attr->setBuffer(e->vbuf);
        attr->setCount(vertexCount);

        attr = attrs[2];
        attr->setBuffer(e->vbuf);
        attr->setCount(vertexCount);

        attr = attrs[3];
        attr->setBuffer(e->ibuf);
        attr->setCount(indexCount);
        attr->setByteOffset((quintptr) indexOffset);
    }

    gr->setVertexCount(elemCount);
}

// called once per frame, performs gui-related updates for the scene
void ImguiManager::update3D()
{
    ImDrawData *d = ImGui::GetDrawData();
    ImGuiIO &io = ImGui::GetIO();

    // ### d->ScaleClipRects

    if (m_cmdList.count() < d->CmdListsCount) {
        m_cmdList.resize(d->CmdListsCount);
    } else {
        // Make sure all unused entities are untagged.
        for (int n = d->CmdListsCount; n < m_cmdList.count(); ++n)
            resizePool(&m_cmdList[n], 0);
    }

    float z = 1.0f;
    for (int n = 0; n < d->CmdListsCount; ++n) {
        const ImDrawList *cmdList = d->CmdLists[n];
        CmdListEntry *e = &m_cmdList[n];

        if (!e->vbuf) {
            e->vbuf = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::VertexBuffer);
            e->vbuf->setUsage(Qt3DRender::QBuffer::StreamDraw);
        }
        // NB! must make a copy in any case, fromRawData would be wrong here
        // even if we did not need to insert the dummy z values.
        QByteArray vdata((const char *) cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
        // Initialize Z values. The shader does not need it but some Qt3D stuff (back-to-front sorting, bounding volumes) does.
        {
            ImDrawVert *v = (ImDrawVert *) vdata.data();
            int sz = cmdList->VtxBuffer.Size;
            while (sz-- > 0) {
                v->z = z;
                ++v;
            }
        }
        e->vbuf->setData(vdata);

        if (!e->ibuf) {
            e->ibuf = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::IndexBuffer);
            e->ibuf->setUsage(Qt3DRender::QBuffer::StreamDraw);
        }
        // same here
        e->ibuf->setData(QByteArray((const char *) cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx)));

        // Ensure the needed number of entities and components are available; tag/untag as necessary
        resizePool(e, cmdList->CmdBuffer.Size);

        const ImDrawIdx *indexBufOffset = nullptr;
        for (int i = 0; i < cmdList->CmdBuffer.Size; ++i) {
            const ImDrawCmd *cmd = &cmdList->CmdBuffer[i];
            if (!cmd->UserCallback) {
                updateGeometry(e, i, cmd->ElemCount, cmdList->VtxBuffer.Size, cmdList->IdxBuffer.Size, indexBufOffset);

                Qt3DRender::QScissorTest *scissor = e->cmds[i].scissor;
                scissor->setLeft(cmd->ClipRect.x);
                scissor->setBottom(io.DisplaySize.y - cmd->ClipRect.w);
                scissor->setWidth(cmd->ClipRect.z - cmd->ClipRect.x);
                scissor->setHeight(cmd->ClipRect.w - cmd->ClipRect.y);
            } else {
                cmd->UserCallback(cmdList, cmd);
            }
            indexBufOffset += cmd->ElemCount;
        }

        z -= 0.01f;
    }
}

static const char *vertSrcES2 =
        "attribute vec4 vertexPosition;\n"
        "attribute vec4 vertexColor;\n"
        "attribute vec2 vertexTexCoord;\n"
        "varying vec2 uv;\n"
        "varying vec4 color;\n"
        "uniform mat4 projectionMatrix;\n"
        "void main() {\n"
        "    uv = vertexTexCoord;\n"
        "    color = vertexColor;\n"
        "    gl_Position = projectionMatrix * vec4(vertexPosition.xy, 0.0, 1.0);\n"
        "}\n";

static const char *fragSrcES2 =
        "uniform sampler2D tex;\n"
        "varying vec2 uv;\n"
        "varying vec4 color;\n"
        "void main() {\n"
        "    gl_FragColor = color * texture2D(tex, uv);\n"
        "}\n";

static const char *vertSrcGL3 =
        "#version 150\n"
        "in vec4 vertexPosition;\n"
        "in vec4 vertexColor;\n"
        "in vec2 vertexTexCoord;\n"
        "out vec2 uv;\n"
        "out vec4 color;\n"
        "uniform mat4 projectionMatrix;\n"
        "void main() {\n"
        "    uv = vertexTexCoord;\n"
        "    color = vertexColor;\n"
        "    gl_Position = projectionMatrix * vec4(vertexPosition.xy, 0.0, 1.0);\n"
        "}\n";

static const char *fragSrcGL3 =
        "#version 150\n"
        "uniform sampler2D tex;\n"
        "in vec2 uv;\n"
        "in vec4 color;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = color * texture(tex, uv);\n"
        "}\n";

Qt3DRender::QMaterial *ImguiManager::buildMaterial(Qt3DRender::QScissorTest **scissor)
{
    Qt3DRender::QMaterial *material = new Qt3DRender::QMaterial;
    Qt3DRender::QEffect *effect = new Qt3DRender::QEffect;

    auto buildShaderProgram = [](const char *vs, const char *fs) {
        Qt3DRender::QShaderProgram *prog = new Qt3DRender::QShaderProgram;
        prog->setVertexShaderCode(vs);
        prog->setFragmentShaderCode(fs);
        return prog;
    };

    if (!q3d.valid) {
        q3d.valid = true;

        q3d.progES2 = buildShaderProgram(vertSrcES2, fragSrcES2);
        q3d.progGL3 = buildShaderProgram(vertSrcGL3, fragSrcGL3);

        // the framegraph is expected to filter for this key in its gui pass
        q3d.techniqueFilterKey = new Qt3DRender::QFilterKey;
        q3d.techniqueFilterKey->setName(QLatin1String("gui"));
        q3d.techniqueFilterKey->setValue(QLatin1String("1"));

        q3d.texParam = new Qt3DRender::QParameter;
        q3d.texParam->setName(QLatin1String("tex"));
        q3d.texParam->setValue(QVariant::fromValue(m_atlasTex));

        q3d.depthTest = new Qt3DRender::QDepthTest;
        q3d.depthTest->setDepthFunction(Qt3DRender::QDepthTest::Always);

        q3d.noDepthWrite = new Qt3DRender::QNoDepthMask;

        q3d.blendFunc = new Qt3DRender::QBlendEquation;
        q3d.blendArgs = new Qt3DRender::QBlendEquationArguments;
        q3d.blendFunc->setBlendFunction(Qt3DRender::QBlendEquation::Add);
        q3d.blendArgs->setSourceRgb(Qt3DRender::QBlendEquationArguments::SourceAlpha);
        q3d.blendArgs->setDestinationRgb(Qt3DRender::QBlendEquationArguments::OneMinusSourceAlpha);
        q3d.blendArgs->setSourceAlpha(Qt3DRender::QBlendEquationArguments::OneMinusSourceAlpha);
        q3d.blendArgs->setDestinationAlpha(Qt3DRender::QBlendEquationArguments::Zero);

        q3d.cullFace = new Qt3DRender::QCullFace;
        q3d.cullFace->setMode(Qt3DRender::QCullFace::NoCulling);
    }

    *scissor = new Qt3DRender::QScissorTest;

    // have two techniques: one for OpenGL ES (2.0+) and one for OpenGL core (3.2+)

    auto buildRenderPass = [=](Qt3DRender::QShaderProgram *prog) {
        Qt3DRender::QRenderPass *rpass = new Qt3DRender::QRenderPass;
        rpass->setShaderProgram(prog);

        rpass->addParameter(q3d.texParam);
        rpass->addRenderState(q3d.depthTest);
        rpass->addRenderState(q3d.noDepthWrite);
        rpass->addRenderState(q3d.blendFunc);
        rpass->addRenderState(q3d.blendArgs);
        rpass->addRenderState(q3d.cullFace);
        rpass->addRenderState(*scissor);

        // ###
        // glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        return rpass;
    };

    Qt3DRender::QTechnique *techniqueES2 = new Qt3DRender::QTechnique;
    techniqueES2->addFilterKey(q3d.techniqueFilterKey);
    Qt3DRender::QGraphicsApiFilter *apiFilterES2 = techniqueES2->graphicsApiFilter();
    apiFilterES2->setApi(Qt3DRender::QGraphicsApiFilter::OpenGLES);
    apiFilterES2->setMajorVersion(2);
    apiFilterES2->setMinorVersion(0);
    apiFilterES2->setProfile(Qt3DRender::QGraphicsApiFilter::NoProfile);
    techniqueES2->addRenderPass(buildRenderPass(q3d.progES2));

    Qt3DRender::QTechnique *techniqueGL3 = new Qt3DRender::QTechnique;
    techniqueGL3->addFilterKey(q3d.techniqueFilterKey);
    Qt3DRender::QGraphicsApiFilter *apiFilterGL3 = techniqueGL3->graphicsApiFilter();
    apiFilterGL3->setApi(Qt3DRender::QGraphicsApiFilter::OpenGL);
    apiFilterGL3->setMajorVersion(3);
    apiFilterGL3->setMinorVersion(2);
    apiFilterGL3->setProfile(Qt3DRender::QGraphicsApiFilter::CoreProfile);
    techniqueGL3->addRenderPass(buildRenderPass(q3d.progGL3));

    effect->addTechnique(techniqueES2);
    effect->addTechnique(techniqueGL3);

    material->setEffect(effect);

    return material;
}

// Do not bother with 3D picking, assume the UI is displayed 1:1 in the window.

class ImguiWindowEventFilter : public QObject
{
public:
    bool eventFilter(QObject *watched, QEvent *event) override;

    QPointF mousePos;
    Qt::MouseButtons mouseButtonsDown = Qt::NoButton;
};

bool ImguiWindowEventFilter::eventFilter(QObject *, QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease:
    {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        mousePos = me->windowPos();
        mouseButtonsDown = me->buttons();
    }
        break;

    default:
        break;
    }

    return false;
}

ImguiManager::~ImguiManager()
{
    delete m_windowEventFilter;
}

void ImguiManager::setWindow(ImguiQt3DWindow *window)
{
    if (m_window && m_windowEventFilter)
        m_window->removeEventFilter(m_windowEventFilter);

    m_window = window;

    if (!m_windowEventFilter)
        m_windowEventFilter = new ImguiWindowEventFilter;

    m_window->installEventFilter(m_windowEventFilter);
}

void ImguiManager::updateInput()
{
    ImGuiIO &io = ImGui::GetIO();
    ImguiWindowEventFilter *w = m_windowEventFilter;
    io.MousePos = ImVec2(w->mousePos.x(), w->mousePos.y());
    io.MouseDown[0] = w->mouseButtonsDown.testFlag(Qt::LeftButton);
    io.MouseDown[1] = w->mouseButtonsDown.testFlag(Qt::RightButton);
    io.MouseDown[2] = w->mouseButtonsDown.testFlag(Qt::MiddleButton);
}
