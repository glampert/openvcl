// Regression tests: openvcl's diagnostics are easy to find and act on.
//
// Errors were printed as "file(line) : message", without the "error:" tag
// warnings already had, so searching a build log for "error:" found
// nothing.  Running out of registers said only "Register allocation ran out
// of registers", with no variable, register type or location.  And a failed
// build still wrote its output file, which then looked up to date to make.

#include "test_harness.h"
#include "openvcl_runner.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
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

TEST_CASE("openvcl: running out of VI registers names the variable and where it is set")
{
    // only two VI registers for three values live at once; line 9 sets the third
    ::test::RunResult r = ::test::run_openvcl(std::vector<std::string>(), program(
        "\t.init_vf_all\n\t.init_vi vi01, vi02\n",
        "\tilw.x iA, 0(vi00)\n"
        "\tilw.x iB, 1(vi00)\n"
        "\tilw.x iC, 2(vi00)\n"
        "\tiadd iD, iA, iB\n"
        "\tiadd iE, iD, iC\n"
        "\tisw.x iE, 3(vi00)\n"));

    CHECK(r.exit_code != 0);
    CHECK(contains(r.stderr_data, "(9) : error: Register allocation ran out of VI registers (2 available) for iC (ilw.x >>> iC <<<, 2(vi00))"));
}

TEST_CASE("openvcl: running out of VF registers names the variable and where it is set")
{
    ::test::RunResult r = ::test::run_openvcl(std::vector<std::string>(), program(
        "\t.init_vf vf01-vf02\n\t.init_vi_all\n",
        "\tlq fA, 0(vi00)\n"
        "\tlq fB, 1(vi00)\n"
        "\tlq fC, 2(vi00)\n"
        "\tadd fD, fA, fB\n"
        "\tadd fE, fD, fC\n"
        "\tsq fE, 3(vi00)\n"));

    CHECK(r.exit_code != 0);
    CHECK(contains(r.stderr_data, "(9) : error: Register allocation ran out of VF registers (2 available) for fC (lq >>> fC <<<"));
}

TEST_CASE("openvcl: a failed build writes no output")
{
    char path[] = "/tmp/openvcl_diag_XXXXXX.vsm";
    int fd = mkstemps(path, 4);
    REQUIRE(fd >= 0);
    const char stale[] = "stale output from an earlier build\n";
    REQUIRE(write(fd, stale, sizeof(stale) - 1) == static_cast<ssize_t>(sizeof(stale) - 1));
    close(fd);

    std::vector<std::string> args;
    args.push_back("-o");
    args.push_back(path);
    ::test::RunResult r = ::test::run_openvcl(args, invalidProgram());

    std::ifstream f(path);
    std::stringstream contents;
    contents << f.rdbuf();
    std::remove(path);

    CHECK(r.exit_code != 0);
    CHECK(contents.str() == stale);

    ::test::RunResult toStdout = ::test::run_openvcl(std::vector<std::string>(), invalidProgram());
    CHECK(toStdout.exit_code != 0);
    CHECK(toStdout.stdout_data.empty());
}
