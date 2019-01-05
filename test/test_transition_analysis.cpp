/*
  liblightgrep: not the worst forensics regexp engine
  Copyright (C) 2013, Lightbox Technologies, Inc

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <scope/test.h>

#include "test_helper.h"
#include "transition_analysis.h"


SCOPE_TEST(testPivotTransitions) {
  NFA fsm(5);
  edge(0, 1, fsm, fsm.TransFac->getByte('a'));
  edge(0, 2, fsm, fsm.TransFac->getByte('a'));
  edge(0, 3, fsm, fsm.TransFac->getByte('z'));
  edge(0, 4, fsm, fsm.TransFac->getByte('z'));

  fsm[1].IsMatch = true;
  fsm[1].Label = 0;

  fsm[2].IsMatch = true;
  fsm[2].Label = 1;

  TransitionAnalyzer analyzer;
  analyzer.pivotStates(0, fsm);
  SCOPE_ASSERT_EQUAL(256u, analyzer.Transitions.size());
  for (uint32_t i = 0; i < 256; ++i) {
  	auto& tbl(analyzer.Transitions[i]);
    if (i == 'a') {
      SCOPE_ASSERT_EQUAL(2u, tbl.size());
      SCOPE_ASSERT(std::find(tbl.begin(), tbl.end(), 1) != tbl.end());
      SCOPE_ASSERT(std::find(tbl.begin(), tbl.end(), 2) != tbl.end());
    }
    else if (i == 'z') {
      SCOPE_ASSERT_EQUAL(2u, tbl.size());
      SCOPE_ASSERT(std::find(tbl.begin(), tbl.end(), 3) != tbl.end());
      SCOPE_ASSERT(std::find(tbl.begin(), tbl.end(), 4) != tbl.end());
    }
    else {
      SCOPE_ASSERT(tbl.empty());
    }
  }
}

SCOPE_TEST(testMaxOutbound) {
  NFA fsm(5);
  edge(0, 1, fsm, fsm.TransFac->getByte('a'));
  edge(0, 2, fsm, fsm.TransFac->getByte('a'));
  edge(0, 3, fsm, fsm.TransFac->getByte('b'));
  edge(0, 4, fsm, fsm.TransFac->getByte('c'));
  TransitionAnalyzer analyzer;
  analyzer.pivotStates(0, fsm);
  SCOPE_ASSERT_EQUAL(2u, analyzer.maxOutbound());
}
