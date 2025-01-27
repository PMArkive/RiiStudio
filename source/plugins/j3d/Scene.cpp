#include "Scene.hpp"

#include <librii/gx.h>

namespace riistudio::j3d {

using namespace libcube;

static gx::TexCoordGen postTexGen(const gx::TexCoordGen& gen) {
  return gx::TexCoordGen{// gen.id,
                         gen.func, gen.sourceParam,
                         static_cast<gx::TexMatrix>(gen.postMatrix), false,
                         gen.postMatrix};
}
void Model::MatCache::propagate(Material& mat) {
  Indirect _ind(mat);
  indirectInfos.push_back(mat); // one per mat
  update_section(cullModes, mat.cullMode);
  for (int i = 0; i < mat.chanData.size(); ++i) {
    auto& chan = mat.chanData[i];
    update_section(matColors, chan.matColor);
    update_section(ambColors, chan.ambColor);
  }
  update_section(nColorChan, static_cast<u8>(mat.chanData.size()));
  update_section_multi(colorChans, mat.colorChanControls);
  update_section_multi(lightColors, mat.lightColors);
  update_section(nTexGens, static_cast<u8>(mat.texGens.size()));
  auto tgs = mat.texGens;
  for (auto& tg : tgs) {
    if (auto mtx = tg.getMatrixIndex();
        mtx > 0 && mat.texMatrices[mtx].isIdentity())
      tg.matrix = gx::TexMatrix::Identity;
  }
  update_section_multi(texGens, tgs);

  for (int i = 0; i < mat.texGens.size(); ++i) {
    if (mat.texGens[i].postMatrix != gx::PostTexMatrix::Identity) {
      update_section(posTexGens, postTexGen(mat.texGens[i]));
    }
  }

  for (int i = 0; i < mat.texMatrices.size(); ++i) {
    update_section(texMatrices, mat.texMatrices[i]);
  }
  for (int i = 0; i < mat.postTexMatrices.size(); ++i) {
    update_section(postTexMatrices, mat.postTexMatrices[i]);
  }

  for (int i = 0; i < mat.samplers.size(); ++i) {
    // We have already condensed samplers by this point. Only the ID matters.
    // MaterialData::J3DSamplerData tmp;
    // tmp.btiId =
    // reinterpret_cast<MaterialData::J3DSamplerData*>(mat.samplers[i].get())->btiId;
    // update_section(samplers, tmp);
    update_section(samplers, mat.samplers[i]);
  }
  for (auto& stage : mat.mStages) {
    TevOrder order;
    order.rasOrder = stage.rasOrder;
    order.texCoord = stage.texCoord;
    order.texMap = stage.texMap;

    update_section(orders, order);

    SwapSel swap;
    swap.colorChanSel = stage.rasSwap;
    swap.texSel = stage.texMapSwap;

    update_section(swapModes, swap);

    librii::gx::TevStage tmp;
    tmp.colorStage = stage.colorStage;
    tmp.colorStage.constantSelection = gx::TevKColorSel::k0;
    tmp.alphaStage = stage.alphaStage;
    tmp.alphaStage.constantSelection = gx::TevKAlphaSel::k0_a;

    update_section(tevStages, tmp);
  }
  update_section_multi(tevColors, mat.tevColors);
  update_section_multi(konstColors, mat.tevKonstColors);
  update_section(nTevStages, static_cast<u8>(mat.mStages.size()));

  update_section_multi(swapTables, mat.mSwapTable);
  update_section(fogs, mat.fogInfo);
  update_section(alphaComparisons, mat.alphaCompare);
  update_section(blendModes, mat.blendMode);
  update_section(zModes, mat.zMode);
  update_section(zCompLocs, static_cast<u8>(mat.earlyZComparison));
  update_section(dithers, static_cast<u8>(mat.dither));
  update_section(nbtScales, mat.nbtScale);
}

} // namespace riistudio::j3d
