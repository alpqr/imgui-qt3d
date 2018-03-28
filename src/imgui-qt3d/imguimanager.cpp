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
#include <imgui.h>

#include <QMouseEvent>
#include <QKeyEvent>
#include <QImage>

#include <QTransform>
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
#include <QColorMask>
#include <QScissorTest>
#include <QFrameAction>

class ImguiTextureImageDataGen : public Qt3DRender::QTextureImageDataGenerator
{
public:
    ImguiTextureImageDataGen(const QImage &image) : m_image(image) { }

    Qt3DRender::QTextureImageDataPtr operator()() override
    {
        Qt3DRender::QTextureImageDataPtr textureData = Qt3DRender::QTextureImageDataPtr::create();
        textureData->setImage(m_image);
        return textureData;
    }
    bool operator==(const Qt3DRender::QTextureImageDataGenerator &other) const override
    {
        const ImguiTextureImageDataGen *otherFunctor = functor_cast<ImguiTextureImageDataGen>(&other);
        return otherFunctor && otherFunctor->m_image == m_image;
    }

    QT3D_FUNCTOR(ImguiTextureImageDataGen)

private:
    QImage m_image;
};

class TextureImage : public Qt3DRender::QAbstractTextureImage
{
public:
    TextureImage(const QImage &image) { m_gen = QSharedPointer<ImguiTextureImageDataGen>::create(image); }

private:
    Qt3DRender::QTextureImageDataGeneratorPtr dataGenerator() const override { return m_gen; }

    Qt3DRender::QTextureImageDataGeneratorPtr m_gen;
};

void ImguiManager::initialize(Qt3DCore::QEntity *rootEntity)
{
    m_rootEntity = rootEntity;

    Qt3DLogic::QFrameAction *frameUpdater = new Qt3DLogic::QFrameAction;
    QObject::connect(frameUpdater, &Qt3DLogic::QFrameAction::triggered, [this]() {
        if (!m_enabled || !m_frame || !m_outputInfoFunc)
            return;

        m_outputInfo = m_outputInfoFunc();

        ImGuiIO &io = ImGui::GetIO();
        io.DisplaySize.x = m_outputInfo.size.width() * m_outputInfo.dpr;
        io.DisplaySize.y = m_outputInfo.size.height() * m_outputInfo.dpr;

        updateInput();

        // One weakness regarding input is that io.WantCaptureMouse/Keyboard
        // cannot really be supported. We could check for them after the
        // NewFrame() call below, but there is nothing we can do at this stage,
        // the Qt events have already been dispatched and processed.

        ImGui::NewFrame();
        m_frame(m_rootEntity);
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

    io.Fonts->SetTexID(m_atlasTex);
}

// To be called when the Qt 3D scene goes down (and thus destroys the objects
// ImguiManager references) but the ImguiManager instance will be reused later
// on. Must be followed by a call to initialize().
void ImguiManager::releaseResources()
{
    // assume that everything starting from our root entity is already gone
    rpd = SharedRenderPassData();
    m_cmdList.clear();
}

void ImguiManager::resizePool(CmdListEntry *e, int newSize)
{
    Q_ASSERT(m_outputInfo.guiTag && m_outputInfo.activeGuiTag);

    const int oldSize = e->cmds.count();
    if (newSize > oldSize) {
        e->cmds.resize(newSize);
        for (int i = oldSize; i < newSize; ++i) {
            CmdEntry *ecmd = &e->cmds[i];
            Qt3DCore::QEntity *entity = new Qt3DCore::QEntity(m_rootEntity);
            entity->addComponent(m_outputInfo.guiTag);
            Qt3DRender::QMaterial *material = buildMaterial(&ecmd->scissor);
            entity->addComponent(material);
            Qt3DRender::QGeometryRenderer *geomRenderer = new Qt3DRender::QGeometryRenderer;
            entity->addComponent(geomRenderer);
            Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
            entity->addComponent(transform);
            Qt3DRender::QParameter *texParam = new Qt3DRender::QParameter;
            texParam->setName(QLatin1String("tex"));
            texParam->setValue(QVariant::fromValue(m_atlasTex)); // needs a valid initial value
            material->addParameter(texParam);
            ecmd->entity = entity;
            ecmd->material = material;
            ecmd->geomRenderer = geomRenderer;
            ecmd->transform = transform;
            ecmd->texParam = texParam;
        }
    }

    // make sure only entities from the first newSize entries are tagged as active gui
    if (e->activeSize > newSize) {
        for (int i = newSize; i < e->activeSize; ++i) {
            e->cmds[i].entity->removeComponent(m_outputInfo.activeGuiTag);
            updateGeometry(e, i, 0, 0, 0, nullptr);
        }
    } else if (e->activeSize < newSize) {
        for (int i = e->activeSize; i < newSize; ++i)
            e->cmds[i].entity->addComponent(m_outputInfo.activeGuiTag);
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

    if (m_cmdList.count() < d->CmdListsCount) {
        m_cmdList.resize(d->CmdListsCount);
    } else {
        // Make sure all unused entities are untagged.
        for (int n = d->CmdListsCount; n < m_cmdList.count(); ++n)
            resizePool(&m_cmdList[n], 0);
    }

    d->ScaleClipRects(ImVec2(m_scale, m_scale));

    // CmdLists is in back-to-front order, assign z values accordingly
    const float zstep = 0.01f;
    float z = -1000;

    for (int n = 0; n < d->CmdListsCount; ++n) {
        const ImDrawList *cmdList = d->CmdLists[n];
        CmdListEntry *e = &m_cmdList[n];

        if (!e->vbuf) {
            e->vbuf = new Qt3DRender::QBuffer;
            e->vbuf->setUsage(Qt3DRender::QBuffer::StreamDraw);
        }
        // NB! must make a copy in any case, fromRawData would be wrong here
        // even if we did not need to insert the dummy z values.
        QByteArray vdata((const char *) cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
        // Initialize Z values. The shader does not need it but some Qt3D stuff (back-to-front sorting, bounding volumes) does.
        {
            ImDrawVert *v = (ImDrawVert *) vdata.data();
            uint idxOffset = 0;
            // Assign a Z value per draw call, not per draw call list.
            // This assumes no vertices are shared between the draw calls.
            for (int i = 0; i < cmdList->CmdBuffer.Size; ++i) {
                const ImDrawCmd *cmd = &cmdList->CmdBuffer[i];
                if (!cmd->UserCallback) {
                    for (uint ei = 0; ei < cmd->ElemCount; ++ei) {
                        ImDrawIdx idx = cmdList->IdxBuffer.Data[idxOffset + ei];
                        v[idx].z = z;
                    }
                }
                idxOffset += cmd->ElemCount;
                z += zstep;
            }
        }
        e->vbuf->setData(vdata);

        if (!e->ibuf) {
            e->ibuf = new Qt3DRender::QBuffer;
            e->ibuf->setUsage(Qt3DRender::QBuffer::StreamDraw);
        }
        // same here
        e->ibuf->setData(QByteArray((const char *) cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx)));

        // Ensure the needed number of entities and components are available; tag/untag as necessary
        resizePool(e, cmdList->CmdBuffer.Size);

        const ImDrawIdx *indexBufOffset = nullptr;
        for (int i = 0; i < cmdList->CmdBuffer.Size; ++i) {
            const ImDrawCmd *cmd = &cmdList->CmdBuffer[i];
            CmdEntry *ecmd = &e->cmds[i];
            if (!cmd->UserCallback) {
                updateGeometry(e, i, cmd->ElemCount, cmdList->VtxBuffer.Size, cmdList->IdxBuffer.Size, indexBufOffset);
                ecmd->texParam->setValue(QVariant::fromValue(static_cast<Qt3DRender::QAbstractTexture *>(cmd->TextureId)));

                Qt3DRender::QScissorTest *scissor = ecmd->scissor;
                scissor->setLeft(cmd->ClipRect.x);
                scissor->setBottom(io.DisplaySize.y - cmd->ClipRect.w);
                scissor->setWidth(cmd->ClipRect.z - cmd->ClipRect.x);
                scissor->setHeight(cmd->ClipRect.w - cmd->ClipRect.y);

                Qt3DCore::QTransform *transform = ecmd->transform;
                transform->setScale(m_scale);
            } else {
                cmd->UserCallback(cmdList, cmd);
            }
            indexBufOffset += cmd->ElemCount;
        }
    }
}

static const char *vertSrcES2 =
        "attribute vec4 vertexPosition;\n"
        "attribute vec4 vertexColor;\n"
        "attribute vec2 vertexTexCoord;\n"
        "varying vec2 uv;\n"
        "varying vec4 color;\n"
        "uniform mat4 projectionMatrix;\n"
        "uniform mat4 modelMatrix;\n"
        "void main() {\n"
        "    uv = vertexTexCoord;\n"
        "    color = vertexColor;\n"
        "    gl_Position = projectionMatrix * modelMatrix * vec4(vertexPosition.xy, 0.0, 1.0);\n"
        "}\n";

static const char *fragSrcES2 =
        "uniform sampler2D tex;\n"
        "varying highp vec2 uv;\n"
        "varying highp vec4 color;\n"
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
        "uniform mat4 modelMatrix;\n"
        "void main() {\n"
        "    uv = vertexTexCoord;\n"
        "    color = vertexColor;\n"
        "    gl_Position = projectionMatrix * modelMatrix * vec4(vertexPosition.xy, 0.0, 1.0);\n"
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

    if (!rpd.valid) {
        rpd.valid = true;

        rpd.progES2 = buildShaderProgram(vertSrcES2, fragSrcES2);
        rpd.progGL3 = buildShaderProgram(vertSrcGL3, fragSrcGL3);

        // the framegraph is expected to filter for this key in its gui pass
        rpd.techniqueFilterKey = m_outputInfo.guiTechniqueFilterKey;

        rpd.depthTest = new Qt3DRender::QDepthTest;
        rpd.depthTest->setDepthFunction(Qt3DRender::QDepthTest::Always);

        rpd.noDepthWrite = new Qt3DRender::QNoDepthMask;

        rpd.blendFunc = new Qt3DRender::QBlendEquation;
        rpd.blendArgs = new Qt3DRender::QBlendEquationArguments;
        rpd.blendFunc->setBlendFunction(Qt3DRender::QBlendEquation::Add);
        rpd.blendArgs->setSourceRgb(Qt3DRender::QBlendEquationArguments::SourceAlpha);
        rpd.blendArgs->setDestinationRgb(Qt3DRender::QBlendEquationArguments::OneMinusSourceAlpha);
        rpd.blendArgs->setSourceAlpha(Qt3DRender::QBlendEquationArguments::OneMinusSourceAlpha);
        rpd.blendArgs->setDestinationAlpha(Qt3DRender::QBlendEquationArguments::Zero);

        rpd.cullFace = new Qt3DRender::QCullFace;
        rpd.cullFace->setMode(Qt3DRender::QCullFace::NoCulling);

        rpd.colorMask = new Qt3DRender::QColorMask;
        rpd.colorMask->setAlphaMasked(false);
    }

    *scissor = new Qt3DRender::QScissorTest;

    // have two techniques: one for OpenGL ES (2.0+) and one for OpenGL core (3.2+)

    auto buildRenderPass = [this, scissor](Qt3DRender::QShaderProgram *prog) {
        Qt3DRender::QRenderPass *rpass = new Qt3DRender::QRenderPass;
        rpass->setShaderProgram(prog);

        rpass->addRenderState(rpd.depthTest);
        rpass->addRenderState(rpd.noDepthWrite);
        rpass->addRenderState(rpd.blendFunc);
        rpass->addRenderState(rpd.blendArgs);
        rpass->addRenderState(rpd.cullFace);
        rpass->addRenderState(rpd.colorMask);
        rpass->addRenderState(*scissor);

        // Our setEnabled() maps to QNode::setEnabled() on the QRenderPass
        // hence the need for keeping track. This is simpler than playing with
        // QLayer on entities.
        rpd.enabledToggle.append(rpass);

        return rpass;
    };

    Qt3DRender::QTechnique *techniqueES2 = new Qt3DRender::QTechnique;
    techniqueES2->addFilterKey(rpd.techniqueFilterKey);
    Qt3DRender::QGraphicsApiFilter *apiFilterES2 = techniqueES2->graphicsApiFilter();
    apiFilterES2->setApi(Qt3DRender::QGraphicsApiFilter::OpenGLES);
    apiFilterES2->setMajorVersion(2);
    apiFilterES2->setMinorVersion(0);
    apiFilterES2->setProfile(Qt3DRender::QGraphicsApiFilter::NoProfile);
    techniqueES2->addRenderPass(buildRenderPass(rpd.progES2));

    Qt3DRender::QTechnique *techniqueGL3 = new Qt3DRender::QTechnique;
    techniqueGL3->addFilterKey(rpd.techniqueFilterKey);
    Qt3DRender::QGraphicsApiFilter *apiFilterGL3 = techniqueGL3->graphicsApiFilter();
    apiFilterGL3->setApi(Qt3DRender::QGraphicsApiFilter::OpenGL);
    apiFilterGL3->setMajorVersion(3);
    apiFilterGL3->setMinorVersion(2);
    apiFilterGL3->setProfile(Qt3DRender::QGraphicsApiFilter::CoreProfile);
    techniqueGL3->addRenderPass(buildRenderPass(rpd.progGL3));

    effect->addTechnique(techniqueES2);
    effect->addTechnique(techniqueGL3);

    material->setEffect(effect);

    return material;
}

#define FIRSTSPECKEY (0x01000000)
#define LASTSPECKEY (0x01000017)
#define MAPSPECKEY(k) ((k) - FIRSTSPECKEY + 256)

// Do not bother with 3D picking, assume the UI is displayed 1:1 in the window.
class ImguiInputEventFilter : public QObject
{
public:
    ImguiInputEventFilter()
    {
        memset(keyDown, 0, sizeof(keyDown));
    }

    bool eventFilter(QObject *watched, QEvent *event) override;

    QPointF mousePos;
    Qt::MouseButtons mouseButtonsDown = Qt::NoButton;
    float mouseWheel = 0;
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    bool keyDown[256 + (LASTSPECKEY - FIRSTSPECKEY + 1)];
    QString keyText;
    bool enabled = true;
};

bool ImguiInputEventFilter::eventFilter(QObject *, QEvent *event)
{
    if (!enabled)
        return false;

    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease:
    {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        mousePos = me->pos();
        mouseButtonsDown = me->buttons();
        modifiers = me->modifiers();
    }
        break;

    case QEvent::Wheel:
    {
        QWheelEvent *we = static_cast<QWheelEvent *>(event);
        mouseWheel += we->angleDelta().y() / 120.0f;
    }
        break;

    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    {
        const bool down = event->type() == QEvent::KeyPress;
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        modifiers = ke->modifiers();
        if (down)
            keyText.append(ke->text());
        int k = ke->key();
        if (k <= 0xFF)
            keyDown[k] = down;
        else if (k >= FIRSTSPECKEY && k <= LASTSPECKEY)
            keyDown[MAPSPECKEY(k)] = down;
    }
        break;

    default:
        break;
    }

    return false;
}

ImguiManager::~ImguiManager()
{
    delete m_inputEventFilter;
}

void ImguiManager::setInputEventSource(QObject *src)
{
    if (m_inputEventSource && m_inputEventFilter)
        m_inputEventSource->removeEventFilter(m_inputEventFilter);

    m_inputEventSource = src;

    if (!m_inputEventFilter)
        m_inputEventFilter = new ImguiInputEventFilter;

    m_inputEventSource->installEventFilter(m_inputEventFilter);
}

void ImguiManager::updateInput()
{
    if (!m_inputEventFilter)
        return;

    ImGuiIO &io = ImGui::GetIO();

    if (!m_inputInitialized) {
        m_inputInitialized = true;

        io.KeyMap[ImGuiKey_Tab] = MAPSPECKEY(Qt::Key_Tab);
        io.KeyMap[ImGuiKey_LeftArrow] = MAPSPECKEY(Qt::Key_Left);
        io.KeyMap[ImGuiKey_RightArrow] = MAPSPECKEY(Qt::Key_Right);
        io.KeyMap[ImGuiKey_UpArrow] = MAPSPECKEY(Qt::Key_Up);
        io.KeyMap[ImGuiKey_DownArrow] = MAPSPECKEY(Qt::Key_Down);
        io.KeyMap[ImGuiKey_PageUp] = MAPSPECKEY(Qt::Key_PageUp);
        io.KeyMap[ImGuiKey_PageDown] = MAPSPECKEY(Qt::Key_PageDown);
        io.KeyMap[ImGuiKey_Home] = MAPSPECKEY(Qt::Key_Home);
        io.KeyMap[ImGuiKey_End] = MAPSPECKEY(Qt::Key_End);
        io.KeyMap[ImGuiKey_Delete] = MAPSPECKEY(Qt::Key_Delete);
        io.KeyMap[ImGuiKey_Backspace] = MAPSPECKEY(Qt::Key_Backspace);
        io.KeyMap[ImGuiKey_Enter] = MAPSPECKEY(Qt::Key_Return);
        io.KeyMap[ImGuiKey_Escape] = MAPSPECKEY(Qt::Key_Escape);

        io.KeyMap[ImGuiKey_A] = Qt::Key_A;
        io.KeyMap[ImGuiKey_C] = Qt::Key_C;
        io.KeyMap[ImGuiKey_V] = Qt::Key_V;
        io.KeyMap[ImGuiKey_X] = Qt::Key_X;
        io.KeyMap[ImGuiKey_Y] = Qt::Key_Y;
        io.KeyMap[ImGuiKey_Z] = Qt::Key_Z;
    }

    ImguiInputEventFilter *w = m_inputEventFilter;

    io.MousePos = ImVec2((w->mousePos.x() / m_scale) * m_outputInfo.dpr, (w->mousePos.y() / m_scale) * m_outputInfo.dpr);

    io.MouseDown[0] = w->mouseButtonsDown.testFlag(Qt::LeftButton);
    io.MouseDown[1] = w->mouseButtonsDown.testFlag(Qt::RightButton);
    io.MouseDown[2] = w->mouseButtonsDown.testFlag(Qt::MiddleButton);

    io.MouseWheel = w->mouseWheel;
    w->mouseWheel = 0;

    io.KeyCtrl = w->modifiers.testFlag(Qt::ControlModifier);
    io.KeyShift = w->modifiers.testFlag(Qt::ShiftModifier);
    io.KeyAlt = w->modifiers.testFlag(Qt::AltModifier);
    io.KeySuper = w->modifiers.testFlag(Qt::MetaModifier);

    memcpy(io.KeysDown, w->keyDown, sizeof(w->keyDown));

    if (!w->keyText.isEmpty()) {
        for (const QChar &c : w->keyText) {
            ImWchar u = c.unicode();
            if (u)
                io.AddInputCharacter(u);
        }
        w->keyText.clear();
    }
}

void ImguiManager::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;

    for (Qt3DCore::QNode *n : rpd.enabledToggle)
        n->setEnabled(m_enabled);

    if (m_inputEventFilter)
        m_inputEventFilter->enabled = m_enabled;
}

void ImguiManager::setScale(float scale)
{
    m_scale = scale;
}
