#include "transition_analysis.h"


void TransitionAnalyzer::pivotStates(NFA::VertexDescriptor source, const NFA& graph) {
  Transitions.clear();
  Transitions.resize(256);
  ByteSet permitted;

  for (const NFA::VertexDescriptor ov : graph.outVertices(source)) {
    graph[ov].Trans->getBytes(permitted);
    for (uint32_t i = 0; i < 256; ++i) {
      if (permitted[i] && std::find(Transitions[i].begin(), Transitions[i].end(), ov) == Transitions[i].end()) {
        Transitions[i].push_back(ov);
      }
    }
  }
}

uint32_t TransitionAnalyzer::maxOutbound(void) const {
  return std::max_element(Transitions.begin(), Transitions.end(),
    [](const std::vector<NFA::VertexDescriptor>& l,
       const std::vector<NFA::VertexDescriptor>& r) {
      return l.size() < r.size();
    }
  )->size();
}
