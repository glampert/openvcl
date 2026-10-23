// Regression tests: a pipelined loop leaves alone the registers of values
// live across it.
//
// The software pipeliner takes scratch registers for the values it rotates
// from VF31 down, skipping the registers the loop body names.  A value the
// body does not name, but that is read after the loop or on the next trip of
// an enclosing loop, lost its register to a scratch.  ps2gl's general program
// lost material_spec and vert_to_eye that way in its directional light loop.

#include "test_harness.h"
#include "openvcl_runner.h"

#include <sstream>
#include <string>
#include <vector>

namespace
{
    // The pipeliner rotates vf03 through a scratch register.
    const char* kLoop =
        "loop_lid:\n"
        "\t--LoopCS 1,1\n"
        "\tlq.xyzw vf01, 0(vi01)\n"
        "\tmul.xyzw vf03, vf01, vf02\n"
        "\tdiv q, vf00w, vf03w\n"
        "\tmulq.xyz vf03, vf03, q\n"
        "\tsq.xyz vf03, 0(vi02)\n"
        "\tadd.xyz vf10, vf00, vf00\n"
        "\tadd.xyz vf11, vf00, vf00\n"
        "\tadd.xyz vf12, vf00, vf00\n"
        "\tadd.xyz vf13, vf00, vf00\n"
        "\tadd.xyz vf14, vf00, vf00\n"
        "\tiaddiu vi01, vi01, 1\n"
        "\tiaddiu vi02, vi02, 1\n"
        "\tibne vi01, vi04, loop_lid\n";

    // vf31 is set before the loop and stored after it.
    std::string readAfterLoopSource()
    {
        return std::string(
            "\t.init_vf_all\n"
            "\t.init_vi_all\n"
            "\t--enter\n"
            "\t--endenter\n"
            "\tiaddiu vi01, vi00, 0\n"
            "\tiaddiu vi02, vi00, 0\n"
            "\tiaddiu vi04, vi00, 3\n"
            "\tmove.xyzw vf02, vf00\n"
            "\tlq.xyzw vf31, 7(vi00)\n")
            + kLoop +
            "\tsq.xyzw vf31, 8(vi00)\n"
            "\t--exit\n"
            "\t--endexit\n";
    }

    // vf31 is set before the outer loop and stored on each of its trips,
    // before the inner loop.
    std::string readInOuterLoopSource()
    {
        return std::string(
            "\t.init_vf_all\n"
            "\t.init_vi_all\n"
            "\t--enter\n"
            "\t--endenter\n"
            "\tlq.xyzw vf31, 7(vi00)\n"
            "\tiaddiu vi05, vi00, 2\n"
            "outer_lid:\n"
            "\tsq.xyzw vf31, 8(vi05)\n"
            "\tiaddiu vi01, vi00, 0\n"
            "\tiaddiu vi02, vi00, 0\n"
            "\tiaddiu vi04, vi00, 3\n"
            "\tmove.xyzw vf02, vf00\n")
            + kLoop +
            "\tiaddi vi05, vi05, -1\n"
            "\tibne vi05, vi00, outer_lid\n"
            "\t--exit\n"
            "\t--endexit\n";
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
                result.push_back(line.substr(start, end == std::string::npos ? std::string::npos : end - start));
                start = (end == std::string::npos) ? end : line.find_first_not_of(' ', end);
            }
        }
        return result;
    }

    // The instructions that write `reg`: it is their first operand, and they
    // are not stores.
    std::vector<std::string> writing(const std::vector<std::string>& code, const std::string& reg)
    {
        std::vector<std::string> result;
        for (std::vector<std::string>::const_iterator i = code.begin(); i != code.end(); ++i)
        {
            const std::string::size_type space = i->find(' ');
            if (space != std::string::npos && i->compare(space + 1, reg.size() + 1, reg + ",") == 0
                && i->compare(0, 2, "sq") != 0)
                result.push_back(*i);
        }
        return result;
    }
}

TEST_CASE("Software pipeline: a value read after a pipelined loop keeps its register")
{
    ::test::RunResult result = ::test::run_openvcl(std::vector<std::string>(), readAfterLoopSource());
    REQUIRE(result.exit_code == 0);
    CHECK(result.stdout_data.find("loop_lid__PROLOG:") != std::string::npos);

    const std::vector<std::string> writes = writing(instructions(result.stdout_data), "VF31");
    REQUIRE(writes.size() == 1);
    CHECK(writes[0] == "lq VF31, 7(VI00)");
}

TEST_CASE("Software pipeline: a value read on each trip of an outer loop keeps its register")
{
    ::test::RunResult result = ::test::run_openvcl(std::vector<std::string>(), readInOuterLoopSource());
    REQUIRE(result.exit_code == 0);
    CHECK(result.stdout_data.find("loop_lid__PROLOG:") != std::string::npos);

    const std::vector<std::string> writes = writing(instructions(result.stdout_data), "VF31");
    REQUIRE(writes.size() == 1);
    CHECK(writes[0] == "lq VF31, 7(VI00)");
}
