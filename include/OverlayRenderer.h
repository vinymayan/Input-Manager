#pragma once
#include "render/d3d_context.h"
#include "render/cbuffer.h"
#include "render/line_drawer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <mutex>

class OverlayRenderer {
public:
    static OverlayRenderer* GetSingleton() {
        static OverlayRenderer singleton;
        return &singleton;
    }

    void Install();
    void Render(Render::D3DContext& ctx);

private:
    OverlayRenderer() = default;
    ~OverlayRenderer() = default;

    bool _bDXHooked = false;

    struct VSMatricesCBuffer {
        glm::mat4 matProjView = glm::identity<glm::mat4>();
        float curTime = 0.0f;
        float pad[3] = { 0.0f, 0.0f, 0.0f };
    };

    struct VSPerObjectCBuffer {
        glm::mat4 model = glm::identity<glm::mat4>();
    };

    VSMatricesCBuffer cbufPerFrameStaging = {};
    VSPerObjectCBuffer cbufPerObjectStaging = {};

    std::shared_ptr<Render::CBuffer> cbufPerFrame;
    std::shared_ptr<Render::CBuffer> cbufPerObject;
    std::unique_ptr<Render::LineDrawer> lineDrawer;
};