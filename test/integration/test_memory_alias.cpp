// Regression tests: memory accesses through different registers keep
// their order.
//
// Two VI registers can hold the same address, but the scheduler took a
// different base register as proof that a load or store touches other
// memory, and moved it across a store to that very address.  Only
// accesses through the same register, at different constant offsets, are
// known to be apart.  The software pipeliner's delayed store has the same
// rule: a load the next iteration makes keeps the store in place unless
// the two addresses are proven apart.

#include "test_harness.h"
#include "openvcl_runner.h"

#include <sstream>
#include <string>
#include <vector>

namespace
{
    // iWalk is 900, so the second store writes the address of the first.
    const char* kStoreThenStore =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmMemoryAlias\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tiaddiu iWalk, vi00, 900\n"
        "\tlq fA, 1(vi00)\n"
        "\tlq fB, 2(vi00)\n"
        "\tmul fC, fA, fB\n"
        "\tsq fC, 1(iWalk)\n"
        "\tsq fA, 901(vi00)\n"
        "\t--exit\n"
        "\t--endexit\n";

    // The load reads the address the store before it writes.
    const char* kStoreThenLoad =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmMemoryAlias\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tiaddiu iWalk, vi00, 900\n"
        "\tlq fA, 1(vi00)\n"
        "\tlq fB, 2(vi00)\n"
        "\tmul fC, fA, fB\n"
        "\tsq fC, 1(iWalk)\n"
        "\tlq fD, 901(vi00)\n"
        "\tsq fD, 5(vi00)\n"
        "\t--exit\n"
        "\t--endexit\n";

    // The load reads the quadword after the one the store writes.
    const char* kStoreThenLoadApart =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmMemoryAlias\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tiaddiu iWalk, vi00, 900\n"
        "\tlq fA, 1(vi00)\n"
        "\tlq fB, 2(vi00)\n"
        "\tmul fC, fA, fB\n"
        "\tsq fC, 1(iWalk)\n"
        "\tlq fD, 2(iWalk)\n"
        "\tsq fD, 5(vi00)\n"
        "\t--exit\n"
        "\t--endexit\n";

    // Each iteration stores at vi03 and steps it, so the load at the top of
    // the next one, at -1(vi03), reads what was just stored.  The pipeliner
    // hoists that load into the previous iteration and delays the store
    // into the next, which would put the load first.
    const char* kHoistedLoadOfStoredAddress =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmMemoryAlias\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tiaddiu vi01, vi00, 0\n"
        "\tiaddiu vi02, vi00, 4\n"
        "\tiaddiu vi03, vi00, 32\n"
        "\tlq.xyz vf21, 64(vi00)\n"
        "loop_lid:\n"
        "\t--LoopCS 1,1\n"
        "\tlq.xyz vf20, -1(vi03)\n"
        "\tdiv q, vf20[x], vf21[x]\n"
        "\tmulq.xyz vf02, vf20, q\n"
        "\tadd.xyz vf10, vf20, vf00\n"
        "\tadd.xyz vf11, vf20, vf00\n"
        "\tdiv q, vf10[x], vf21[x]\n"
        "\tadd.xyz vf12, vf20, vf00\n"
        "\tadd.xyz vf13, vf20, vf00\n"
        "\tmulq.xyz vf05, vf20, q\n"
        "\tsq.xyz vf05, 0(vi03)\n"
        "\tiaddiu vi03, vi03, 1\n"
        "\tiaddiu vi01, vi01, 1\n"
        "\tibne vi01, vi02, loop_lid\n"
        "\t--exit\n"
        "\t--endexit\n";

    // The same loop, loading through vi04 = vi03 - 1: the address the
    // previous iteration stored at, reached through another register.
    const char* kHoistedLoadThroughOtherRegister =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmMemoryAlias\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tiaddiu vi01, vi00, 0\n"
        "\tiaddiu vi02, vi00, 4\n"
        "\tiaddiu vi03, vi00, 32\n"
        "\tisubiu vi04, vi03, 1\n"
        "\tlq.xyz vf21, 64(vi00)\n"
        "loop_lid:\n"
        "\t--LoopCS 1,1\n"
        "\tlq.xyz vf20, 0(vi04)\n"
        "\tdiv q, vf20[x], vf21[x]\n"
        "\tmulq.xyz vf02, vf20, q\n"
        "\tadd.xyz vf10, vf20, vf00\n"
        "\tadd.xyz vf11, vf20, vf00\n"
        "\tdiv q, vf10[x], vf21[x]\n"
        "\tadd.xyz vf12, vf20, vf00\n"
        "\tadd.xyz vf13, vf20, vf00\n"
        "\tmulq.xyz vf05, vf20, q\n"
        "\tsq.xyz vf05, 0(vi03)\n"
        "\tiaddiu vi03, vi03, 1\n"
        "\tiaddiu vi04, vi04, 1\n"
        "\tiaddiu vi01, vi01, 1\n"
        "\tibne vi01, vi02, loop_lid\n"
        "\t--exit\n"
        "\t--endexit\n";

    ::test::RunResult compile(const char* source)
    {
        return ::test::run_openvcl(std::vector<std::string>(), source);
    }

    // The emitted instructions, upper and lower halves apart: code lines are
    // indented by 20 spaces, and padding separates the two halves.
    std::vector<std::string> instructions(const std::string& vsm)
    {
        std::vector<std::string> result;
        std::stringstream stream(vsm);
        std::string line;
        while (std::getline(stream, line))
        {
            if (line.compare(0, 20, std::string(20, ' ')) != 0)
                continue;
            std::string::size_type start = line.find_first_not_of(' ');
            while (start != std::string::npos)
            {
                const std::string::size_type end = line.find("  ", start);
                result.push_back(line.substr(start, end == std::string::npos ? end : end - start));
                start = line.find_first_not_of(' ', end);
            }
        }
        return result;
    }

    // The position of the first instruction with the given opcode and
    // memory operand, or -1.
    int position(const std::vector<std::string>& code, const std::string& opcode, const std::string& operand)
    {
        for (std::vector<std::string>::const_iterator i = code.begin(); i != code.end(); ++i)
        {
            if (i->compare(0, opcode.length(), opcode) != 0)
                continue;
            if (i->length() > opcode.length() && (*i)[opcode.length()] != ' ' && (*i)[opcode.length()] != '.')
                continue;
            if (i->length() >= operand.length()
                && i->compare(i->length() - operand.length(), operand.length(), operand) == 0)
                return static_cast<int>(i - code.begin());
        }
        return -1;
    }
}

TEST_CASE("openvcl: a store through another register stays behind a store to its address")
{
    ::test::RunResult result = compile(kStoreThenStore);
    REQUIRE(result.exit_code == 0);

    const std::vector<std::string> code = instructions(result.stdout_data);
    const int first = position(code, "sq", ", 1(VI01)");
    const int second = position(code, "sq", ", 901(VI00)");
    REQUIRE(first >= 0);
    REQUIRE(second >= 0);
    CHECK(first < second);
}

TEST_CASE("openvcl: a load through another register stays behind a store to its address")
{
    ::test::RunResult result = compile(kStoreThenLoad);
    REQUIRE(result.exit_code == 0);

    const std::vector<std::string> code = instructions(result.stdout_data);
    const int store = position(code, "sq", ", 1(VI01)");
    const int load = position(code, "lq", ", 901(VI00)");
    REQUIRE(store >= 0);
    REQUIRE(load >= 0);
    CHECK(store < load);
}

TEST_CASE("openvcl: a load through the same register at another offset still moves ahead")
{
    ::test::RunResult result = compile(kStoreThenLoadApart);
    REQUIRE(result.exit_code == 0);

    const std::vector<std::string> code = instructions(result.stdout_data);
    const int store = position(code, "sq", ", 1(VI01)");
    const int load = position(code, "lq", ", 2(VI01)");
    REQUIRE(store >= 0);
    REQUIRE(load >= 0);
    CHECK(load < store);
}

TEST_CASE("openvcl: a loop whose first load reads the previous iteration's store is not pipelined")
{
    ::test::RunResult result = compile(kHoistedLoadOfStoredAddress);
    REQUIRE(result.exit_code == 0);

    // Pipelining this loop peels its first iteration into a prolog.
    CHECK(result.stdout_data.find("loop_lid__PROLOG:") == std::string::npos);
    const std::vector<std::string> code = instructions(result.stdout_data);
    CHECK(position(code, "lq.xyz", ", -1(VI03)") >= 0);
    CHECK(position(code, "sq.xyz", ", 0(VI03)") >= 0);
}

TEST_CASE("openvcl: a loop whose first load reads the previous iteration's store through another register is not pipelined")
{
    ::test::RunResult result = compile(kHoistedLoadThroughOtherRegister);
    REQUIRE(result.exit_code == 0);

    CHECK(result.stdout_data.find("loop_lid__PROLOG:") == std::string::npos);
    const std::vector<std::string> code = instructions(result.stdout_data);
    CHECK(position(code, "lq.xyz", ", 0(VI04)") >= 0);
    CHECK(position(code, "sq.xyz", ", 0(VI03)") >= 0);
}
