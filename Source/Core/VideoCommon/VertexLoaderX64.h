// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <map>

#include "Common/x64Emitter.h"
#include "VideoCommon/VertexLoaderBase.h"

class VertexLoaderX64 : public VertexLoaderBase, public Gen::X64CodeBlock
{
public:
	VertexLoaderX64(const TVtxDesc& vtx_desc, const VAT& vtx_att);

protected:
	std::string GetName() const override { return "VertexLoaderX64"; }
	bool IsInitialized() override { return true; }
	int RunVertices(DataReader src, DataReader dst, int count) override;

private:
	std::map<const void*, Gen::X64Reg> m_constants;
	bool m_constant_array_strides = true;
	BitSet32 m_used_strides = BitSet32(0);
	u32 m_strides[16];
	u32 m_src_ofs;
	u32 m_dst_ofs;
	Gen::FixupBranch m_skip_vertex;
	Gen::OpArg GetVertexAddr(int array, u64 attribute);
	Gen::OpArg GetConstant(const void* ptr);
	int ReadVertex(Gen::OpArg data, u64 attribute, int format, int count_in, int count_out, bool dequantize, u8 scaling_exponent, AttributeFormat* native_format);
	void ReadColor(Gen::OpArg data, u64 attribute, int format);
	void GenerateVertexLoader();
};
