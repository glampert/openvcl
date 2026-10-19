// Regression tests: an instruction immediate must fit its encoding.
//
// dvp-as keeps only the low bits of an immediate that does not fit the
// instruction's field, and says nothing: 'iaddi VI03, VI10, -18' assembles
// to the same word as '+14', because IADDI's immediate is 5 bits, signed.
// openvcl passed such values through, both from the source and from its own
// offset rewrites, so the first sign of one was on screen.
//
// Now an iaddi outside -16..15 becomes an isubiu or iaddiu (SCE VCL emits
// every iaddi that way), an iaddiu/isubiu or flag-mask immediate that does
// not fit is an error, and a memory offset outside -1024..1023 is a warning
// (the assembler wraps it, which on VU1 reaches the same address).  The
// scheduler no longer moves a load or store when that would push its offset
// out of the field.

#include "test_harness.h"
#include "openvcl_runner.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace
{
    std::string program(const std::string& body)
    {
        return
            "\t.init_vf_all\n"
            "\t.init_vi_all\n"
            "\t.syntax new\n"
            "\t.name vsmImmediateRanges\n"
            "\t--enter\n"
            "\t--endenter\n"
            + body +
            "\t--exit\n"
            "\t--endexit\n";
    }

    ::test::RunResult compile(const std::string& source)
    {
        return ::test::run_openvcl(std::vector<std::string>(), source);
    }

    bool contains(const std::string& text, const std::string& needle)
    {
        return text.find(needle) != std::string::npos;
    }

    // True when every "<offset>(VIxx)" memory operand in the emitted code
    // fits the 11-bit field of lq/sq/ilw/isw.
    bool memoryOffsetsFit(const std::string& vsm)
    {
        for (std::string::size_type i = vsm.find("(VI"); i != std::string::npos; i = vsm.find("(VI", i + 1))
        {
            std::string::size_type begin = i;
            while (begin > 0 && ((vsm[begin - 1] >= '0' && vsm[begin - 1] <= '9') || vsm[begin - 1] == '-'))
                --begin;
            if (begin == i)
                continue;
            const long offset = std::strtol(vsm.substr(begin, i - begin).c_str(), NULL, 10);
            if (offset < -1024 || offset > 1023)
                return false;
        }
        return true;
    }

    std::string withOffset(const std::string& source, const std::string& offset)
    {
        std::string result = source;
        result.replace(result.find("OFFSET"), 6, offset);
        return result;
    }

    // A software-pipelined loop that prefetches the next iteration's load,
    // lq.xyzw vf01, OFFSET+1(vi01).
    const char* kPrefetchLoop =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tiaddiu vi01, vi00, 0\n"
        "\tiaddiu vi02, vi00, 3\n"
        "\tmove.xyzw vf03, vf00\n"
        "loop_lid:\n"
        "\t--LoopCS 1,1\n"
        "\tlq.xyzw vf01, OFFSET(vi01)\n"
        "\tdiv q, vf00w, vf01w\n"
        "\tmulq.xyz vf02, vf03, q\n"
        "\tadd.xyz vf10, vf00, vf00\n"
        "\tadd.xyz vf11, vf00, vf00\n"
        "\tadd.xyz vf12, vf00, vf00\n"
        "\tadd.xyz vf13, vf00, vf00\n"
        "\tadd.xyz vf14, vf00, vf00\n"
        "\tadd.xyz vf15, vf00, vf00\n"
        "\tiaddiu vi01, vi01, 1\n"
        "\tibne vi01, vi02, loop_lid\n"
        "\t--exit\n"
        "\t--endexit\n";

    // A software-pipelined loop whose store drains into the next iteration
    // at OFFSET-1(vi02).
    const char* kSuffixStoreLoop =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tiaddiu vi01, vi00, 0\n"
        "\tiaddiu vi02, vi00, 0\n"
        "\tiaddiu vi04, vi00, 3\n"
        "\tmove.xyzw vf02, vf00\n"
        "loop_lid:\n"
        "\t--LoopCS 1,1\n"
        "\tlq.xyzw vf01, 0(vi01)\n"
        "\tdiv q, vf00w, vf01w\n"
        "\tmulq.xyz vf03, vf02, q\n"
        "\tsq.xyz vf03, OFFSET(vi02)\n"
        "\tadd.xyz vf10, vf00, vf00\n"
        "\tadd.xyz vf11, vf00, vf00\n"
        "\tadd.xyz vf12, vf00, vf00\n"
        "\tadd.xyz vf13, vf00, vf00\n"
        "\tadd.xyz vf14, vf00, vf00\n"
        "\tiaddiu vi01, vi01, 1\n"
        "\tiaddiu vi02, vi02, 1\n"
        "\tibne vi01, vi04, loop_lid\n"
        "\t--exit\n"
        "\t--endexit\n";

    // A multi-Q loop whose cyclic prefix carries lq.xyzw vf03, OFFSET(vi01)
    // across the vi01 update.
    const char* kMultiQPrefixLoop =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tiaddiu vi01, vi00, 0\n"
        "\tiaddiu vi02, vi00, 3\n"
        "loop_lid:\n"
        "\t--LoopCS 1,1\n"
        "\tlq.xyzw vf03, OFFSET(vi01)\n"
        "\tdiv q, vf00w, vf03w\n"
        "\tmulq.xyz vf02, vf00, q\n"
        "\tadd.xyz vf10, vf00, vf00\n"
        "\tadd.xyz vf11, vf00, vf00\n"
        "\tdiv q, vf00w, vf00w\n"
        "\tadd.xyz vf12, vf00, vf00\n"
        "\tadd.xyz vf13, vf00, vf00\n"
        "\tmulq.xyz vf05, vf00, q\n"
        "\tadd.xyz vf14, vf00, vf00\n"
        "\tiaddiu vi01, vi01, 1\n"
        "\tibne vi01, vi02, loop_lid\n"
        "\t--exit\n"
        "\t--endexit\n";

    // A loop whose store can fill the branch delay slot, after the vi01
    // update, as sq.xyzw vf02, OFFSET-1(vi01).
    const char* kDelaySlotStoreLoop =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tiaddiu vi01, vi00, 0\n"
        "\tiaddiu vi02, vi00, 3\n"
        "loop_lid:\n"
        "\tlq.xyzw vf01, 0(vi00)\n"
        "\tadd.xyzw vf02, vf01, vf01\n"
        "\tsq.xyzw vf02, OFFSET(vi01)\n"
        "\tiaddiu vi01, vi01, 1\n"
        "\tibne vi01, vi02, loop_lid\n"
        "\t--exit\n"
        "\t--endexit\n";
}

TEST_CASE("openvcl: iaddi keeps an immediate that fits its 5 bits")
{
    ::test::RunResult r = compile(program(
        "\tiaddi iA, vi00, -16\n"
        "\tiaddi iB, vi00, 15\n"
        "\tisw.x iA, 0(vi00)\n"
        "\tisw.x iB, 1(vi00)\n"));

    CHECK(r.exit_code == 0);
    CHECK(r.stderr_data.empty());
    CHECK(contains(r.stdout_data, "iaddi VI01, VI00, -16"));
    CHECK(contains(r.stdout_data, "iaddi VI02, VI00, 15"));
}

TEST_CASE("openvcl: iaddi beyond its 5 bits becomes isubiu or iaddiu")
{
    ::test::RunResult r = compile(program(
        "\tilw.x iA, 0(vi00)\n"
        "\tiaddi iB, iA, -18\n"
        "\tiaddi iC, iA, 16\n"
        "\tiaddi iD, iA, (0 - 17)\n"
        "\tiaddi iE, iA, -32767\n"
        "\tiaddi iF, iA, 32767\n"
        "\tisw.x iB, 1(vi00)\n"
        "\tisw.x iC, 2(vi00)\n"
        "\tisw.x iD, 3(vi00)\n"
        "\tisw.x iE, 4(vi00)\n"
        "\tisw.x iF, 5(vi00)\n"));

    CHECK(r.exit_code == 0);
    CHECK(r.stderr_data.empty());
    CHECK(contains(r.stdout_data, "isubiu VI02, VI01, 18"));
    CHECK(contains(r.stdout_data, "iaddiu VI03, VI01, 16"));
    CHECK(contains(r.stdout_data, "isubiu VI04, VI01, 17"));
    CHECK(contains(r.stdout_data, "isubiu VI05, VI01, 32767"));
    CHECK(contains(r.stdout_data, "iaddiu VI06, VI01, 32767"));
    CHECK(!contains(r.stdout_data, "iaddi "));
}

TEST_CASE("openvcl: iaddi beyond 15 bits is an error")
{
    ::test::RunResult r = compile(program(
        "\tiaddi iA, vi00, -32768\n"
        "\tisw.x iA, 0(vi00)\n"));

    CHECK(r.exit_code != 0);
    CHECK(contains(r.stderr_data, "Immediate -32768 is outside -32767..32767"));
}

TEST_CASE("openvcl: iaddiu and isubiu take 0..32767")
{
    ::test::RunResult ok = compile(program(
        "\tiaddiu iA, vi00, 32767\n"
        "\tiaddiu iB, vi00, 0x7fff\n"
        "\tisubiu iC, vi00, 0\n"
        "\tisw.x iA, 0(vi00)\n"
        "\tisw.x iB, 1(vi00)\n"
        "\tisw.x iC, 2(vi00)\n"));
    CHECK(ok.exit_code == 0);
    CHECK(ok.stderr_data.empty());

    const char* rejected[][2] =
    {
        { "iaddiu iA, vi00, 32768", "Immediate 32768 is outside 0..32767" },
        { "iaddiu iA, vi00, 0x8000", "Immediate 32768 is outside 0..32767" },
        { "iaddiu iA, vi00, -1", "Immediate -1 is outside 0..32767" },
        { "isubiu iA, vi00, 40000", "Immediate 40000 is outside 0..32767" },
        { "isubiu iA, vi00, -1", "Immediate -1 is outside 0..32767" },
    };
    for (unsigned int i = 0; i < sizeof(rejected) / sizeof(rejected[0]); ++i)
    {
        ::test::RunResult r = compile(program(std::string("\t") + rejected[i][0] + "\n\tisw.x iA, 0(vi00)\n"));
        CHECK(r.exit_code != 0);
        CHECK(contains(r.stderr_data, rejected[i][1]));
    }
}

TEST_CASE("openvcl: clip flag masks take 24 bits")
{
    ::test::RunResult ok = compile(program(
        "\tfcset 0xffffff\n"
        "\tfcand vi01, 0xffffff\n"
        "\tisw.x vi01, 0(vi00)\n"));
    CHECK(ok.exit_code == 0);
    CHECK(ok.stderr_data.empty());

    const char* rejected[] =
    {
        "\tfcset 0\n\tfcand vi01, 0x1000000\n\tisw.x vi01, 0(vi00)\n",
        "\tfcset 0\n\tfcor vi01, 16777216\n\tisw.x vi01, 0(vi00)\n",
        "\tfcset 0\n\tfceq vi01, 0x1000000\n\tisw.x vi01, 0(vi00)\n",
        "\tfcset 0x1000000\n\tfcand vi01, 1\n\tisw.x vi01, 0(vi00)\n",
    };
    for (unsigned int i = 0; i < sizeof(rejected) / sizeof(rejected[0]); ++i)
    {
        ::test::RunResult r = compile(program(rejected[i]));
        CHECK(r.exit_code != 0);
        CHECK(contains(r.stderr_data, "Immediate 16777216 is outside 0..16777215"));
    }
}

TEST_CASE("openvcl: status flag masks take 12 bits")
{
    ::test::RunResult ok = compile(program(
        "\tfsset 0xfff\n"
        "\tfsand vi01, 0xfff\n"
        "\tisw.x vi01, 0(vi00)\n"));
    CHECK(ok.exit_code == 0);
    CHECK(ok.stderr_data.empty());

    const char* rejected[] =
    {
        "\tfsand vi01, 0x1000\n\tisw.x vi01, 0(vi00)\n",
        "\tfsor vi01, 4096\n\tisw.x vi01, 0(vi00)\n",
        "\tfseq vi01, 0x1000\n\tisw.x vi01, 0(vi00)\n",
        "\tfsset 0x1000\n",
    };
    for (unsigned int i = 0; i < sizeof(rejected) / sizeof(rejected[0]); ++i)
    {
        ::test::RunResult r = compile(program(rejected[i]));
        CHECK(r.exit_code != 0);
        CHECK(contains(r.stderr_data, "Immediate 4096 is outside 0..4095"));
    }
}

TEST_CASE("openvcl: an immediate error does not cascade into later lines")
{
    ::test::RunResult r = compile(program(
        "\tiaddiu iA, vi00, 40000\n"
        "\tisw.x iA, 0(vi00)\n"));

    CHECK(r.exit_code != 0);
    CHECK(contains(r.stderr_data, "Immediate 40000 is outside 0..32767 (iaddiu iA, vi00, >>> 40000 <<<)"));
    CHECK(r.stderr_data.find('\n') == r.stderr_data.size() - 1);
}

TEST_CASE("openvcl: a memory offset outside 11 bits is a warning")
{
    ::test::RunResult edges = compile(program(
        "\tlq fA, 1023(vi00)\n"
        "\tsq fA, -1024(vi00)\n"
        "\tilw.x iA, 1023(vi00)\n"
        "\tisw.x iA, -1024(vi00)\n"));
    CHECK(edges.exit_code == 0);
    CHECK(edges.stderr_data.empty());

    ::test::RunResult r = compile(program(
        "\tlq fA, 1024(vi00)\n"
        "\tsq fA, (1010 + 15)(vi00)\n"
        "\tilw.x iA, -1025(vi00)\n"
        "\tisw.x iA, 2000(vi00)\n"
        "\tsq fA, 0x400(vi00)\n"));
    CHECK(r.exit_code == 0);
    CHECK(contains(r.stderr_data, "warning: Offset 1024 is outside -1024..1023 and assembles as -1024 (lq fA, >>> 1024(vi00) <<<)"));
    CHECK(contains(r.stderr_data, "warning: Offset 1025 is outside -1024..1023 and assembles as -1023"));
    CHECK(contains(r.stderr_data, "warning: Offset -1025 is outside -1024..1023 and assembles as 1023"));
    CHECK(contains(r.stderr_data, "warning: Offset 2000 is outside -1024..1023 and assembles as -48"));
    CHECK(contains(r.stderr_data, "warning: Offset 1024 is outside -1024..1023 and assembles as -1024 (sq fA, >>> 0x400(vi00) <<<)"));
    CHECK(contains(r.stdout_data, "lq VF01, 1024(VI00)"));
    CHECK(contains(r.stdout_data, "isw.x VI01, 2000(VI00)"));
}

TEST_CASE("openvcl: immediates only the assembler can evaluate are left to it")
{
    ::test::RunResult r = compile(program(
        "\tiaddiu iA, vi00, kVertexCount\n"
        "\tisw.x iA, 0(vi00)\n"
        "\tlq fA, kBase+1(vi00)\n"
        "\tsq fA, kBase+2(vi00)\n"));

    CHECK(r.exit_code == 0);
    CHECK(r.stderr_data.empty());
    CHECK(contains(r.stdout_data, "iaddiu VI01, VI00, kVertexCount"));
    CHECK(contains(r.stdout_data, "lq VF01, kBase+1(VI00)"));
}

TEST_CASE("openvcl: software pipelining keeps a prefetch offset encodable")
{
    ::test::RunResult fits = compile(withOffset(kPrefetchLoop, "1022"));
    CHECK(fits.exit_code == 0);
    CHECK(contains(fits.stdout_data, "loop_lid__PROLOG:"));
    CHECK(contains(fits.stdout_data, "lq VF01, 1023(VI01)"));

    // the next iteration's lq.xyzw vf01, 1024(vi01) would not fit
    ::test::RunResult r = compile(withOffset(kPrefetchLoop, "1023"));
    CHECK(r.exit_code == 0);
    CHECK(r.stderr_data.empty());
    CHECK(memoryOffsetsFit(r.stdout_data));
}

TEST_CASE("openvcl: software pipelining keeps a drained store offset encodable")
{
    ::test::RunResult fits = compile(withOffset(kSuffixStoreLoop, "-1022"));
    CHECK(fits.exit_code == 0);
    CHECK(contains(fits.stdout_data, "loop_lid__DRAIN:"));
    CHECK(contains(fits.stdout_data, "sq.xyz VF03, -1024(VI02)"));
    CHECK(memoryOffsetsFit(fits.stdout_data));

    // draining (and advancing the vi02 update) would need -1025(vi02) or below
    const char* offsets[] = { "-1023", "-1024" };
    for (unsigned int i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i)
    {
        ::test::RunResult r = compile(withOffset(kSuffixStoreLoop, offsets[i]));
        CHECK(r.exit_code == 0);
        CHECK(r.stderr_data.empty());
        CHECK(memoryOffsetsFit(r.stdout_data));
    }
}

TEST_CASE("openvcl: software pipelining keeps a cyclic prefix offset encodable")
{
    ::test::RunResult fits = compile(withOffset(kMultiQPrefixLoop, "1022"));
    CHECK(fits.exit_code == 0);
    CHECK(contains(fits.stdout_data, "loop_lid__PROLOG:"));
    CHECK(contains(fits.stdout_data, "lq VF03, 1023(VI01)"));

    // the prefix load would become lq.xyzw vf03, 1024(vi01)
    ::test::RunResult r = compile(withOffset(kMultiQPrefixLoop, "1023"));
    CHECK(r.exit_code == 0);
    CHECK(r.stderr_data.empty());
    CHECK(memoryOffsetsFit(r.stdout_data));
}

TEST_CASE("openvcl: a store moved into a branch delay slot keeps its offset encodable")
{
    ::test::RunResult moved = compile(withOffset(kDelaySlotStoreLoop, "-1023"));
    CHECK(moved.exit_code == 0);
    CHECK(contains(moved.stdout_data, "sq VF02, -1024(VI01)"));

    // after the vi01 update the store would need -1025(vi01)
    ::test::RunResult r = compile(withOffset(kDelaySlotStoreLoop, "-1024"));
    CHECK(r.exit_code == 0);
    CHECK(r.stderr_data.empty());
    CHECK(contains(r.stdout_data, "sq VF02, -1024(VI01)"));
    CHECK(memoryOffsetsFit(r.stdout_data));
}
