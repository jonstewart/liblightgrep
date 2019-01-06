#pragma once

#include <vector>

#include "automata.h"

typedef std::vector<std::vector<NFA::VertexDescriptor>> TransitionTbl;

class TransitionAnalyzer {
public:
	TransitionAnalyzer();

	void pivotStates(NFA::VertexDescriptor source, const NFA& graph);

	uint32_t maxOutbound(void) const { return MaxOutbound; }
	uint32_t first(void) const { return First; }
	uint32_t last(void) const { return Last; }

	TransitionTbl Transitions;

private:
	size_t	 MaxOutbound;
	uint32_t First,
			 Last;
};
