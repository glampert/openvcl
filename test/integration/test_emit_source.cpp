// Regression tests: -c annotates the code without changing it.
//
// -c writes each instruction's source line above it as a "; Line N:"
// comment.  The comments went into the same list as the code, so the check
// for padding already in place before a branch found a comment instead,
// and added another nop pair: the program changed under -c, and the
// branch's comment ended up above that nop.  Instructions emitted as a
// pair, or with waitq/waitp, got no comment.

#include "test_harness.h"
#include "openvcl_runner.h"

#include <sstream>
#include <string>
#include <vector>

namespace
{
    // The branch reads a fresh ilw result, so latency padding precedes it.
    const char* kLoadThenBranch =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmEmitSource\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tilw.x iFlag, 0(vi00)\n"
        "\tibeq iFlag, vi00, skip\n"
        "\tiaddiu iValue, vi00, 6\n"
        "\tisw.x iValue, 1(vi00)\n"
        "skip:\n"
        "\tiaddiu iDone, vi00, 1\n"
        "\tisw.x iDone, 2(vi00)\n"
        "\t--exit\n"
        "\t--endexit\n";

    // Upper/lower pairs, a mulq issued with waitq, a move emitted as max.
    const char* kPairedLoop =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmEmitSource\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tiaddiu iCount, vi00, 4\n"
        "\tlq fA, 0(vi00)\n"
        "\tlq fB, 1(vi00)\n"
        "loop:\n"
        "\tilw.x iFlag, 2(vi00)\n"
        "\tadd.xyz fC, fA, fB\n"
        "\tdiv q, vf00[w], fA[w]\n"
        "\tmulq.xyz fD, fC, q\n"
        "\tibeq iFlag, vi00, skip\n"
        "\tmove.xyz fE, fD\n"
        "\tsq.xyz fE, 3(vi00)\n"
        "skip:\n"
        "\tiaddi iCount, iCount, -1\n"
        "\tsq.xyz fD, 4(vi00)\n"
        "\tibne iCount, vi00, loop\n"
        "\t--exit\n"
        "\t--endexit\n";

    // The store moves into the loop branch's delay slot.
    const char* kDelaySlotLoop =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tiaddiu vi01, vi00, 0\n"
        "\tiaddiu vi02, vi00, 3\n"
        "loop_lid:\n"
        "\tlq.xyzw vf01, 0(vi00)\n"
        "\tadd.xyzw vf02, vf01, vf01\n"
        "\tsq.xyzw vf02, 0(vi01)\n"
        "\tiaddiu vi01, vi01, 1\n"
        "\tibne vi01, vi02, loop_lid\n"
        "\t--exit\n"
        "\t--endexit\n";

    ::test::RunResult compile(const char* source, bool emitSource)
    {
        std::vector<std::string> args;
        if (emitSource)
            args.push_back("-c");
        return ::test::run_openvcl(args, source);
    }

    std::vector<std::string> splitLines(const std::string& text)
    {
        std::vector<std::string> result;
        std::stringstream stream(text);
        std::string line;
        while (std::getline(stream, line))
            result.push_back(line);
        return result;
    }

    bool isComment(const std::string& line)
    {
        const std::string::size_type first = line.find_first_not_of(" \t");
        return first != std::string::npos && line[first] == ';';
    }

    std::string stripComments(const std::string& text)
    {
        std::string result;
        const std::vector<std::string> lines = splitLines(text);
        for (std::vector<std::string>::const_iterator i = lines.begin(); i != lines.end(); ++i)
        {
            if (!isComment(*i))
                result += *i + "\n";
        }
        return result;
    }

    // An emitted instruction pair other than nop/nop (code lines are indented
    // by 20 spaces; labels and directives are not).
    bool isInstruction(const std::string& line)
    {
        if (line.compare(0, 20, std::string(20, ' ')) != 0 || isComment(line))
            return false;
        std::stringstream words(line);
        std::string word;
        while (words >> word)
        {
            if (word.compare(0, 3, "nop") != 0)
                return true;
        }
        return false;
    }

    unsigned int uncommentedInstructions(const std::string& text)
    {
        unsigned int count = 0;
        const std::vector<std::string> lines = splitLines(text);
        for (std::vector<std::string>::size_type i = 0; i < lines.size(); ++i)
        {
            if (isInstruction(lines[i]) && (i == 0 || !isComment(lines[i - 1])))
                ++count;
        }
        return count;
    }

    // The comments directly above the first line that contains `code`.
    std::vector<std::string> commentsAbove(const std::string& text, const std::string& code)
    {
        std::vector<std::string> result;
        const std::vector<std::string> lines = splitLines(text);
        for (std::vector<std::string>::size_type i = 0; i < lines.size(); ++i)
        {
            if (isComment(lines[i]) || lines[i].find(code) == std::string::npos)
                continue;
            for (std::vector<std::string>::size_type j = i; j > 0 && isComment(lines[j - 1]); --j)
                result.insert(result.begin(), lines[j - 1]);
            break;
        }
        return result;
    }

    bool contains(const std::string& text, const std::string& needle)
    {
        return text.find(needle) != std::string::npos;
    }
}

TEST_CASE("openvcl: -c leaves the padding before a branch as it is")
{
    ::test::RunResult plain = compile(kLoadThenBranch, false);
    ::test::RunResult annotated = compile(kLoadThenBranch, true);

    REQUIRE(plain.exit_code == 0);
    REQUIRE(annotated.exit_code == 0);
    CHECK(stripComments(annotated.stdout_data) == plain.stdout_data);
    CHECK(uncommentedInstructions(annotated.stdout_data) == 0);
}

TEST_CASE("openvcl: -c comments both halves of a pair and instructions issued with waitq")
{
    ::test::RunResult plain = compile(kPairedLoop, false);
    ::test::RunResult annotated = compile(kPairedLoop, true);

    REQUIRE(plain.exit_code == 0);
    REQUIRE(annotated.exit_code == 0);
    CHECK(stripComments(annotated.stdout_data) == plain.stdout_data);
    CHECK(uncommentedInstructions(annotated.stdout_data) == 0);

    // add.xyz (upper) and ilw.x (lower) issue together
    const std::vector<std::string> pair = commentsAbove(annotated.stdout_data, "ilw.x VI02, 2(VI00)");
    REQUIRE(pair.size() == 2);
    CHECK(contains(pair[0], "Line 12:") && contains(pair[0], "add.xyz fC, fA, fB"));
    CHECK(contains(pair[1], "Line 11:") && contains(pair[1], "ilw.x iFlag, 2(vi00)"));

    const std::vector<std::string> wait = commentsAbove(annotated.stdout_data, "waitq");
    REQUIRE(wait.size() == 1);
    CHECK(contains(wait[0], "Line 14:") && contains(wait[0], "mulq.xyz fD, fC, q"));

    const std::vector<std::string> move = commentsAbove(annotated.stdout_data, "max.xyz");
    REQUIRE(move.size() == 1);
    CHECK(contains(move[0], "Line 16:") && contains(move[0], "move.xyz fE, fD"));
}

TEST_CASE("openvcl: -c leaves a loop branch and its filled delay slot as they are")
{
    ::test::RunResult plain = compile(kDelaySlotLoop, false);
    ::test::RunResult annotated = compile(kDelaySlotLoop, true);

    REQUIRE(plain.exit_code == 0);
    REQUIRE(annotated.exit_code == 0);
    CHECK(stripComments(annotated.stdout_data) == plain.stdout_data);
    CHECK(uncommentedInstructions(annotated.stdout_data) == 0);

    const std::vector<std::string> filler = commentsAbove(annotated.stdout_data, "sq VF02, -1(VI01)");
    REQUIRE(filler.size() == 1);
    CHECK(contains(filler[0], "Line 10:") && contains(filler[0], "sq.xyzw vf02, 0(vi01)"));
}
