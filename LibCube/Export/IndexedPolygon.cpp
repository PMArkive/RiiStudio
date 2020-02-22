#include "IndexedPolygon.hpp"

#include <LibCube/GX/Shader/GXMaterial.hpp>

namespace libcube {

u64 IndexedPolygon::getNumPrimitives() const
{
	u64 total = 0;

	for (u64 i = 0; i < getNumMatrixPrimitives(); ++i)
		total += getMatrixPrimitiveNumIndexedPrimitive(i);

	return total;
}
// Triangles
// We add this to the last mprim. May need to be split up later.
s64 IndexedPolygon::addPrimitive()
{
	if (getNumMatrixPrimitives() == 0)
		addMatrixPrimitive();

	const s64 idx = getNumMatrixPrimitives();
	assert(idx > 0);
	if (idx <= 0)
		return -1;

	//	const s64 iprim_idx = addMatrixPrimitiveIndexedPrimitive();
	//	assert(iprim_idx >= 0);
	//	if (iprim_idx < 0)
	//		return;
	//	
	//	auto& iprim = getMatrixPrimitiveIndexedPrimitive(idx, iprim_idx);
	//	
	//	// TODO
	return -1;
}
bool IndexedPolygon::hasAttrib(SimpleAttrib attrib) const
{
	switch (attrib)
	{
	case SimpleAttrib::EnvelopeIndex:
		return getVcd()[gx::VertexAttribute::PositionNormalMatrixIndex];
	case SimpleAttrib::Position:
		return getVcd()[gx::VertexAttribute::Position];
	case SimpleAttrib::Normal:
		return getVcd()[gx::VertexAttribute::Position];
	case SimpleAttrib::Color0:
	case SimpleAttrib::Color1:
		return getVcd()[gx::VertexAttribute::Color0 + ((u64)attrib - (u64)SimpleAttrib::Color0)];
	case SimpleAttrib::TexCoord0:
	case SimpleAttrib::TexCoord1:
	case SimpleAttrib::TexCoord2:
	case SimpleAttrib::TexCoord3:
	case SimpleAttrib::TexCoord4:
	case SimpleAttrib::TexCoord5:
	case SimpleAttrib::TexCoord6:
	case SimpleAttrib::TexCoord7:
		return getVcd()[gx::VertexAttribute::TexCoord0 + ((u64)attrib - (u64)SimpleAttrib::TexCoord0)];
	default:
		return false;
	}
}
void IndexedPolygon::setAttrib(SimpleAttrib attrib, bool v)
{
	// We usually recompute the index (no direct support*)
	switch (attrib)
	{
	case SimpleAttrib::EnvelopeIndex:
		getVcd().mAttributes[gx::VertexAttribute::PositionNormalMatrixIndex] = gx::VertexAttributeType::Direct;
		getVcd().mBitfield |= (1 << (int)gx::VertexAttribute::PositionNormalMatrixIndex);
		break;
	case SimpleAttrib::Position:
		getVcd().mAttributes[gx::VertexAttribute::Position] = gx::VertexAttributeType::Short;
		getVcd().mBitfield |= (1 << (int)gx::VertexAttribute::Position);
		break;
	case SimpleAttrib::Normal:
		getVcd().mAttributes[gx::VertexAttribute::Normal] = gx::VertexAttributeType::Short;
		getVcd().mBitfield |= (1 << (int)gx::VertexAttribute::Normal);
		break;
	case SimpleAttrib::Color0:
	case SimpleAttrib::Color1:
		getVcd().mAttributes[gx::VertexAttribute::Color0 + ((u64)attrib - (u64)SimpleAttrib::Color0)] = gx::VertexAttributeType::Short;
		getVcd().mBitfield |= (1 << ((int)gx::VertexAttribute::Color0 + ((u64)attrib - (u64)SimpleAttrib::Color0)));
		break;
	case SimpleAttrib::TexCoord0:
	case SimpleAttrib::TexCoord1:
	case SimpleAttrib::TexCoord2:
	case SimpleAttrib::TexCoord3:
	case SimpleAttrib::TexCoord4:
	case SimpleAttrib::TexCoord5:
	case SimpleAttrib::TexCoord6:
	case SimpleAttrib::TexCoord7:
		getVcd().mAttributes[gx::VertexAttribute::TexCoord0 + ((u64)attrib - (u64)SimpleAttrib::TexCoord0)] = gx::VertexAttributeType::Short;
		getVcd().mBitfield |= (1 << ((int)gx::VertexAttribute::TexCoord0 + ((u64)attrib - (u64)SimpleAttrib::TexCoord0)));
		break;
	default:
		assert(!"Invalid simple vertex attrib");
		break;
	}
}
IndexedPrimitive* IndexedPolygon::getIndexedPrimitiveFromSuperIndex(u64 idx)
{
	u64 cnt = 0;
	for (u64 i = 0; i < getNumMatrixPrimitives(); ++i)
	{
		if (idx >= cnt && idx < cnt + getMatrixPrimitiveNumIndexedPrimitive(i))
			return &getMatrixPrimitiveIndexedPrimitive(i, idx - cnt);

		cnt += getMatrixPrimitiveNumIndexedPrimitive(i);
	}
	return nullptr;
}
const IndexedPrimitive* IndexedPolygon::getIndexedPrimitiveFromSuperIndex(u64 idx) const
{
	u64 cnt = 0;
	for (u64 i = 0; i < getNumMatrixPrimitives(); ++i)
	{
		if (idx >= cnt && idx < cnt + getMatrixPrimitiveNumIndexedPrimitive(i))
			return &getMatrixPrimitiveIndexedPrimitive(i, idx - cnt);

		cnt += getMatrixPrimitiveNumIndexedPrimitive(i);
	}
	return nullptr;
}
u64 IndexedPolygon::getPrimitiveVertexCount(u64 index) const
{
	const IndexedPrimitive* prim = getIndexedPrimitiveFromSuperIndex(index);
	assert(prim);
	return prim->mVertices.size();
}
void IndexedPolygon::resizePrimitiveVertexArray(u64 index, u64 size)
{
	IndexedPrimitive* prim = getIndexedPrimitiveFromSuperIndex(index);
	assert(prim);
	prim->mVertices.resize(size);
}
IndexedPolygon::SimpleVertex IndexedPolygon::getPrimitiveVertex(u64 prim_idx, u64 vtx_idx)
{
	const auto& iprim = *getIndexedPrimitiveFromSuperIndex(prim_idx);
	assert(vtx_idx < iprim.mVertices.size());
	const auto& vtx = iprim.mVertices[vtx_idx];

	return {
		(u8)vtx[gx::VertexAttribute::PositionNormalMatrixIndex],
		getPos(vtx[gx::VertexAttribute::Position])
	};
	return {};
}
void IndexedPolygon::propogate(VBOBuilder& out) const
{
	u32 final_bitfield = 0;

	auto propTri = [&](const std::array<IndexedVertex, 3>& tri)
	{
		const auto& vcd = getVcd();
		for (const auto& vtx : tri)
		{
			out.mIndices.push_back(out.mIndices.size());
			final_bitfield |= vcd.mBitfield;
			for (int i = 0; i < (int)gx::VertexAttribute::Max; ++i)
			{
				if (!(vcd.mBitfield & (1 << i))) continue;

				switch (static_cast<gx::VertexAttribute>(i))
				{
				case gx::VertexAttribute::PositionNormalMatrixIndex:
				case gx::VertexAttribute::Texture0MatrixIndex:
				case gx::VertexAttribute::Texture1MatrixIndex:
				case gx::VertexAttribute::Texture2MatrixIndex:
				case gx::VertexAttribute::Texture3MatrixIndex:
				case gx::VertexAttribute::Texture4MatrixIndex:
				case gx::VertexAttribute::Texture5MatrixIndex:
				case gx::VertexAttribute::Texture6MatrixIndex:
				case gx::VertexAttribute::Texture7MatrixIndex:
					break;
				case gx::VertexAttribute::Position:
					out.pushData(0, getPos(vtx[gx::VertexAttribute::Position]));
					break;
				case gx::VertexAttribute::Color0:
					out.pushData(5, getClr(vtx[gx::VertexAttribute::Color0]));
					break;
				case gx::VertexAttribute::TexCoord0:
				case gx::VertexAttribute::TexCoord1:
				case gx::VertexAttribute::TexCoord2:
				case gx::VertexAttribute::TexCoord3:
				case gx::VertexAttribute::TexCoord4:
				case gx::VertexAttribute::TexCoord5:
				case gx::VertexAttribute::TexCoord6:
				case gx::VertexAttribute::TexCoord7:
				{
					const auto chan = i - static_cast<int>(gx::VertexAttribute::TexCoord0);
					const auto attr = static_cast<gx::VertexAttribute>(i);
					out.pushData(7 + chan , getUv(chan, vtx[attr]));
					break;
				}
				case gx::VertexAttribute::Normal:
					out.pushData(4, getNrm(vtx[gx::VertexAttribute::Normal]));
					break;
				default:
					throw "Invalid vtx attrib";
					break;
				}
			}
		}
	};

	for (int i = 0; i < getNumMatrixPrimitives(); ++i)
	{
		for (int j = 0; j < getMatrixPrimitiveNumIndexedPrimitive(i); ++j)
		{
			const auto& idx = getMatrixPrimitiveIndexedPrimitive(i, j);
			switch (idx.mType)
			{
			case gx::PrimitiveType::TriangleStrip:
				for (int v = 2; v < idx.mVertices.size(); ++v)
				{
					std::array<IndexedVertex, 3> tri;
					bool isEven = v % 2;
					tri[0] = idx.mVertices[v - 2];
					tri[1] = isEven ? idx.mVertices[v] : idx.mVertices[v - 1];
					tri[2] = isEven ? idx.mVertices[v - 1] : idx.mVertices[v];

					propTri(tri);
				}
				break;
			case gx::PrimitiveType::Triangles:
				for (int v = 0; v < idx.mVertices.size() / 3; ++v)
				{
					std::array<IndexedVertex, 3> tri;

					tri[0] = idx.mVertices[v * 3 + 0];
					tri[1] = idx.mVertices[v * 3 + 1];
					tri[2] = idx.mVertices[v * 3 + 2];

					propTri(tri);
				}
				break;
			default:
				assert(!"TODO");
				break;
			}
		}
	}

	for (int i = 0; i < (int)gx::VertexAttribute::Max; ++i)
	{
		if (!(final_bitfield & (1 << i))) continue;

		const auto& def = getVertexAttribGenDef((gx::VertexAttribute)i);
		out.mPropogating[def.second].first = VAOEntry{ (u32)def.second, def.first.name, def.first.format, def.first.size * 4 };
	}
}
}