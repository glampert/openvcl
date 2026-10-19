// Regression tests: CLIPw's second operand is always the W component.
//
// SCE VCL accepts `clipw.xyz a, b` and reads b's W, the only component the
// instruction can encode.  openvcl required an explicit `b[w]`, so sources
// written for SCE VCL failed with "Invalid argument" (9 of the Tyra engine's
// 15 microprograms do this).  The bare form must now compile to exactly the
// same instruction as `b[w]`, and a field other than W, which dvp-as would
// reject, is still an error.

#include "test_harness.h"
#include "openvcl_runner.h"

#include <string>
#include <vector>

namespace
{
    std::string program(const std::string& clip)
    {
        return
            "\t.init_vf_all\n"
            "\t.init_vi_all\n"
            "\t.syntax new\n"
            "\t.name vsmClipImplicitW\n"
            "\t--enter\n"
            "\t--endenter\n"
            "\tlq fA, 1(vi00)\n"
            "\t" + clip + "\n"
            "\tfcand vi01, 0x3f\n"
            "\tisw.x vi01, 2(vi00)\n"
            "\t--exit\n"
            "\t--endexit\n";
    }

    ::test::RunResult compile(const std::string& clip)
    {
        std::vector<std::string> args;
        args.push_back("-o");
        args.push_back("-");
        return ::test::run_openvcl(args, program(clip));
    }
}

TEST_CASE("openvcl: clipw without a field reads the W component")
{
    ::test::RunResult bare = compile("clipw.xyz fA, fA");
    ::test::RunResult explicitW = compile("clipw.xyz fA, fA[w]");

    CHECK(bare.exit_code == 0);
    CHECK(explicitW.exit_code == 0);
    CHECK(bare.stdout_data.find("clipw.xyz VF01, VF01w") != std::string::npos);
    CHECK(bare.stdout_data == explicitW.stdout_data);
}

TEST_CASE("openvcl: clipw without a dest field or a source field is .xyz and W")
{
    ::test::RunResult bare = compile("clipw fA, fA");

    CHECK(bare.exit_code == 0);
    CHECK(bare.stdout_data.find("clipw.xyz VF01, VF01w") != std::string::npos);
}

TEST_CASE("openvcl: clipw rejects a source field other than W")
{
    ::test::RunResult r = compile("clipw.xyz fA, fA[x]");

    CHECK(r.exit_code != 0);
    CHECK(r.stderr_data.find("Invalid argument") != std::string::npos);
}
