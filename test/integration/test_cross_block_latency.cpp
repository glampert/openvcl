// Regression tests: results still in flight at a label or a branch are
// waited for on every path.
//
// The scheduler pads each basic block for the results still in flight from
// the code before it in the program, and for nothing a branch into the
// block leaves in flight.  The clip flags from a clipw on the other side
// of a join, a register loaded just before a loop's label or in a branch's
// delay slot, a VI loaded before a join and tested right after it, a lane
// moved into a vertex colour on the lit path and stored at the join: all
// were read a cycle or more early on one of the paths.

#include "test_harness.h"
#include "openvcl_runner.h"

#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    // Each path's clipw is a different distance from the fcand after the join.
    const char* kClipJoin =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmCrossBlockLatency\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tilw.x iFmt, 0(vi00)\n"
        "\tlq fA, 1(vi00)\n"
        "\tibeq iFmt, vi00, lFmtB\n"
        "\tclipw.xyz fA, fA[w]\n"
        "\tb lJoin\n"
        "lFmtB:\n"
        "\tclipw.xyz fA, fA[w]\n"
        "lJoin:\n"
        "\tfcand vi01, 0x3f\n"
        "\tiaddiu iOut, vi01, 0\n"
        "\tisw.x iOut, 2(vi00)\n"
        "\t--exit\n"
        "\t--endexit\n";

    const char* kMacJoin =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmCrossBlockLatency\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tilw.x iFmt, 0(vi00)\n"
        "\tlq fA, 1(vi00)\n"
        "\tibeq iFmt, vi00, lFmtB\n"
        "\tsub.xyz fB, fA, vf00\n"
        "\tb lJoin\n"
        "lFmtB:\n"
        "\tadd.xyz fB, fA, vf00\n"
        "lJoin:\n"
        "\tfmand iM, iFmt\n"
        "\tisw.x iM, 2(vi00)\n"
        "\tsq.xyz fB, 3(vi00)\n"
        "\t--exit\n"
        "\t--endexit\n";

    // fA is loaded again right before the join on the path that falls into it.
    const char* kLoadBeforeJoin =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmCrossBlockLatency\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tilw.x iC, 0(vi00)\n"
        "\tlq fA, 3(vi00)\n"
        "\tibeq iC, vi00, lJoin\n"
        "\tlq fA, 1(vi00)\n"
        "lJoin:\n"
        "\tadd fB, fA, fA\n"
        "\tsq fB, 2(vi00)\n"
        "\t--exit\n"
        "\t--endexit\n";

    // fA is loaded right before the branch to the join, or in its delay slot.
    const char* kLoadBeforeBranch =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmCrossBlockLatency\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tilw.x iC, 0(vi00)\n"
        "\tlq fA, 3(vi00)\n"
        "\tibeq iC, vi00, lB\n"
        "\tlq fA, 1(vi00)\n"
        "\tb lJoin\n"
        "lB:\n"
        "\tiaddiu iT, vi00, 1\n"
        "\tisw.x iT, 4(vi00)\n"
        "lJoin:\n"
        "\tadd fB, fA, fA\n"
        "\tsq fB, 2(vi00)\n"
        "\t--exit\n"
        "\t--endexit\n";

    // fA is loaded right before the loop, and late in each trip.
    const char* kLoadBeforeLoop =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmCrossBlockLatency\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tiaddiu iN, vi00, 4\n"
        "\tlq fA, 1(vi00)\n"
        "lLoop:\n"
        "\tadd fB, fA, fA\n"
        "\tsq fB, 2(vi00)\n"
        "\tiaddi iN, iN, -1\n"
        "\tlq fA, 3(vi00)\n"
        "\tibne iN, vi00, lLoop\n"
        "\t--exit\n"
        "\t--endexit\n";

    // iD is loaded right before the join and tested right after it.
    const char* kIntegerLoadBeforeJoin =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmCrossBlockLatency\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tilw.x iC, 0(vi00)\n"
        "\tilw.y iD, 0(vi00)\n"
        "\tibeq iC, vi00, lJoin\n"
        "\tilw.z iD, 1(vi00)\n"
        "lJoin:\n"
        "\tibne iD, vi00, lOut\n"
        "\tisw.x iD, 3(vi00)\n"
        "lOut:\n"
        "\tisw.y iD, 4(vi00)\n"
        "\t--exit\n"
        "\t--endexit\n";

    // quake2's vertex colour: cleared before the branch, and on the lit path
    // rewritten lane by lane right before the store at the join.
    const char* kMaskedWritesBeforeJoin =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmCrossBlockLatency\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tlq fX, 1(vi00)\n"
        "\tilw.x iC, 0(vi00)\n"
        "\tmove vCol, vf00\n"
        "\tibeq iC, vi00, lSkip\n"
        "\tmove.xyz vCol, vf00\n"
        "\tmove.w vCol, fX\n"
        "lSkip:\n"
        "\tsq vCol, 2(vi00)\n"
        "\t--exit\n"
        "\t--endexit\n";

    // Nothing read after the join is in flight on either path.
    const char* kJoinWithoutHazard =
        "\t.init_vf_all\n"
        "\t.init_vi_all\n"
        "\t.syntax new\n"
        "\t.name vsmCrossBlockLatency\n"
        "\t--enter\n"
        "\t--endenter\n"
        "\tilw.x iFmt, 0(vi00)\n"
        "\tlq fA, 1(vi00)\n"
        "\tibeq iFmt, vi00, lFmtB\n"
        "\tiaddiu iT, vi00, 1\n"
        "\tb lJoin\n"
        "lFmtB:\n"
        "\tiaddiu iT, vi00, 2\n"
        "lJoin:\n"
        "\tisw.x iT, 2(vi00)\n"
        "\tsq fA, 3(vi00)\n"
        "\t--exit\n"
        "\t--endexit\n";

    // An emitted instruction pair, one issue cycle: its upper and lower
    // instructions, and the pairs that can issue next.
    struct Pair
    {
        std::string upper;
        std::string lower;
        std::vector<unsigned int> next;
    };

    std::string mnemonic(const std::string& instruction)
    {
        return instruction.substr(0, instruction.find(' '));
    }

    // The register an instruction names first: what a load or an operation writes.
    std::string destination(const std::string& instruction)
    {
        const std::string::size_type space = instruction.find(' ');
        if (space == std::string::npos)
            return std::string();
        const std::string::size_type comma = instruction.find(',', space);
        return instruction.substr(space + 1, comma == std::string::npos ? std::string::npos : comma - space - 1);
    }

    bool isBranch(const std::string& instruction)
    {
        const std::string op = mnemonic(instruction);
        return op == "b" || op == "bal" || op.compare(0, 2, "ib") == 0 || op == "jr" || op == "jalr";
    }

    // The emitted pairs and the control flow between them: a branch takes
    // effect after its delay slot, and the pair after one with the E bit is
    // the last to run.
    std::vector<Pair> parse(const std::string& vsm, std::map<std::string, unsigned int>& labels)
    {
        std::vector<Pair> pairs;
        std::vector<std::string> pending;
        std::stringstream stream(vsm);
        std::string line;
        while (std::getline(stream, line))
        {
            if (line.compare(0, 20, std::string(20, ' ')) == 0)
            {
                Pair pair;
                const std::string::size_type start = line.find_first_not_of(' ');
                const std::string::size_type split = line.find("  ", start);
                pair.upper = line.substr(start, split == std::string::npos ? std::string::npos : split - start);
                if (split != std::string::npos)
                    pair.lower = line.substr(line.find_first_not_of(' ', split));
                for (unsigned int i = 0; i < pending.size(); ++i)
                    labels[pending[i]] = static_cast<unsigned int>(pairs.size());
                pending.clear();
                pairs.push_back(pair);
            }
            else if (!line.empty() && line[0] != ' ' && line[0] != '\t' && line[line.size() - 1] == ':')
                pending.push_back(line.substr(0, line.size() - 1));
        }

        for (unsigned int i = 0; i < pairs.size(); ++i)
        {
            if (i > 0 && pairs[i - 1].upper.find("[E]") != std::string::npos)
                continue;
            if (i > 0 && isBranch(pairs[i - 1].lower))
            {
                const std::string& branch = pairs[i - 1].lower;
                const std::map<std::string, unsigned int>::const_iterator target = labels.find(branch.substr(branch.rfind(' ') + 1));
                if (target != labels.end())
                    pairs[i].next.push_back(target->second);
                if (mnemonic(branch).compare(0, 2, "ib") == 0 && i + 1 < pairs.size())
                    pairs[i].next.push_back(i + 1);
            }
            else if (i + 1 < pairs.size())
                pairs[i].next.push_back(i + 1);
        }
        return pairs;
    }

    // The first pair with this instruction in either half.
    unsigned int find(const std::vector<Pair>& pairs, const std::string& op)
    {
        for (unsigned int i = 0; i < pairs.size(); ++i)
        {
            if (mnemonic(pairs[i].upper) == op || mnemonic(pairs[i].lower) == op)
                return i;
        }
        return static_cast<unsigned int>(pairs.size());
    }

    // The fewest cycles from a pair for which `produces` holds to the
    // consumer over any path, or -1 when none lies within `limit`.
    int nearestProducer(const std::vector<Pair>& pairs, unsigned int consumer, bool (*produces)(const Pair&), int limit)
    {
        std::vector< std::vector<unsigned int> > previous(pairs.size());
        for (unsigned int i = 0; i < pairs.size(); ++i)
        {
            for (unsigned int n = 0; n < pairs[i].next.size(); ++n)
                previous[pairs[i].next[n]].push_back(i);
        }

        int nearest = -1;
        std::map<unsigned int, int> seen;
        std::vector< std::pair<unsigned int, int> > work;
        for (unsigned int p = 0; p < previous[consumer].size(); ++p)
            work.push_back(std::make_pair(previous[consumer][p], 1));
        while (!work.empty())
        {
            const unsigned int i = work.back().first;
            const int distance = work.back().second;
            work.pop_back();
            if (distance >= limit || (seen.count(i) && seen[i] <= distance))
                continue;
            seen[i] = distance;
            if (produces(pairs[i]))
            {
                if (nearest < 0 || distance < nearest)
                    nearest = distance;
                continue;
            }
            for (unsigned int p = 0; p < previous[i].size(); ++p)
                work.push_back(std::make_pair(previous[i][p], distance + 1));
        }
        return nearest;
    }

    bool writesClipFlags(const Pair& pair)
    {
        return mnemonic(pair.upper).compare(0, 5, "clipw") == 0;
    }

    bool writesMacFlags(const Pair& pair)
    {
        return mnemonic(pair.upper) != "nop";
    }

    bool loadsVF01(const Pair& pair)
    {
        return mnemonic(pair.lower) == "lq" && destination(pair.lower) == "VF01";
    }

    bool loadsVI02(const Pair& pair)
    {
        return mnemonic(pair.lower).compare(0, 3, "ilw") == 0 && destination(pair.lower) == "VI02";
    }

    // The lane move: a lower instruction with a four-cycle latency.
    bool movesIntoVF02(const Pair& pair)
    {
        return mnemonic(pair.lower).compare(0, 4, "move") == 0 && destination(pair.lower) == "VF02";
    }

    // The field move, which the emitter may turn into an upper max.
    bool writesFieldsOfVF02(const Pair& pair)
    {
        return mnemonic(pair.upper) != "nop" && destination(pair.upper) == "VF02";
    }

    ::test::RunResult compile(const char* source)
    {
        return ::test::run_openvcl(std::vector<std::string>(), source);
    }
}

TEST_CASE("openvcl: the clip flags are read four cycles after the clipw on every path into a join")
{
    ::test::RunResult result = compile(kClipJoin);
    REQUIRE(result.exit_code == 0);

    std::map<std::string, unsigned int> labels;
    const std::vector<Pair> code = parse(result.stdout_data, labels);
    const unsigned int fcand = find(code, "fcand");
    REQUIRE(fcand < code.size());
    CHECK(nearestProducer(code, fcand, writesClipFlags, 4) == -1);
}

TEST_CASE("openvcl: the MAC flags are read four cycles after the operation on every path into a join")
{
    ::test::RunResult result = compile(kMacJoin);
    REQUIRE(result.exit_code == 0);

    std::map<std::string, unsigned int> labels;
    const std::vector<Pair> code = parse(result.stdout_data, labels);
    const unsigned int fmand = find(code, "fmand");
    REQUIRE(fmand < code.size());
    CHECK(nearestProducer(code, fmand, writesMacFlags, 4) == -1);
}

TEST_CASE("openvcl: a register loaded before a label or a branch is read four cycles later on every path")
{
    const char* sources[] = { kLoadBeforeJoin, kLoadBeforeBranch, kLoadBeforeLoop };
    for (unsigned int i = 0; i < 3; ++i)
    {
        ::test::RunResult result = compile(sources[i]);
        REQUIRE(result.exit_code == 0);

        std::map<std::string, unsigned int> labels;
        const std::vector<Pair> code = parse(result.stdout_data, labels);
        const unsigned int add = find(code, "add.xyzw");
        REQUIRE(add < code.size());
        CHECK(nearestProducer(code, add, loadsVF01, 4) == -1);
    }
}

TEST_CASE("openvcl: a VI loaded before a join is tested five cycles later on every path")
{
    ::test::RunResult result = compile(kIntegerLoadBeforeJoin);
    REQUIRE(result.exit_code == 0);

    std::map<std::string, unsigned int> labels;
    const std::vector<Pair> code = parse(result.stdout_data, labels);
    const unsigned int ibne = find(code, "ibne");
    REQUIRE(ibne < code.size());
    CHECK(nearestProducer(code, ibne, loadsVI02, 5) == -1);
}

TEST_CASE("openvcl: a vertex colour written lane by lane before a join is stored after the lanes land")
{
    ::test::RunResult result = compile(kMaskedWritesBeforeJoin);
    REQUIRE(result.exit_code == 0);

    std::map<std::string, unsigned int> labels;
    const std::vector<Pair> code = parse(result.stdout_data, labels);
    const unsigned int clear = find(code, "move");
    const unsigned int branch = find(code, "ibeq");
    const unsigned int store = find(code, "sq");
    REQUIRE(store < code.size());
    CHECK(clear < branch);
    CHECK(branch < store);
    CHECK(nearestProducer(code, store, movesIntoVF02, 5) == -1);
    CHECK(nearestProducer(code, store, writesFieldsOfVF02, 4) == -1);
}

TEST_CASE("openvcl: a join with nothing in flight on either path gets no padding")
{
    ::test::RunResult result = compile(kJoinWithoutHazard);
    REQUIRE(result.exit_code == 0);

    std::map<std::string, unsigned int> labels;
    const std::vector<Pair> code = parse(result.stdout_data, labels);
    REQUIRE(labels.count("lJoin") == 1);
    CHECK(mnemonic(code[labels["lJoin"]].lower) == "isw.x");
}
