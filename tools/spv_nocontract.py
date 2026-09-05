# spv_nocontract.py — the DISCRIMINATING EXPERIMENT for the 1-level --fg-core-ab residual.
#
# Hypothesis: the driver's shader compiler contracts a*b+c into FMA differently in the two kernels (the
# expression trees are identical, the surrounding code is not), so a handful of pixels land on the other side
# of an 8-bit rounding boundary. Test: decorate EVERY float arithmetic result in BOTH SPIR-V modules with
# NoContraction (SPIR-V decoration 42), rebuild the binary from the rewritten .spv (an EXPERIMENT build, never
# shipped — wap_warp.comp's product bytes are untouched), and run --fg-core-ab again. diff_px -> 0 = the
# residual IS contraction, explained with data; diff_px unchanged = it is something else.
#
# Lives in tools/ as the build-side instrument of the PFG_NOCONTRACT switch (CMakeLists.txt). Usage:
# spv_nocontract.py <in.spv> <out.spv>  -- in == out is fine  (stdlib only; the SPIR-V binary format is documented in
# the Khronos spec: word 0 magic 0x07230203, instructions = (wordcount << 16 | opcode) + operands.)
# Decorations must precede all function definitions: they are inserted right before the first OpTypeXxx /
# OpVariable / OpConstant group is NOT required — the spec's logical layout puts OpDecorate in section 8
# (after OpEntryPoint/OpExecutionMode, before type declarations), so we insert them after the last existing
# OpDecorate / OpMemberDecorate / OpDecorationGroup (or after the last OpName if there is none).
# Made with my soul - Swately <3
import struct, sys

OP_DECORATE = 71; OP_MEMBER_DECORATE = 72; OP_NAME = 5; OP_MEMBER_NAME = 6
OP_FUNCTION = 54
# float arithmetic that NoContraction is defined for (SPIR-V 2.16.1 / the decoration's text: FAdd FSub FMul ... and
# extended-instruction results are covered by decorating the OpExtInst result too — fma/mix/smoothstep/etc.)
OP_FADD = 129; OP_FSUB = 131; OP_FMUL = 133; OP_FDIV = 136; OP_FNEGATE = 127; OP_DOT = 148
OP_VECTOR_TIMES_SCALAR = 142; OP_VECTOR_TIMES_MATRIX = 144; OP_MATRIX_TIMES_VECTOR = 145; OP_EXT_INST = 12
TARGET_OPS = {OP_FADD, OP_FSUB, OP_FMUL, OP_FDIV, OP_FNEGATE, OP_DOT, OP_VECTOR_TIMES_SCALAR,
              OP_VECTOR_TIMES_MATRIX, OP_MATRIX_TIMES_VECTOR, OP_EXT_INST}
DECO_NO_CONTRACTION = 42

def main():
    src, dst = sys.argv[1], sys.argv[2]
    data = open(src, 'rb').read()
    words = list(struct.unpack('<%dI' % (len(data) // 4), data))
    assert words[0] == 0x07230203, 'not a SPIR-V module'
    bound = words[3]
    # walk the instructions
    insts = []  # (start_index, wordcount, opcode)
    i = 5
    while i < len(words):
        wc = words[i] >> 16; op = words[i] & 0xFFFF
        assert wc > 0, ('bad wordcount at', i)
        insts.append((i, wc, op)); i += wc
    # result ids of the target ops (all of them sit inside functions; the result id is operand 2 = words[start+2])
    targets = []
    already = set()
    for (s, wc, op) in insts:
        if op == OP_DECORATE and words[s + 2] == DECO_NO_CONTRACTION: already.add(words[s + 1])
    for (s, wc, op) in insts:
        if op in TARGET_OPS:
            rid = words[s + 2]
            if rid not in already: targets.append(rid)
    # insertion point: after the last OpDecorate/OpMemberDecorate (section 8), else after the last OpName/OpMemberName
    ins_after = None
    for (s, wc, op) in insts:
        if op in (OP_DECORATE, OP_MEMBER_DECORATE): ins_after = s + wc
    if ins_after is None:
        for (s, wc, op) in insts:
            if op in (OP_NAME, OP_MEMBER_NAME): ins_after = s + wc
    assert ins_after is not None, 'no decoration/name section found'
    new = []
    for rid in targets:
        new += [(3 << 16) | OP_DECORATE, rid, DECO_NO_CONTRACTION]
    out = words[:ins_after] + new + words[ins_after:]
    open(dst, 'wb').write(struct.pack('<%dI' % len(out), *out))
    print('%s: %d instructions, %d float results decorated NoContraction (%d already), bound %d -> %s' % (src.split('\\')[-1], len(insts), len(targets), len(already), bound, dst))

if __name__ == '__main__':
    main()
