#include "PolyDataSurface.hpp"

#include <imcxx/Widgets.hpp>

namespace libcube::UI {

using namespace librii;

void drawProperty(kpi::PropertyDelegate<libcube::IndexedPolygon>& dl,
                  PolyDataSurface) {
  // ImGui::PushStyleVar(ImGuiStyleVar_TableAngledHeadersAngle, 15.0f * 2.0f * 3.1415926535 / 360.0f);
  // Defer defer([&]() { ImGui::PopStyleVar(); });

  auto& poly = dl.getActive();
  auto& desc = poly.getVcd();
  auto& mesh_data = poly.getMeshData();

  glm::vec4 prim_id(1.0f, 1.0f, 1.0f, 1.0f);

  using rng = std::mt19937;
  std::uniform_int_distribution<rng::result_type> u24dist(0, 0xFF'FFFF);
  rng generator;

  auto randId = [&]() {
    u32 clr = u24dist(generator);
    prim_id.r = static_cast<float>((clr >> 16) & 0xff) / 255.0f;
    prim_id.g = static_cast<float>((clr >> 8) & 0xff) / 255.0f;
    prim_id.b = static_cast<float>((clr >> 0) & 0xff) / 255.0f;
  };

  auto draw_p = [&](int i, int j) {
    auto prim = poly.getMeshData().mMatrixPrimitives[i].mPrimitives[j];
    u32 k = 0;
    randId();
    for (auto& v : prim.mVertices) {
      ImGui::TableNextRow();

      riistudio::util::IDScope v_s(k);

      ImGui::TableSetColumnIndex(1);

      if (prim.mType == librii::gx::PrimitiveType::Triangles && k % 3 == 0 &&
          k >= 3) {
        randId();
      }

      ImVec4 clr(prim_id.r, prim_id.g, prim_id.b, 1.0f);
      ImGui::TextColored(clr, "%u", k);

      u32 q = 0;
      for (auto& e : poly.getVcd().mAttributes) {
        if (e.second == gx::VertexAttributeType::None)
          continue;
        ImGui::TableSetColumnIndex(2 + q);
        int data = v.operator[](e.first);
        ImGui::TextColored(clr, "%i", data);
        ++q;
      }
      ++k;
    }
  };

  auto draw_mp = [&](int i) {
    auto& mprim = mesh_data.mMatrixPrimitives[i];
    ImGui::Text("Default Matrix: %i"_j, (int)mprim.mCurrentMatrix);

    const int attrib_cnt = std::count_if(
        desc.mAttributes.begin(), desc.mAttributes.end(), [](const auto& e) {
          return e.second != gx::VertexAttributeType::None;
        });

    const auto table_flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
        ImGuiTableFlags_Sortable;
    if (ImGui::BeginTable("Vertex data"_j, 2 + attrib_cnt, table_flags)) {
      ImGui::TableSetupColumn("Primitive Index"_j
#ifdef WIP_ANGLED_HEADERS
                              , ImGuiTableColumnFlags_AngledHeader
#endif
      );
      ImGui::TableSetupColumn("Vertex Index"_j
#ifdef WIP_ANGLED_HEADERS
                              , ImGuiTableColumnFlags_AngledHeader
#endif
      );

      u32 q = 0;
      for (auto& e : poly.getVcd().mAttributes) {
        if (e.second == gx::VertexAttributeType::None)
          continue;

        int type = static_cast<int>(e.first);
        ImGui::TableSetupColumn(vertexAttribNamesArray[type]
#ifdef WIP_ANGLED_HEADERS
                                , ImGuiTableColumnFlags_AngledHeader
#endif
        );
        ++q;
      }

#ifdef WIP_ANGLED_HEADERS
      ImGui::TableAngledHeadersRow();
#endif
      ImGui::TableHeadersRow();

      static const std::array<std::string, 8> prim_types{
          "Quads"_j,        "QuadStrips"_j, "Triangles"_j,  "TriangleStrips"_j,
          "TriangleFans"_j, "Lines"_j,      "LineStrips"_j, "Points"_j};

      for (int j = 0;
           j < poly.getMeshData().mMatrixPrimitives[i].mPrimitives.size();
           ++j) {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        bool open = ImGui::TreeNodeEx(
            (std::string("#") + std::to_string(j) + " (" +
             prim_types[static_cast<int>(poly.getMeshData()
                                             .mMatrixPrimitives[i]
                                             .mPrimitives[j]
                                             .mType)] +
             ")")
                .c_str(),
            ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen);
        if (open) {
          draw_p(i, j);
          ImGui::TreePop();
        }
      }

      ImGui::EndTable();
    }
  };

  if (ImGui::BeginTabBar("Matrix Primitives"_j)) {
    for (int i = 0; i < poly.getMeshData().mMatrixPrimitives.size(); ++i) {
      if (ImGui::BeginTabItem(
              (std::string("Matrix Prim: "_j) + std::to_string(i)).c_str())) {
        draw_mp(i);
        ImGui::EndTabItem();
      }
    }
    ImGui::EndTabBar();
  }
}

} // namespace libcube::UI
