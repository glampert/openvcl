// Regression tests: openvcl's diagnostics are easy to find and act on.
//
// Errors were printed as "file(line) : message", without the "error:" tag
// warnings already had, so searching a build log for "error:" found
// nothing.

#include "test_harness.h"
#include "openvcl_runner.h"

#include <string>
#include <vector>

namespace
{
    bool contains(const std::string& text, const std::string& needle)
    {
        return text.find(needle) != std::string::npos;
    }

    std::string program(const std::string& directives, const std::string& body)
    {
        return directives +
            "\t.syntax new\n"
            "\t.name vsmDiagnostics\n"
            "\t--enter\n"
            "\t--endenter\n"
            + body +
            "\t--exit\n"
            "\t--endexit\n";
    }

    const char* kAllRegisters = "\t.init_vf_all\n\t.init_vi_all\n";

    // Line 8 passes clipw a field other than W.
    std::string invalidProgram()
    {
        return program(kAllRegisters,
            "\tlq fA, 1(vi00)\n"
            "\tclipw.xyz fA, fA[x]\n"
            "\tfcand vi01, 0x3f\n"
            "\tisw.x vi01, 2(vi00)\n");
    }

    std::string validProgram()
    {
        return program(kAllRegisters,
            "\tlq fA, 1(vi00)\n"
            "\tsq fA, 2(vi00)\n");
    }
}

TEST_CASE("openvcl: an error at a source line is tagged error:")
{
    ::test::RunResult r = ::test::run_openvcl(std::vector<std::string>(), invalidProgram());

    CHECK(r.exit_code != 0);
    CHECK(contains(r.stderr_data, "(8) : error: Invalid argument (clipw.xyz fA, >>> fA[x] <<<"));
}

TEST_CASE("openvcl: an error without a location is tagged error:")
{
    std::vector<std::string> args;
    args.push_back("-o");
    args.push_back("/nonexistent-openvcl-dir/out.vsm");
    ::test::RunResult r = ::test::run_openvcl(args, validProgram());

    CHECK(r.exit_code != 0);
    CHECK(contains(r.stderr_data, "openvcl: error: Could not open output file"));
}
