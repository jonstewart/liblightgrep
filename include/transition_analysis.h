#pragma once

#include <vector>

#include "automata.h"

typedef std::vector<std::vector<NFA::VertexDescriptor>> TransitionTbl;

class TransitionAnalyzer {
public:
	void pivotStates(NFA::VertexDescriptor source, const NFA& graph);

	uint32_t maxOutbound(void) const;

	TransitionTbl Transitions;
};
