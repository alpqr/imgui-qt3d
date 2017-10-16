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

void ImguiManager::resizePool(CmdListEntry *e, int count)
{
    Qt3DRender::QLayer *guiTag = m_window->guiTag();
    Qt3DRender::QLayer *activeGuiTag = m_window->activeGuiTag();
    Q_ASSERT(guiTag && activeGuiTag);

    // do not resize when count is smaller
    if (e->cmds.count() < count) {
        e->cmds.resize(count);
        for (int i = 0; i < count; ++i) {
            Qt3DCore::QEntity *entity = new Qt3DCore::QEntity(m_rootEntity);
            entity->addComponent(guiTag);
            entity->addComponent(buildMaterial(&e->cmds[i].scissor));
            Qt3DRender::QGeometryRenderer *geomRenderer = new Qt3DRender::QGeometryRenderer;
            entity->addComponent(geomRenderer);
            e->cmds[i].entity = entity;
            e->cmds[i].geomRenderer = geomRenderer;
        }
    }

    // but make sure only entities from the first 'count' entries are tagged as active gui
    if (e->activeCount > count) {
        for (int i = count; i < e->activeCount; ++i)
            e->cmds[i].entity->removeComponent(activeGuiTag);
    } else if (e->activeCount < count) {
        for (int i = e->activeCount; i < count; ++i)
            e->cmds[i].entity->addComponent(activeGuiTag);
    }

    e->activeCount = count;
}

void ImguiManager::updateGeometry(CmdListEntry *e, int idx, const ImDrawCmd *cmd, int vertexCount, int indexCount, const void *indexOffset)
{
    Qt3DRender::QGeometryRenderer *gr = e->cmds[idx].geomRenderer;
    Qt3DRender::QGeometry *g = gr->geometry();

    if (!g) {
        gr->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);
        g = new Qt3DRender::QGeometry;

        for (int i = 0; i < 4; ++i)
            g->addAttribute(new Qt3DRender::QAttribute);

        const int vsize = 3; // assumes ImDrawVert was overridden in imconfig.h
        Q_ASSERT(sizeof(ImDrawVert) == sizeof(ImVec2) + sizeof(float) + sizeof(ImVec2) + sizeof(ImU32));

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

    gr->setVertexCount(cmd->ElemCount);
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

    for (int n = 0; n < d->CmdListsCount; ++n) {
        const ImDrawList *cmdList = d->CmdLists[n];
        const ImDrawIdx *indexBufOffset = nullptr;
        CmdListEntry *e = &m_cmdList[n];

        if (!e->vbuf) {
            e->vbuf = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::VertexBuffer);
            e->vbuf->setUsage(Qt3DRender::QBuffer::StreamDraw);
        }
        // NB! must make a copy in any case, fromRawData would be wrong here
        // even if we did not need to insert the dummy z values.
        QByteArray vdata((const char *) cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
        // Initialize Z values. The shader does not need it but some Qt3D stuff (bounding volumes etc.) might.
        {
            ImDrawVert *v = (ImDrawVert *) vdata.data();
            int sz = cmdList->VtxBuffer.Size;
            while (sz-- > 0) {
                v->z = 0.0f;
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

        for (int i = 0; i < cmdList->CmdBuffer.Size; ++i) {
            const ImDrawCmd *cmd = &cmdList->CmdBuffer[i];
            if (!cmd->UserCallback) {
                updateGeometry(e, i, cmd, cmdList->VtxBuffer.Size, cmdList->IdxBuffer.Size, indexBufOffset);

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
    }
}

static const char *vertSrc =
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

static const char *fragSrc =
        "uniform sampler2D tex;\n"
        "varying vec2 uv;\n"
        "varying vec4 color;\n"
        "void main() {\n"
        "    gl_FragColor = color * texture2D(tex, uv);\n"
        "}\n";

static Qt3DRender::QShaderProgram *buildShaderProgram()
{
    Qt3DRender::QShaderProgram *prog = new Qt3DRender::QShaderProgram;
    prog->setVertexShaderCode(vertSrc);
    prog->setFragmentShaderCode(fragSrc);
    return prog;
}

Qt3DRender::QMaterial *ImguiManager::buildMaterial(Qt3DRender::QScissorTest **scissor)
{
    Qt3DRender::QMaterial *material = new Qt3DRender::QMaterial;
    Qt3DRender::QEffect *effect = new Qt3DRender::QEffect;

    Qt3DRender::QTechnique *technique = new Qt3DRender::QTechnique;

    // the framegraph is expected to filter for this key in its gui pass
    Qt3DRender::QFilterKey *techniqueFilterKey = new Qt3DRender::QFilterKey;
    techniqueFilterKey->setName(QLatin1String("gui"));
    techniqueFilterKey->setValue(QLatin1String("1"));
    technique->addFilterKey(techniqueFilterKey);

    // ### only a 2.x technique for now, add a 3.2+ core profile one at some point
    technique->graphicsApiFilter()->setApi(Qt3DRender::QGraphicsApiFilter::OpenGL);
    technique->graphicsApiFilter()->setMajorVersion(2);
    technique->graphicsApiFilter()->setMinorVersion(0);

    Qt3DRender::QRenderPass *rpass = new Qt3DRender::QRenderPass;
    if (!m_guiProg)
        m_guiProg = buildShaderProgram();
    rpass->setShaderProgram(m_guiProg);

    Qt3DRender::QParameter *texParam = new Qt3DRender::QParameter;
    texParam->setName(QLatin1String("tex"));
    texParam->setValue(QVariant::fromValue(m_atlasTex));
    rpass->addParameter(texParam);

    Qt3DRender::QDepthTest *depthTest = new Qt3DRender::QDepthTest;
    depthTest->setDepthFunction(Qt3DRender::QDepthTest::Always);
    rpass->addRenderState(depthTest);

    rpass->addRenderState(new Qt3DRender::QNoDepthMask);

    Qt3DRender::QBlendEquation *blendFunc = new Qt3DRender::QBlendEquation;
    Qt3DRender::QBlendEquationArguments *blendArgs = new Qt3DRender::QBlendEquationArguments;
    blendFunc->setBlendFunction(Qt3DRender::QBlendEquation::Add);
    blendArgs->setSourceRgb(Qt3DRender::QBlendEquationArguments::SourceAlpha);
    blendArgs->setDestinationRgb(Qt3DRender::QBlendEquationArguments::OneMinusSourceAlpha);
    blendArgs->setSourceAlpha(Qt3DRender::QBlendEquationArguments::One);
    blendArgs->setDestinationAlpha(Qt3DRender::QBlendEquationArguments::OneMinusSourceAlpha);
    rpass->addRenderState(blendFunc);
    rpass->addRenderState(blendArgs);

    Qt3DRender::QCullFace *cullFace = new Qt3DRender::QCullFace;
    cullFace->setMode(Qt3DRender::QCullFace::NoCulling);
    rpass->addRenderState(cullFace);

    *scissor = new Qt3DRender::QScissorTest;
    rpass->addRenderState(*scissor);

    // ###
    // glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    technique->addRenderPass(rpass);

    effect->addTechnique(technique);
    material->setEffect(effect);

    return material;
}
