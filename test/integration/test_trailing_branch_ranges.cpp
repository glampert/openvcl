// Regression test: a branch that ends the program keeps live ranges inside
// the program.
//
// The register allocator ends a path's stretch of the program at the token
// after a branch.  When the branch was the program's last token, as in
// ps2gl's programs that loop back with `b` and have no --exit block, that
// token was the end of the token list: dereferencing it read whatever lay
// past the list, and the ranges of values live across the branch ended at
// that number.

#include "test_harness.h"
#include "openvcl_runner.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace
{
    // fA and iN are live across the b on the last line, line 11.
    const char* kTrailingBranch =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tlq fA, 0(vi00)\n"
        "\tiaddiu iN, vi00, 1\n"
        "lLoop:\n"
        "\tsq fA, 1(iN)\n"
        "\tiaddiu iN, iN, 1\n"
        "\tb lLoop\n";

    const unsigned long kLastLine = 11;

    // The ends of the [start-stop] ranges --show-reg-alloc prints.
    std::vector<unsigned long> rangeEnds(const std::string& report)
    {
        std::vector<unsigned long> result;
        std::string::size_type open = report.find('[');
        while (open != std::string::npos)
        {
            const std::string::size_type dash = report.find('-', open);
            const std::string::size_type close = report.find(']', open);
            if (dash != std::string::npos && close != std::string::npos && dash < close
                && report.find_first_not_of("0123456789", open + 1) == dash)
                result.push_back(std::strtoul(report.c_str() + dash + 1, NULL, 10));
            open = report.find('[', open + 1);
        }
        return result;
    }
}

TEST_CASE("openvcl: values live across a branch that ends the program keep ranges inside it")
{
    std::vector<std::string> args;
    args.push_back("--show-reg-alloc");
    ::test::RunResult result = ::test::run_openvcl(args, kTrailingBranch);
    REQUIRE(result.exit_code == 0);

    const std::vector<unsigned long> ends = rangeEnds(result.stderr_data);
    REQUIRE(!ends.empty());
    for (unsigned int i = 0; i < ends.size(); ++i)
        CHECK(ends[i] <= kLastLine);
}
