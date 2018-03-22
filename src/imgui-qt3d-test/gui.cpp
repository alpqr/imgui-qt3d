#include "gui.h"
#include "imguimanager.h"
#include <QPainter>
#include <Qt3DCore/QEntity>
#include <Qt3DRender/QTexture>
#include <Qt3DRender/QPaintedTextureImage>

class TextureContents : public Qt3DRender::QPaintedTextureImage
{
public:
    void paint(QPainter *painter) override {
        const QRect r = QRect(QPoint(0, 0), size());
        painter->fillRect(r, Qt::white);
        painter->setBrush(Qt::green);
        painter->drawEllipse(r.center(), 100, 50);
        painter->setPen(Qt::red);
        painter->drawText(r, Qt::AlignCenter, QLatin1String("Hello from QPainter"));
    }
};

void Gui::frame(Qt3DCore::QEntity *rootEntity)
{
    // 1. Show a simple window
    // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
    {
        static float f = 0.0f;
        ImGui::Text("Hello, world!");
        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
        ImGui::ColorEdit3("clear color", (float*)&clear_color);
        if (ImGui::Button("Test Window")) show_test_window ^= 1;
        if (ImGui::Button("Another Window")) show_another_window ^= 1;
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

        // Add some extra content to exercise our Qt3D integration a bit.
        if (ImGui::Button("Scale up"))
            m_manager->setScale(m_manager->scale() + 0.2f);
        if (ImGui::Button("Scale down"))
            m_manager->setScale(m_manager->scale() - 0.2f);

        const QSize customImageSize(320, 200);
        if (!m_customTexture) {
            // rootEntity's only purpose is to serve as a parent for resources like textures
            m_customTexture = new Qt3DRender::QTexture2D(rootEntity);
            Qt3DRender::QPaintedTextureImage *customTextureContents = new TextureContents;
            customTextureContents->setWidth(customImageSize.width());
            customTextureContents->setHeight(customImageSize.height());
            m_customTexture->addTextureImage(customTextureContents);
        }
        ImGui::Image(m_customTexture, ImVec2(customImageSize.width(), customImageSize.height()));
    }

    // 2. Show another simple window, this time using an explicit Begin/End pair
    if (show_another_window)
    {
        ImGui::Begin("Another Window", &show_another_window);
        ImGui::Text("Hello from another window!");
        ImGui::End();
    }

    // 3. Show the ImGui test window. Most of the sample code is in ImGui::ShowTestWindow()
    if (show_test_window)
    {
        ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver);
        ImGui::ShowTestWindow(&show_test_window);
    }
}
