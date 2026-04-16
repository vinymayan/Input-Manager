#include "OverlayRenderer.h"
#include "render/common.h"
#include "render/timer.h"
#include "Events.h"     
#include "Settings.h"   
#include "logger.h"

void OverlayRenderer::Install() {
    if (!_bDXHooked) {
        Render::InstallHooks();
        if (Render::HasContext()) {
            _bDXHooked = true;

            auto& ctx = Render::GetContext();

            // Setup Buffers
            Render::CBufferCreateInfo perObj;
            perObj.bufferUsage = D3D11_USAGE::D3D11_USAGE_DYNAMIC;
            perObj.cpuAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_WRITE;
            perObj.size = sizeof(VSPerObjectCBuffer);
            perObj.initialData = &cbufPerObjectStaging;
            cbufPerObject = std::make_shared<Render::CBuffer>(perObj, ctx);

            Render::CBufferCreateInfo perFrame;
            perFrame.bufferUsage = D3D11_USAGE::D3D11_USAGE_DYNAMIC;
            perFrame.cpuAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_WRITE;
            perFrame.size = sizeof(VSMatricesCBuffer);
            perFrame.initialData = &cbufPerFrameStaging;
            cbufPerFrame = std::make_shared<Render::CBuffer>(perFrame, ctx);

            lineDrawer = std::make_unique<Render::LineDrawer>(ctx);

            // Registra a função Render para ser chamada a cada frame do jogo
            Render::OnPresent(std::bind(&OverlayRenderer::Render, this, std::placeholders::_1));
            
            logger::info("[OverlayRenderer] DirectX 11 Hookado com sucesso para Gestos!");
        } else {
            logger::error("[OverlayRenderer] Falha ao injetar no DirectX.");
        }
    }
}

void OverlayRenderer::Render(Render::D3DContext& ctx) {
    // 1. Atualiza os buffers (Como é 2D na tela toda, usamos Identity Matrix)
    cbufPerFrameStaging.matProjView = glm::identity<glm::mat4>();
    cbufPerFrame->Update(&cbufPerFrameStaging, 0, sizeof(VSMatricesCBuffer), ctx);

    cbufPerFrame->Bind(Render::PipelineStage::Vertex, 1, ctx);
    cbufPerFrame->Bind(Render::PipelineStage::Fragment, 1, ctx);

    // 2. Setup do D3D11 para desenhar por cima de tudo (Z-Buffer ignorado)
    Render::SetDepthState(ctx, true, true, D3D11_COMPARISON_FUNC::D3D11_COMPARISON_ALWAYS);
    Render::SetBlendState(ctx, true,
        D3D11_BLEND_OP::D3D11_BLEND_OP_ADD, D3D11_BLEND_OP::D3D11_BLEND_OP_ADD,
        D3D11_BLEND::D3D11_BLEND_SRC_ALPHA, D3D11_BLEND::D3D11_BLEND_INV_SRC_ALPHA,
        D3D11_BLEND::D3D11_BLEND_ONE, D3D11_BLEND::D3D11_BLEND_INV_SRC_ALPHA);

    // 3. Captura os pontos do gesto e constrói as linhas
    Render::LineList linesToDraw;

    if (ActionMenuUI::showGestureTrail) { // Verifica a Checkbox do Menu
        auto keyManager = PluginLogic::KeyManager::GetSingleton();
        
        if (keyManager->IsDrawingGesture()) {
            auto path = keyManager->GetActiveGesturePathCopy();
            
            if (path.size() >= 2) {
                float width = ctx.windowSize.x;
                float height = ctx.windowSize.y;
                float centerX = width * 0.5f;
                float centerY = height * 0.5f;

                glm::vec4 color = { 0.0f, 1.0f, 1.0f, 0.8f }; // Ciano com transparência
                double timeStamp = 0.0; 

                for (size_t i = 1; i < path.size(); ++i) {
                    // Centraliza os offsets virtuais na tela
                    float p1x = centerX + path[i - 1].x;
                    float p1y = centerY + path[i - 1].y;
                    float p2x = centerX + path[i].x;
                    float p2y = centerY + path[i].y;

                    // Converte Pixels da Tela para o Padrão NDC do DirectX (-1.0 a 1.0)
                    glm::vec3 startNDC { (p1x / width) * 2.0f - 1.0f, 1.0f - (p1y / height) * 2.0f, 0.0f };
                    glm::vec3 endNDC   { (p2x / width) * 2.0f - 1.0f, 1.0f - (p2y / height) * 2.0f, 0.0f };

                    // duration 0.0f significa que a linha só existe neste frame
                    float duration = 0.0f; 
                    
                    Render::Line line(Render::Point(startNDC, color), Render::Point(endNDC, color), timeStamp, duration);
                    linesToDraw.push_back(line);
                }
            }
        }
    }

    // 4. Submete para a GPU desenhar
    if (!linesToDraw.empty()) {
        lineDrawer->Submit(linesToDraw);
    }
}