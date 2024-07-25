#pragma once

#include <array>

namespace dnm
{
struct VertexGizmo
{
    v3 x, y, z;   // Position data
    v3 r, g, b;   // color
};

struct GizmoData
{
    constexpr static u32                        reservedGizmoSpace = 4000;
    std::array<VertexGizmo, reservedGizmoSpace> m_verticesGizmo;
    u32                                         m_occupiedVertexPlaces = 0u;

    void addLines(std::span<VertexGizmo> lineElements) {
        const u32 elementsToCopy = std::min(reservedGizmoSpace - m_occupiedVertexPlaces, static_cast<u32>(lineElements.size()));
        assert(elementsToCopy == lineElements.size());
        std::copy(lineElements.begin(), lineElements.begin() + elementsToCopy, m_verticesGizmo.begin() + m_occupiedVertexPlaces);
        m_occupiedVertexPlaces += elementsToCopy;
    }

    void reset() {
        // No need to do any cleanup here, trivially destructible types don't
        // require destructor calls
        m_occupiedVertexPlaces = 0;
    }
};
}   // namespace dnm
