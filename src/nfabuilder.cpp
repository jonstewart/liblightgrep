#include "nfabuilder.h"

#include "states.h"
#include "concrete_encodings.h"
#include "utility.h"

#include <iostream>
#include <utility>

std::ostream& operator<<(std::ostream& out, const InListT& list) {
  out << '[';
  for (InListT::const_iterator it(list.begin()); it != list.end(); ++it) {
    if (it != list.begin()) {
      out << ", ";
    }
    out << *it;
  }
  out << ']';
  return out;
}

std::ostream& operator<<(std::ostream& out, const OutListT& list) {
  out << '[';
  for (OutListT::const_iterator i(list.begin()); i != list.end(); ++i) {
    if (i != list.begin()) {
      out << ", ";
    }

    out << '(' << i->first << ',' << i->second << ')';
  }
  out << ']';
  return out;
}

std::ostream& operator<<(std::ostream& out, const Fragment& f) {
  out << "in " << f.InList << ", out " << f.OutList
      << ", skip " << f.Skippable;
  return out;
}

NFABuilder::NFABuilder():
  CaseSensitive(true),
  CurLabel(0),
  ReserveSize(0)
{
  for (unsigned int i = 0; i < 256; ++i) {
    LitFlyweights.push_back(TransitionPtr(new LitState(i)));
  }
  setEncoding(boost::shared_ptr<Encoding>(new Ascii));
  reset();
}

void NFABuilder::reset() {
  IsGood = false;
  if (Fsm) {
    Fsm->clear();
    Fsm->addVertex();
  }
  else {
    Fsm.reset(new Graph(1));
//    Fsm.reset(new Graph(1, ReserveSize));
  }
  while (!Stack.empty()) {
    Stack.pop();
  }
  TempFrag.initFull(0, Node());
  Stack.push(TempFrag);
}

void NFABuilder::setEncoding(const boost::shared_ptr<Encoding>& e) {
  Enc = e;
  TempBuf.reset(new byte[Enc->maxByteLength()]);
}

void NFABuilder::setCaseSensitive(bool caseSensitive) {
  CaseSensitive = caseSensitive;
}

void NFABuilder::setSizeHint(uint64 reserveSize) {
  ReserveSize = reserveSize;
}

void NFABuilder::setLiteralTransition(TransitionPtr& state, byte val) {
  if (CaseSensitive || !std::isalpha(val)) {
// FIXME: Labeled vertices can't be shared. We don't know which will be
// labeled (permanently) until after walking back labels. If the memory
// we were saving this way was really important, we need to figure out
// something else to do here.
//    state = LitFlyweights[val];
    state = TransitionPtr(new LitState(val));
  }
  else {
    state.reset(new EitherState(std::toupper(val), std::tolower(val)));
  }
}

void NFABuilder::patch_mid(OutListT& src, const InListT& dst, uint32 dstskip) {
  // Make an edge from each vertex in src to each vertex in dst. Edges
  // to vertices in dst before dstskip go before the insertion point in
  // src, edges to vertices in dst after dstskip go after the insertion
  // point in src.
  const InListT::const_iterator skip_stop(dst.begin() + dstskip);

  for (OutListT::iterator oi(src.begin()); oi != src.end(); ++oi) {
    uint32 pos = oi->second;

    InListT::const_iterator ii(dst.begin());

    // make edges before dstskip, inserting before src insertion point
    for ( ; ii != dst.end() && ii < skip_stop; ++ii) {
      Fsm->addEdgeAtND(oi->first, *ii, pos++);
    }

    // save the new insertion point for dst
    const uint32 spos = pos;

    // make edges after dstskip, inserting after src insertion point
    for ( ; ii != dst.end(); ++ii) {
      Fsm->addEdgeAtND(oi->first, *ii, pos++);
    }

    // set the new insertion point for dst
    oi->second = spos;
  }
}

void NFABuilder::patch_pre(OutListT& src, const InListT& dst) {
  // Put all new edges before dst's insertion point.
  patch_mid(src, dst, dst.size());
}

void NFABuilder::patch_post(OutListT& src, const InListT& dst) {
  // Put all new edges after dst's insertion point.
  patch_mid(src, dst, 0);
}

void NFABuilder::literal(const Node& n) {
  uint32 len = Enc->write(n.Val, TempBuf.get());
  if (0 == len) {
    // FXIME: should we really be checking this if it's supposed to be
    // an impossible condition?
    throw std::logic_error("bad things");
  }
  else {
    Graph& g(*Fsm);
    Graph::vertex first, prev, last;
    first = prev = last = g.addVertex();
    setLiteralTransition(g[first], TempBuf[0]);
    for (uint32 i = 1; i < len; ++i) {
      last = g.addVertex();
      addNewEdge(prev, last, g);
      setLiteralTransition(g[last], TempBuf[i]);
      prev = last;
    }
    TempFrag.reset(n);
    TempFrag.InList.push_back(first);
    TempFrag.OutList.push_back(std::make_pair(last, 0));
    Stack.push(TempFrag);
  }
}

void NFABuilder::dot(const Node& n) {
  Graph::vertex v = Fsm->addVertex();
  (*Fsm)[v].reset(new RangeState(0, 255));
  TempFrag.initFull(v, n);
  Stack.push(TempFrag);
}

void NFABuilder::charClass(const Node& n, const std::string& lbl) {
  Graph::vertex v = Fsm->addVertex();
  uint32 num = 0;
  byte first = 0, last = 0;
  for (uint32 i = 0; i < 256; ++i) {
    if (n.Bits.test(i)) {
      if (!num) {
        first = i;
      }
      if (++num == n.Bits.count()) {
        last = i;
        break;
      }
    }
    else {
      num = 0;
    }
  }
  if (num == n.Bits.count()) {
    (*Fsm)[v].reset(new RangeState(first, last));
  }
  else {
    (*Fsm)[v].reset(new CharClassState(n.Bits, lbl));
  }
  TempFrag.initFull(v, n);
  Stack.push(TempFrag);
}

void NFABuilder::question(const Node&) {
  Fragment& optional = Stack.top();
  if (optional.Skippable > optional.InList.size()) {
    optional.Skippable = optional.InList.size();
  }
}

void NFABuilder::question_ng(const Node&) {
  Fragment& optional = Stack.top();
  optional.Skippable = 0;
}

void NFABuilder::plus(const Node& n) {
  Fragment& repeat = Stack.top();
  repeat.N = n;

  // make back edges
  patch_pre(repeat.OutList, repeat.InList);
}

void NFABuilder::plus_ng(const Node& n) {
  Fragment& repeat = Stack.top();
  repeat.N = n;

  // make back edges
  patch_post(repeat.OutList, repeat.InList);
}

void NFABuilder::star(const Node& n) {
  plus(n);
  question(n);
}

void NFABuilder::star_ng(const Node& n) {
  plus_ng(n);
  question_ng(n);
}

void NFABuilder::repetition(const Node& n) {
  if (n.Min == 0) {
    if (n.Max == 1) {
      question(n);
      return;
    }
    else if (n.Max == UNBOUNDED) {
      star(n);
      return;
    }
  }
  else if (n.Min == 1 && n.Max == UNBOUNDED) {
    plus(n);
    return;
  }

  // all other cases are already reduced by traverse
}

void NFABuilder::repetition_ng(const Node& n) {
  if (n.Min == 0) {
    if (n.Max == 1) {
      question_ng(n);
      return;
    }
    else if (n.Max == UNBOUNDED) {
      star_ng(n);
      return;
    }
  }
  else if (n.Min == 1 && n.Max == UNBOUNDED) {
    plus_ng(n);
    return;
  }

  // all other cases are already reduced by traverse
}

void NFABuilder::alternate(const Node& n) {
  Fragment second;
  second.assign(Stack.top());
  Stack.pop();
  
  Fragment& first(Stack.top());

  if (first.Skippable != NOSKIP) {
    // leave first.Skippable unchanged
  }
  else if (second.Skippable != NOSKIP) {
    first.Skippable = first.InList.size() + second.Skippable;
  }
  else {
    first.Skippable = NOSKIP;
  }

  first.InList.insert(first.InList.end(),
                      second.InList.begin(), second.InList.end());
  first.OutList.insert(first.OutList.end(),
                       second.OutList.begin(), second.OutList.end());

  first.N = n;
}

void NFABuilder::concatenate(const Node& n) {
  TempFrag.assign(Stack.top());
  Stack.pop();
  Fragment& first(Stack.top());

  // patch left out to right in
  patch_mid(first.OutList, TempFrag.InList, TempFrag.Skippable);

  // build new in list
  if (first.Skippable != NOSKIP) {
    first.InList.insert(first.InList.begin() + first.Skippable,
                        TempFrag.InList.begin(), TempFrag.InList.end());
  }

  // build new out list
  if (TempFrag.Skippable != NOSKIP) {
    first.OutList.insert(first.OutList.end(),
                         TempFrag.OutList.begin(), TempFrag.OutList.end());
  }
  else {
    first.OutList.swap(TempFrag.OutList);
  }

  // set new skippable
  first.Skippable = first.Skippable == NOSKIP || TempFrag.Skippable == NOSKIP
    ? NOSKIP : first.Skippable + TempFrag.Skippable;

  first.N = n;
}

void NFABuilder::finish(const Node& n) {
  if (2 == Stack.size()) {
    concatenate(n);
    Fragment& start(Stack.top());

    for (OutListT::const_iterator i(start.OutList.begin()); i != start.OutList.end(); ++i) {
      // std::cout << "marking " << *it << " as a match" << std::endl;
      Graph::vertex v = i->first;
      if (0 == v) { // State 0 is not allowed to be a match state; i.e. 0-length REs are not allowed
        reset();
        return;
      }
      else {
        TransitionPtr final((*Fsm)[v]->clone()); // this is to make sure the transition isn't shared amongst other states
        final->Label = CurLabel;
        final->IsMatch = true;
        (*Fsm)[v] = final;
      }
    }
    // std::cout << "final is " << final << std::endl;
    IsGood = true;
    Stack.pop();
  }
  else {
    reset();
    return;
    // THROW_RUNTIME_ERROR_WITH_OUTPUT("Final parse stack size should be 2, was " << Stack.size());
  }
}

void NFABuilder::traverse(const Node* root) {
  // do a postorder depth-first traversal of the parse tree

  std::stack<const Node*> child_stack;
  std::stack<const Node*> parent_stack;

  // FIXME: preallocate the child_stack: max size should be number of
  // nodes in the parse tree + extra nodes for unrolled repetitions

  child_stack.push(root);

  // Dummy nodes used for expanding counted repetitions. It's safe to
  // pass nodes without valid children to callback() so long as the
  // callbacks for these types don't use anything but Node::Type.
  Node concat(Node::CONCATENATION, (Node*) 0, (Node*) 0);
  Node ques(Node::REPETITION, 0, 0, 1);
  Node ques_ng(Node::REPETITION_NG, 0, 0, 1);
  Node plus(Node::REPETITION, 0, 1, UNBOUNDED);
  Node plus_ng(Node::REPETITION_NG, 0, 1, UNBOUNDED);

  const Node* n;
  while (!child_stack.empty()) {
    n = child_stack.top();
    child_stack.pop();
    parent_stack.push(n);

    if (n->Left) {
      // this node has a left child
      if ((n->Type == Node::REPETITION || n->Type == Node::REPETITION_NG) &&
          !((n->Min == 0 && (n->Max == 1 || n->Max == UNBOUNDED)) ||
            (n->Min == 1 && n->Max == UNBOUNDED)))
      {
        // This is a repetition, but not one of the special ones.

        // NB: We expect that all empty repetitions ({0,0} and {0,0}?)
        // will have been excised from the parse tree by now.

        if (n->Min == 1 && n->Max == 1) {
          // skip the repetition node
          child_stack.push(n->Left);
        }
        else {
          //
          // T{n} = T...T
          //          ^
          //        n times
          //
          // T{n,} = T...TT*
          //           ^
          //         n times
          //
          // T{n,m} = T...TT?...T? = T...T(T(T...)?)?
          //            ^     ^
          //       n times   m-n times
          //
          // Note that the latter equivalence for T{n,m} produces
          // a graph with outdegree 2, while the former produces
          // one with outdegree m-n.
          //

          if (n->Min == n->Max) {
            // push n-1 concatenations
            for (uint32 i = n->Min - 1; i > 0; --i) {
              parent_stack.push(&concat);
            }

            // push the subpattern n times
            for (uint32 i = n->Min; i > 0; --i) {
              parent_stack.push(n->Left);
            }
          }
          else if (n->Max == UNBOUNDED) {
            // push n-1 concatenations
            for (uint32 i = n->Min - 1; i > 0; --i) {
              parent_stack.push(&concat);
            }

            // push the last mandatory part and the optional part
            parent_stack.push(n->Type == Node::REPETITION ? &plus : &plus_ng);
            parent_stack.push(n->Left);

            // push the first n-1 mandataory parts
            for (uint32 i = n->Min - 1; i > 0; --i) {
              parent_stack.push(n->Left);
            }
          }
          else {
            // push n concatenations
            for (uint32 i = n->Min; i > 0; --i) {
              parent_stack.push(&concat);
            }

            // push m-n-1 optional concatenations
            for (uint32 i = n->Max - n->Min - 1; i > 0; --i) {
              parent_stack.push(n->Type == Node::REPETITION ? &ques : &ques_ng);
              parent_stack.push(&concat);
            }

            // push the last ?
            parent_stack.push(n->Type == Node::REPETITION ? &ques : &ques_ng);

            // push m subpatterns
            for (uint32 i = n->Max; i > 0; --i) {
              parent_stack.push(n->Left);
            }
          }
        }
      }
      else {
        // this is not a repetition, or is one of ? * + ?? *? +?
        child_stack.push(n->Left);
      }
    }
  
    if (n->Right) {
      child_stack.push(n->Right);
    }
  }

  while (!parent_stack.empty()) {
    n = parent_stack.top();
    parent_stack.pop();

    callback("", *n);
  }
}

bool NFABuilder::build(const ParseTree& tree) {
  traverse(tree.Root);
  return IsGood;
}

void NFABuilder::callback(const std::string& type, const Node& n) {
//  std::cerr << n << std::endl;
  switch (n.Type) {
    case Node::REGEXP:
      finish(n);
      break;
    case Node::ALTERNATION:
      alternate(n);
      break;
    case Node::CONCATENATION:
      concatenate(n);
      break;
    case Node::REPETITION:
      repetition(n);
      break;
    case Node::REPETITION_NG:
      repetition_ng(n);
      break;
    case Node::DOT:
      dot(n);
      break;
    case Node::CHAR_CLASS:
      charClass(n, type);
      break;
    case Node::LITERAL:
      literal(n);
      break;
    default:
      break;
  }

/*
  std::cerr << "Stack size is " << Stack.size() << std::endl;
  if (Stack.size() > 0) {
     std::cerr << "top is " << Stack.top() << std::endl;
  }
*/
}