#include <scope/test.h>
#include <boost/smart_ptr.hpp>

#include "states.h"

#include <iostream>

template<class TransitionType>
void testClone(const TransitionType& toCopy, byte* text) {
  boost::shared_array<byte> buf(new byte[toCopy.objSize()]);
  TransitionType* dupe(toCopy.clone(buf.get()));
  SCOPE_ASSERT_EQUAL(buf.get(), (byte*)dupe);
  SCOPE_ASSERT_EQUAL(text+1, dupe->allowed(text, text+1));
}

SCOPE_TEST(litAccept) {
  const LitState lit('a');
  byte ch[2] = "a";
  SCOPE_ASSERT_EQUAL(ch+1, lit.allowed(ch, ch+1));
  ch[0] = 'b';
  SCOPE_ASSERT_EQUAL(ch, lit.allowed(ch, ch+1));
  std::bitset<256> bits(0);
  lit.getBits(bits);
  SCOPE_ASSERT_EQUAL(1u, bits.count());
  SCOPE_ASSERT(bits.test('a'));
  SCOPE_ASSERT(bits.any());
  SCOPE_ASSERT(!bits.test('c'));

  SCOPE_ASSERT_EQUAL(sizeof(void*) + 1, lit.objSize());
  ch[0] = 'a';
  testClone(lit, ch);
}

SCOPE_TEST(eitherAccept) {
  const EitherState e('A', 'a');
  byte ch = 'a';
  SCOPE_ASSERT_EQUAL(&ch+1, e.allowed(&ch, &ch+1));
  ch = 'b';
  SCOPE_ASSERT_EQUAL(&ch, e.allowed(&ch, &ch+1));
  ch = 'A';
  SCOPE_ASSERT_EQUAL(&ch+1, e.allowed(&ch, &ch+1));
  
  std::bitset<256> bits(0);
  e.getBits(bits);
  SCOPE_ASSERT_EQUAL(2u, bits.count());
  SCOPE_ASSERT(bits.test('a'));
  SCOPE_ASSERT(bits.test('A'));
  SCOPE_ASSERT(!bits.test('#'));

  SCOPE_ASSERT_EQUAL(sizeof(void*) + 2, e.objSize());
  testClone(e, &ch);
}

SCOPE_TEST(rangeAccept) {
  const RangeState r('0', '9');
  byte ch;
  std::bitset<256> bits(0);
  r.getBits(bits);
  SCOPE_ASSERT_EQUAL(10u, bits.count());
  for (unsigned int i = 0; i < 256; ++i) {
    ch = i;
    if ('0' <= ch && ch <= '9') {
      SCOPE_ASSERT_EQUAL(&ch+1, r.allowed(&ch, &ch+1));
      SCOPE_ASSERT(bits.test(ch));
    }
    else {
      SCOPE_ASSERT_EQUAL(&ch, r.allowed(&ch, &ch+1));
      SCOPE_ASSERT(!bits.test(ch));
    }
  }
  SCOPE_ASSERT_EQUAL(sizeof(void*) + 2, r.objSize());
  ch = '1';
  testClone(r, &ch);
}
