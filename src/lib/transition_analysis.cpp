#include "transition_analysis.h"

TransitionAnalyzer::TransitionAnalyzer():
    Transitions(256),
    First(0),
    Last(0),
    NumRanges(0)
{}

void TransitionAnalyzer::pivotStates(NFA::VertexDescriptor source, const NFA& graph) {
  Transitions.clear();
  Transitions.resize(256);
  First = 256;
  Last  = 0;
  MaxOutbound = 0;
  ByteSet permitted;

  for (const NFA::VertexDescriptor ov : graph.outVertices(source)) {
    graph[ov].Trans->getBytes(permitted);
    for (uint32_t i = 0; i < 256; ++i) {
      if (permitted[i]) {
        First = std::min(First, i);
        Last = std::max(Last, i);
        auto& tbl(Transitions[i]);
        if (std::find(tbl.begin(), tbl.end(), ov) == tbl.end()) {
          tbl.push_back(ov);
          MaxOutbound = std::max(MaxOutbound, tbl.size());
        }
        if (i > First && tbl != Transitions[i - 1]) {
          ++NumRanges;
        }
      }
    }
  }
  if (First <= Last) {
    // loop above doesn't count the _first_ range
    ++NumRanges;
  }
}
