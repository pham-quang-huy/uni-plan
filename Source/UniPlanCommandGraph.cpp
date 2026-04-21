#include "UniPlanCliConstants.h"
#include "UniPlanCommandHelp.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanJSONHelpers.h"
#include "UniPlanOptionTypes.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <algorithm>
#include <deque>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace UniPlan
{

// ===================================================================
// graph command (v0.98.0+) — walks typed topic + phase dependencies
// across all bundles and emits uni-plan-graph-v1 JSON.
//
// Graph shape:
//   - nodes: one per topic + one per phase that has outgoing or
//     incoming dependency edges.
//   - edges: {from, to, kind, note} describing a typed dependency.
//     `from` / `to` are node ids of the form "topic:<T>" or
//     "phase:<T>/<N>".
//
// Scope:
//   - no --topic: full corpus graph (every bundle, every declared dep).
//   - --topic <T>: focus on <T>'s reachable neighborhood, traversing
//     edges in both directions until --depth levels from <T>.
//   - --depth -1 (default when --topic is set): unlimited walk.
// ===================================================================

namespace
{

// Canonical node id helpers.
std::string TopicNodeID(const std::string &InTopic)
{
    return "topic:" + InTopic;
}
std::string PhaseNodeID(const std::string &InTopic, int InPhaseIndex)
{
    return "phase:" + InTopic + "/" + std::to_string(InPhaseIndex);
}

// One edge in the dependency graph.
struct FGraphEdge
{
    std::string mFrom;
    std::string mTo;
    std::string mKind; // bundle / phase / governance / external
    std::string mNote;
    std::string mTargetPath; // populated for governance/external
};

// One node in the dependency graph.
struct FGraphNode
{
    std::string mId;
    std::string mKind; // "topic" or "phase"
    std::string mTopic;
    int mPhaseIndex = -1; // -1 for topic nodes
    std::string mLabel;   // human-readable; topic title or phase scope
};

// Build the full list of edges + nodes across every bundle. Always
// emits a topic node for each loaded topic even if it has no edges —
// this lets consumers render the full topic roster without having to
// load bundles separately.
void CollectGraph(const std::vector<FTopicBundle> &InBundles,
                  std::vector<FGraphNode> &OutNodes,
                  std::vector<FGraphEdge> &OutEdges)
{
    // First pass: seed one topic node per bundle and one phase node per
    // phase. This ensures every real topic/phase is present regardless
    // of whether any edge touches it.
    for (const FTopicBundle &B : InBundles)
    {
        FGraphNode TopicNode;
        TopicNode.mId = TopicNodeID(B.mTopicKey);
        TopicNode.mKind = "topic";
        TopicNode.mTopic = B.mTopicKey;
        TopicNode.mLabel = B.mMetadata.mTitle;
        OutNodes.push_back(std::move(TopicNode));
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            FGraphNode PhaseNode;
            PhaseNode.mId = PhaseNodeID(B.mTopicKey, static_cast<int>(PI));
            PhaseNode.mKind = "phase";
            PhaseNode.mTopic = B.mTopicKey;
            PhaseNode.mPhaseIndex = static_cast<int>(PI);
            PhaseNode.mLabel = B.mPhases[PI].mScope;
            OutNodes.push_back(std::move(PhaseNode));
        }
    }

    // Second pass: walk each bundle's topic-level and per-phase deps.
    for (const FTopicBundle &B : InBundles)
    {
        // Topic-level deps originate from topic:<B.mTopicKey>.
        const std::string From = TopicNodeID(B.mTopicKey);
        for (const FBundleReference &R : B.mMetadata.mDependencies)
        {
            FGraphEdge E;
            E.mFrom = From;
            E.mKind = ToString(R.mKind);
            E.mNote = R.mNote;
            E.mTargetPath = R.mPath;
            if (R.mKind == EDependencyKind::Bundle && !R.mTopic.empty())
                E.mTo = TopicNodeID(R.mTopic);
            else if (R.mKind == EDependencyKind::Phase && !R.mTopic.empty())
                E.mTo = PhaseNodeID(R.mTopic, R.mPhase);
            else
            {
                // governance / external: target is the path, not a topic
                // node. Encode as "path:<path>" so consumers can still
                // treat it as a string node even though there's no
                // corresponding FGraphNode.
                E.mTo = "path:" + R.mPath;
            }
            OutEdges.push_back(std::move(E));
        }
        // Phase-level deps originate from phase:<B.mTopicKey>/<PhaseIndex>.
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const std::string PhaseFrom =
                PhaseNodeID(B.mTopicKey, static_cast<int>(PI));
            const FPhaseRecord &Phase = B.mPhases[PI];
            for (const FBundleReference &R : Phase.mDesign.mDependencies)
            {
                FGraphEdge E;
                E.mFrom = PhaseFrom;
                E.mKind = ToString(R.mKind);
                E.mNote = R.mNote;
                E.mTargetPath = R.mPath;
                if (R.mKind == EDependencyKind::Bundle && !R.mTopic.empty())
                    E.mTo = TopicNodeID(R.mTopic);
                else if (R.mKind == EDependencyKind::Phase &&
                         !R.mTopic.empty())
                    E.mTo = PhaseNodeID(R.mTopic, R.mPhase);
                else
                    E.mTo = "path:" + R.mPath;
                OutEdges.push_back(std::move(E));
            }
        }
    }
}

// Build adjacency map for a BFS walk. Walks BOTH directions (outgoing
// and incoming) because "neighborhood" in governance terms means both
// "what does <T> depend on" and "what depends on <T>".
void BuildAdjacency(const std::vector<FGraphEdge> &InEdges,
                    std::map<std::string, std::set<std::string>> &OutAdj)
{
    for (const FGraphEdge &E : InEdges)
    {
        OutAdj[E.mFrom].insert(E.mTo);
        OutAdj[E.mTo].insert(E.mFrom);
    }
}

// BFS walk from the focus topic (all of its phase nodes + the topic
// node itself). Depth counts edge hops; -1 = unlimited.
std::set<std::string>
WalkNeighborhood(const std::vector<FGraphNode> &InNodes,
                 const std::vector<FGraphEdge> &InEdges,
                 const std::string &InFocusTopic, int InDepth)
{
    std::map<std::string, std::set<std::string>> Adj;
    BuildAdjacency(InEdges, Adj);

    std::set<std::string> Visited;
    std::deque<std::pair<std::string, int>> Queue;
    // Seed with the topic node and every phase node of the focus topic.
    for (const FGraphNode &N : InNodes)
    {
        if (N.mTopic == InFocusTopic)
        {
            Queue.push_back({N.mId, 0});
            Visited.insert(N.mId);
        }
    }
    while (!Queue.empty())
    {
        const auto [Id, Distance] = Queue.front();
        Queue.pop_front();
        if (InDepth >= 0 && Distance >= InDepth)
            continue;
        const auto AdjIt = Adj.find(Id);
        if (AdjIt == Adj.end())
            continue;
        for (const std::string &Neighbor : AdjIt->second)
        {
            if (Visited.count(Neighbor) > 0)
                continue;
            Visited.insert(Neighbor);
            Queue.push_back({Neighbor, Distance + 1});
        }
    }
    return Visited;
}

} // namespace

int RunGraphCommand(const std::vector<std::string> &InArgs,
                    const std::string &InRepoRoot)
{
    // Route --help through the shared help registry rather than the
    // option parser so agents always see a clean 0-exit block.
    if (ContainsHelpFlag(InArgs))
    {
        PrintCommandUsage(std::cout, "graph");
        return 0;
    }
    const FGraphOptions Options = ParseGraphOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    std::vector<std::string> Warnings;
    std::vector<FTopicBundle> Bundles = LoadAllBundles(RepoRoot, Warnings);

    std::vector<FGraphNode> AllNodes;
    std::vector<FGraphEdge> AllEdges;
    CollectGraph(Bundles, AllNodes, AllEdges);

    // Filter to neighborhood if --topic is set.
    std::set<std::string> VisibleNodeIds;
    if (!Options.mTopic.empty())
    {
        // Verify the focus topic exists; report a blank neighborhood
        // with an explicit warning if not. Keeps the schema consistent
        // across hit/miss cases.
        bool Found = false;
        for (const FGraphNode &N : AllNodes)
        {
            if (N.mTopic == Options.mTopic)
            {
                Found = true;
                break;
            }
        }
        if (!Found)
        {
            Warnings.push_back("graph: topic '" + Options.mTopic +
                               "' not found in any loaded bundle");
        }
        VisibleNodeIds =
            WalkNeighborhood(AllNodes, AllEdges, Options.mTopic, Options.mDepth);
    }

    // Select visible nodes + edges. No filter: emit everything.
    std::vector<FGraphNode> Nodes;
    std::vector<FGraphEdge> Edges;
    if (Options.mTopic.empty())
    {
        Nodes = AllNodes;
        Edges = AllEdges;
    }
    else
    {
        for (const FGraphNode &N : AllNodes)
        {
            if (VisibleNodeIds.count(N.mId) > 0)
                Nodes.push_back(N);
        }
        for (const FGraphEdge &E : AllEdges)
        {
            if (VisibleNodeIds.count(E.mFrom) > 0 ||
                VisibleNodeIds.count(E.mTo) > 0)
                Edges.push_back(E);
        }
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kGraphSchema, UTC, RepoRoot.string());
    if (!Options.mTopic.empty())
    {
        EmitJsonField("focus_topic", Options.mTopic);
        EmitJsonFieldInt("depth", Options.mDepth);
    }
    EmitJsonFieldSizeT("node_count", Nodes.size());
    EmitJsonFieldSizeT("edge_count", Edges.size());

    // nodes
    std::cout << "\"nodes\":[";
    for (size_t I = 0; I < Nodes.size(); ++I)
    {
        if (I > 0)
            std::cout << ",";
        const FGraphNode &N = Nodes[I];
        std::cout << "{";
        EmitJsonField("id", N.mId);
        EmitJsonField("kind", N.mKind);
        EmitJsonField("topic", N.mTopic);
        if (N.mPhaseIndex >= 0)
            std::cout << "\"phase_index\":" << N.mPhaseIndex << ",";
        else
            std::cout << "\"phase_index\":null,";
        EmitJsonField("label", N.mLabel, false);
        std::cout << "}";
    }
    std::cout << "],";

    // edges
    std::cout << "\"edges\":[";
    for (size_t I = 0; I < Edges.size(); ++I)
    {
        if (I > 0)
            std::cout << ",";
        const FGraphEdge &E = Edges[I];
        std::cout << "{";
        EmitJsonField("from", E.mFrom);
        EmitJsonField("to", E.mTo);
        EmitJsonField("kind", E.mKind);
        EmitJsonField("path", E.mTargetPath);
        EmitJsonField("note", E.mNote, false);
        std::cout << "}";
    }
    std::cout << "],";

    PrintJsonClose(Warnings);
    return 0;
}

} // namespace UniPlan
