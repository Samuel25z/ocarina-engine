// (zelda-port, onda1 gpu-vram) The write-back by changed 4 KB chunks, with fingerprints in place of the VRAM shadows.
//
// The write-back's diff (vk_memory.cpp, "the write-back's diff") keeps a shadow of each pool it writes back: a whole copy of
// the pool in VRAM (336 MB of them in Hyrule Field on an RTX 4050 laptop, 6 GB). When VRAM is 95% full they are given back
// and not made again until it is under 85%: on a 6 GB card that is most of a run, and from then on every written range went
// over PCIe whole, ~61 MB a frame. Here a pool keeps a fingerprint of each 4 KB chunk instead (8 bytes a chunk: a 112 MB pool
// takes 224 KB), after juniortesla53's rx5700xt fork (8d85730, Kokiri on an RX 5700 XT: GPU 20.2 -> 14.0 ms a frame).
//
// A compute shader goes over a run in 4 KB chunks: it fingerprints the mirror's words in the run (two 32-bit sums of a per-word
// mix that is a bijection of the word for its place: one word changed always changes both), compares with the chunk's stored
// fingerprint, and only when they differ copies the chunk's words of the run to guest memory and stores the new fingerprint.
// Reading the mirror costs VRAM bandwidth only; only changed chunks cross the bus.
//
// What the fork did not have: a chunk's fingerprint is trusted only while guest memory there is known to hold what was
// fingerprinted. vk_memory.cpp keeps that per page (Pool::shadow_known, the same bits the shadows use, cleared for the pages the
// CPU wrote, shadow_written, and for the runs copied whole), and sets the fingerprints of the pages it does not know to all ones
// before the pass (vkCmdFillBuffer): such a chunk is copied, as the whole copy did. So guest memory comes out as with the whole
// copy, except for runs the GPU left as they were since the last write-back, which guest memory already holds.
//
// The shader is built with Sirit: set 0 by push descriptor, binding 0 the mirror, 1 the pool's buffer in guest memory, 2 the
// fingerprints; push constants: the run's first and end word, and its first chunk. RX_VK_WB_PRINTS (vk_memory.cpp) picks it.
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "vk_internal.h"
#include "vk_draw.h"   // (descriptors_disturbed)
#include <sirit/sirit.h>
#include "vk_work.h"   // (after the others: counts the commands that do GPU work, for full_barrier)

namespace mkwvk {
namespace {

constexpr uint32_t kChunkWords = 1024;   // 4 KB: a guest page
constexpr uint32_t kThreads = 256;       // (each thread kChunkWords / kThreads words, strided: neighbours write neighbours)

VkPipeline pipe = VK_NULL_HANDLE;
VkPipelineLayout layout = VK_NULL_HANDLE;
bool tried = false;

std::vector<uint32_t> build() {
    using namespace Sirit;
    Module m{0x00010300};
    m.AddCapability(spv::Capability::Shader);
    m.SetMemoryModel(spv::AddressingModel::Logical, spv::MemoryModel::GLSL450);
    const Id void_t = m.TypeVoid(), u32 = m.TypeUInt(32), b1 = m.TypeBool();
    const Id uvec3 = m.TypeVector(u32, 3);
    const auto c = [&](uint32_t v) { return m.Constant(u32, v); };

    // the three storage buffers: runtime arrays of words
    const Id runtime = m.TypeRuntimeArray(u32);
    m.Decorate(runtime, spv::Decoration::ArrayStride, 4U);
    const Id block = m.TypeStruct(runtime);
    m.Decorate(block, spv::Decoration::Block);
    m.MemberDecorate(block, 0U, spv::Decoration::Offset, 0U);
    const Id block_ptr = m.TypePointer(spv::StorageClass::StorageBuffer, block);
    const Id word_ptr_t = m.TypePointer(spv::StorageClass::StorageBuffer, u32);
    Id bufs[3];
    for (uint32_t i = 0; i < 3; i++) {
        bufs[i] = m.AddGlobalVariable(block_ptr, spv::StorageClass::StorageBuffer);
        m.Decorate(bufs[i], spv::Decoration::DescriptorSet, 0U);
        m.Decorate(bufs[i], spv::Decoration::Binding, i);
    }
    m.Decorate(bufs[1], spv::Decoration::Coherent);   // (guest memory: written straight through to the host)
    // push constants: lo word, hi word (end), first chunk
    const Id pc_t = m.TypeStruct(u32, u32, u32);
    m.Decorate(pc_t, spv::Decoration::Block);
    for (uint32_t i = 0; i < 3; i++) m.MemberDecorate(pc_t, i, spv::Decoration::Offset, i * 4);
    const Id pc = m.AddGlobalVariable(m.TypePointer(spv::StorageClass::PushConstant, pc_t), spv::StorageClass::PushConstant);
    const Id pc_word_t = m.TypePointer(spv::StorageClass::PushConstant, u32);
    // the workgroup's two sums, and the chunk's fingerprint as it was (read once, before anyone writes the new one)
    const Id shared_t = m.TypeArray(u32, c(4));
    const Id shared = m.AddGlobalVariable(m.TypePointer(spv::StorageClass::Workgroup, shared_t), spv::StorageClass::Workgroup);
    const Id shared_word_t = m.TypePointer(spv::StorageClass::Workgroup, u32);
    const Id lid_v = m.AddGlobalVariable(m.TypePointer(spv::StorageClass::Input, uvec3), spv::StorageClass::Input);
    m.Decorate(lid_v, spv::Decoration::BuiltIn, spv::BuiltIn::LocalInvocationId);
    const Id wid_v = m.AddGlobalVariable(m.TypePointer(spv::StorageClass::Input, uvec3), spv::StorageClass::Input);
    m.Decorate(wid_v, spv::Decoration::BuiltIn, spv::BuiltIn::WorkgroupId);

    const Id main = m.OpFunction(void_t, spv::FunctionControlMask::MaskNone, m.TypeFunction(void_t));
    m.AddLabel();
    const auto word = [&](uint32_t buf, Id index) { return m.OpAccessChain(word_ptr_t, bufs[buf], c(0), index); };
    const auto sh = [&](uint32_t i) { return m.OpAccessChain(shared_word_t, shared, c(i)); };
    const auto barrier = [&] {
        m.OpControlBarrier(c((uint32_t)spv::Scope::Workgroup), c((uint32_t)spv::Scope::Workgroup),
                           c((uint32_t)(spv::MemorySemanticsMask::AcquireRelease | spv::MemorySemanticsMask::WorkgroupMemory)));
    };
    const auto if_then = [&](Id cond, const auto& body) {
        const Id then = m.OpLabel(), merge = m.OpLabel();
        m.OpSelectionMerge(merge, spv::SelectionControlMask::MaskNone);
        m.OpBranchConditional(cond, then, merge);
        m.AddLabel(then);
        body();
        m.OpBranch(merge);
        m.AddLabel(merge);
    };
    const Id lid = m.OpCompositeExtract(u32, m.OpLoad(uvec3, lid_v), 0U);
    const Id wid = m.OpCompositeExtract(u32, m.OpLoad(uvec3, wid_v), 0U);
    const Id lo = m.OpLoad(u32, m.OpAccessChain(pc_word_t, pc, c(0)));
    const Id hi = m.OpLoad(u32, m.OpAccessChain(pc_word_t, pc, c(1)));
    const Id chunk = m.OpIAdd(u32, m.OpLoad(u32, m.OpAccessChain(pc_word_t, pc, c(2))), wid);
    const Id base = m.OpIMul(u32, chunk, c(kChunkWords));
    const Id is_first = m.OpIEqual(b1, lid, c(0));
    const Id f1 = word(2, m.OpIMul(u32, chunk, c(2))), f2 = word(2, m.OpIAdd(u32, m.OpIMul(u32, chunk, c(2)), c(1)));

    if_then(is_first, [&] {
        m.OpStore(sh(0), c(0));
        m.OpStore(sh(1), c(0));
        m.OpStore(sh(2), m.OpLoad(u32, f1));
        m.OpStore(sh(3), m.OpLoad(u32, f2));
    });
    barrier();

    // this thread's words: their index, whether in the run, their value (0 outside), and the two mixes
    constexpr uint32_t kPer = kChunkWords / kThreads;
    Id index[kPer], inside[kPer], value[kPer];
    Id sum1 = c(0), sum2 = c(0);
    for (uint32_t j = 0; j < kPer; j++) {
        index[j] = m.OpIAdd(u32, base, m.OpIAdd(u32, lid, c(j * kThreads)));
        inside[j] = m.OpLogicalAnd(b1, m.OpUGreaterThanEqual(b1, index[j], lo), m.OpULessThan(b1, index[j], hi));
        // (a load only in the run: the binding is the whole pool, but the run's last chunk may end inside it)
        const Id safe = m.OpSelect(u32, inside[j], index[j], lo);
        value[j] = m.OpSelect(u32, inside[j], m.OpLoad(u32, word(0, safe)), c(0));
        // mix 1: (v ^ i*K) * K2, xorshift, * K3. mix 2: (v + i*K4) * K5, xorshift, * K6. Each a bijection of v for its i.
        Id a = m.OpBitwiseXor(u32, value[j], m.OpIMul(u32, index[j], c(0x9E3779B1u)));
        a = m.OpIMul(u32, a, c(0x85EBCA77u));
        a = m.OpBitwiseXor(u32, a, m.OpShiftRightLogical(u32, a, c(13)));
        a = m.OpIMul(u32, a, c(0xC2B2AE3Du));
        Id b = m.OpIAdd(u32, value[j], m.OpIMul(u32, index[j], c(0x27D4EB2Fu)));
        b = m.OpIMul(u32, b, c(0x165667B1u));
        b = m.OpBitwiseXor(u32, b, m.OpShiftRightLogical(u32, b, c(15)));
        b = m.OpIMul(u32, b, c(0xD3A2646Cu | 1u));
        sum1 = m.OpIAdd(u32, sum1, m.OpSelect(u32, inside[j], a, c(0)));
        sum2 = m.OpIAdd(u32, sum2, m.OpSelect(u32, inside[j], b, c(0)));
    }
    const Id scope = c((uint32_t)spv::Scope::Workgroup), sem = c(0);
    m.OpAtomicIAdd(u32, sh(0), scope, sem, sum1);
    m.OpAtomicIAdd(u32, sh(1), scope, sem, sum2);
    barrier();

    const Id h1 = m.OpLoad(u32, sh(0)), h2 = m.OpLoad(u32, sh(1));
    const Id changed = m.OpLogicalOr(b1, m.OpINotEqual(b1, h1, m.OpLoad(u32, sh(2))), m.OpINotEqual(b1, h2, m.OpLoad(u32, sh(3))));
    if_then(changed, [&] {
        for (uint32_t j = 0; j < kPer; j++)
            if_then(inside[j], [&] { m.OpStore(word(1, index[j]), value[j]); });
        if_then(is_first, [&] {
            m.OpStore(f1, h1);
            m.OpStore(f2, h2);
        });
    });
    m.OpReturn();
    m.OpFunctionEnd();
    m.AddEntryPoint(spv::ExecutionModel::GLCompute, main, "main", lid_v, wid_v);
    m.AddExecutionMode(main, spv::ExecutionMode::LocalSize, kThreads, 1U, 1U);
    return m.Assemble();
}

} // namespace

bool wbp_ready() {
    if (tried) return pipe != VK_NULL_HANDLE;
    tried = true;
    if (!dev.push_descriptor || !vkCmdPushDescriptorSetKHR) {
        logf("vk: write-back by fingerprints off: no push descriptors");
        return false;
    }
    VkDescriptorSetLayoutBinding b[3];
    for (uint32_t i = 0; i < 3; i++) b[i] = {i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    li.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    li.bindingCount = 3, li.pBindings = b;
    VkDescriptorSetLayout set = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(dev.device, &li, nullptr, &set) != VK_SUCCESS) return false;
    const VkPushConstantRange pr{VK_SHADER_STAGE_COMPUTE_BIT, 0, 12};
    VkPipelineLayoutCreateInfo pi{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pi.setLayoutCount = 1, pi.pSetLayouts = &set, pi.pushConstantRangeCount = 1, pi.pPushConstantRanges = &pr;
    if (vkCreatePipelineLayout(dev.device, &pi, nullptr, &layout) != VK_SUCCESS) {
        layout = VK_NULL_HANDLE;
        return false;
    }
    const std::vector<uint32_t> code = build();
    VkShaderModuleCreateInfo mi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    mi.codeSize = code.size() * 4, mi.pCode = code.data();
    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(dev.device, &mi, nullptr, &module) != VK_SUCCESS) return false;
    VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    ci.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module, "main", nullptr};
    ci.layout = layout;
    const VkResult r = vkCreateComputePipelines(dev.device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipe);
    vkDestroyShaderModule(dev.device, module, nullptr);
    if (r != VK_SUCCESS) {
        pipe = VK_NULL_HANDLE;
        logf("vk: write-back by fingerprints off: its pipeline was not made (%d)", (int)r);
        return false;
    }
    logf("vk: write-back by fingerprints ready (RX_VK_WB_PRINTS=0: off)");
    return true;
}

/// One pool's runs (byte offsets in the pool, whole words, merged and in order) through the pass, recorded into cmd. The caller
/// set the fingerprints of the pages it does not know to all ones before, with a barrier between.
void wbp_record(VkCommandBuffer cmd, VkBuffer mirror, VkBuffer guest, VkBuffer prints, const VkBufferCopy* runs, size_t n) {
    if (!n || pipe == VK_NULL_HANDLE) return;
    const VkDescriptorBufferInfo infos[3] = {{mirror, 0, VK_WHOLE_SIZE}, {guest, 0, VK_WHOLE_SIZE}, {prints, 0, VK_WHOLE_SIZE}};
    VkWriteDescriptorSet w[3];
    for (uint32_t i = 0; i < 3; i++) {
        w[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w[i].dstBinding = i, w[i].descriptorCount = 1, w[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w[i].pBufferInfo = &infos[i];
    }
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
    vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 3, w);
    for (size_t k = 0; k < n; k++) {
        const uint64_t lo_w = runs[k].dstOffset / 4, hi_w = (runs[k].dstOffset + runs[k].size) / 4;
        if (hi_w <= lo_w) continue;
        for (uint64_t first = lo_w / kChunkWords, last = (hi_w - 1) / kChunkWords; first <= last;) {
            const uint64_t cnt = std::min<uint64_t>(last - first + 1, 65535);   // (a dispatch's limit: a run over 256 MB in pieces)
            const uint32_t push[3] = {(uint32_t)lo_w, (uint32_t)hi_w, (uint32_t)first};
            vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof push, push);
            vkCmdDispatch(cmd, (uint32_t)cnt, 1, 1);
            first += cnt;
        }
    }
    descriptors_disturbed();   // (the replay's own pushed sets are gone)
}

} // namespace mkwvk
