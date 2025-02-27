// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"

#include <stddef.h>

#include <sstream>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Used to test depth subtyping.
class ConDecLoggerParent {
 public:
  virtual ~ConDecLoggerParent() {}

  virtual void SetPtr(int* ptr) = 0;

  virtual int SomeMeth(int x) const = 0;
};

class ConDecLogger : public ConDecLoggerParent {
 public:
  ConDecLogger() : ptr_(NULL) { }
  explicit ConDecLogger(int* ptr) { SetPtr(ptr); }
  ~ConDecLogger() override { --*ptr_; }

  void SetPtr(int* ptr) override {
    ptr_ = ptr;
    ++*ptr_;
  }

  int SomeMeth(int x) const override { return x; }

 private:
  int* ptr_;

  DISALLOW_COPY_AND_ASSIGN(ConDecLogger);
};

struct CountingDeleter {
  explicit CountingDeleter(int* count) : count_(count) {}
  inline void operator()(double* ptr) const {
    (*count_)++;
  }
  int* count_;
};

// Used to test assignment of convertible deleters.
struct CountingDeleterChild : public CountingDeleter {
  explicit CountingDeleterChild(int* count) : CountingDeleter(count) {}
};

class OverloadedNewAndDelete {
 public:
  void* operator new(size_t size) {
    g_new_count++;
    return malloc(size);
  }

  void operator delete(void* ptr) {
    g_delete_count++;
    free(ptr);
  }

  static void ResetCounters() {
    g_new_count = 0;
    g_delete_count = 0;
  }

  static int new_count() { return g_new_count; }
  static int delete_count() { return g_delete_count; }

 private:
  static int g_new_count;
  static int g_delete_count;
};

int OverloadedNewAndDelete::g_new_count = 0;
int OverloadedNewAndDelete::g_delete_count = 0;

scoped_ptr<ConDecLogger> PassThru(scoped_ptr<ConDecLogger> logger) {
  return logger;
}

void GrabAndDrop(scoped_ptr<ConDecLogger> logger) {
}

// Do not delete this function!  It's existence is to test that you can
// return a temporarily constructed version of the scoper.
scoped_ptr<ConDecLogger> TestReturnOfType(int* constructed) {
  return scoped_ptr<ConDecLogger>(new ConDecLogger(constructed));
}

}  // namespace

TEST(ScopedPtrTest, ScopedPtr) {
  int constructed = 0;

  // Ensure size of scoped_ptr<> doesn't increase unexpectedly.
  static_assert(sizeof(int*) >= sizeof(scoped_ptr<int>),
                "scoped_ptr shouldn't be larger than the raw pointer");

  {
    scoped_ptr<ConDecLogger> scoper(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    EXPECT_EQ(10, scoper->SomeMeth(10));
    EXPECT_EQ(10, scoper.get()->SomeMeth(10));
    EXPECT_EQ(10, (*scoper).SomeMeth(10));
  }
  EXPECT_EQ(0, constructed);

  // Test reset() and release()
  {
    scoped_ptr<ConDecLogger> scoper(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    scoper.reset(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    scoper.reset();
    EXPECT_EQ(0, constructed);
    EXPECT_FALSE(scoper.get());

    scoper.reset(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    ConDecLogger* take = scoper.release();
    EXPECT_EQ(1, constructed);
    EXPECT_FALSE(scoper.get());
    delete take;
    EXPECT_EQ(0, constructed);

    scoper.reset(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());
  }
  EXPECT_EQ(0, constructed);

  // Test swap().
  {
    scoped_ptr<ConDecLogger> scoper1;
    scoped_ptr<ConDecLogger> scoper2;
    EXPECT_TRUE(scoper1.get() == scoper2.get());
    EXPECT_FALSE(scoper1.get() != scoper2.get());

    ConDecLogger* logger = new ConDecLogger(&constructed);
    scoper1.reset(logger);
    EXPECT_EQ(logger, scoper1.get());
    EXPECT_FALSE(scoper2.get());
    EXPECT_FALSE(scoper1.get() == scoper2.get());
    EXPECT_TRUE(scoper1.get() != scoper2.get());

    scoper2.swap(scoper1);
    EXPECT_EQ(logger, scoper2.get());
    EXPECT_FALSE(scoper1.get());
    EXPECT_FALSE(scoper1.get() == scoper2.get());
    EXPECT_TRUE(scoper1.get() != scoper2.get());
  }
  EXPECT_EQ(0, constructed);
}

TEST(ScopedPtrTest, ScopedPtrDepthSubtyping) {
  int constructed = 0;

  // Test construction from a scoped_ptr to a derived class.
  {
    scoped_ptr<ConDecLogger> scoper(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    scoped_ptr<ConDecLoggerParent> scoper_parent(std::move(scoper));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper_parent.get());
    EXPECT_FALSE(scoper.get());

    EXPECT_EQ(10, scoper_parent->SomeMeth(10));
    EXPECT_EQ(10, scoper_parent.get()->SomeMeth(10));
    EXPECT_EQ(10, (*scoper_parent).SomeMeth(10));
  }
  EXPECT_EQ(0, constructed);

  // Test assignment from a scoped_ptr to a derived class.
  {
    scoped_ptr<ConDecLogger> scoper(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    scoped_ptr<ConDecLoggerParent> scoper_parent;
    scoper_parent = std::move(scoper);
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper_parent.get());
    EXPECT_FALSE(scoper.get());
  }
  EXPECT_EQ(0, constructed);

  // Test construction of a scoped_ptr with an additional const annotation.
  {
    scoped_ptr<ConDecLogger> scoper(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    scoped_ptr<const ConDecLogger> scoper_const(std::move(scoper));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper_const.get());
    EXPECT_FALSE(scoper.get());

    EXPECT_EQ(10, scoper_const->SomeMeth(10));
    EXPECT_EQ(10, scoper_const.get()->SomeMeth(10));
    EXPECT_EQ(10, (*scoper_const).SomeMeth(10));
  }
  EXPECT_EQ(0, constructed);

  // Test assignment to a scoped_ptr with an additional const annotation.
  {
    scoped_ptr<ConDecLogger> scoper(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    scoped_ptr<const ConDecLogger> scoper_const;
    scoper_const = std::move(scoper);
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper_const.get());
    EXPECT_FALSE(scoper.get());
  }
  EXPECT_EQ(0, constructed);

  // Test assignment to a scoped_ptr deleter of parent type.
  {
    // Custom deleters never touch these value.
    double dummy_value, dummy_value2;
    int deletes = 0;
    int alternate_deletes = 0;
    scoped_ptr<double, CountingDeleter> scoper(&dummy_value,
                                               CountingDeleter(&deletes));
    scoped_ptr<double, CountingDeleterChild> scoper_child(
        &dummy_value2, CountingDeleterChild(&alternate_deletes));

    EXPECT_TRUE(scoper);
    EXPECT_TRUE(scoper_child);
    EXPECT_EQ(0, deletes);
    EXPECT_EQ(0, alternate_deletes);

    // Test this compiles and correctly overwrites the deleter state.
    scoper = std::move(scoper_child);
    EXPECT_TRUE(scoper);
    EXPECT_FALSE(scoper_child);
    EXPECT_EQ(1, deletes);
    EXPECT_EQ(0, alternate_deletes);

    scoper.reset();
    EXPECT_FALSE(scoper);
    EXPECT_FALSE(scoper_child);
    EXPECT_EQ(1, deletes);
    EXPECT_EQ(1, alternate_deletes);

    scoper_child.reset(&dummy_value);
    EXPECT_TRUE(scoper_child);
    EXPECT_EQ(1, deletes);
    EXPECT_EQ(1, alternate_deletes);
    scoped_ptr<double, CountingDeleter> scoper_construct(
        std::move(scoper_child));
    EXPECT_TRUE(scoper_construct);
    EXPECT_FALSE(scoper_child);
    EXPECT_EQ(1, deletes);
    EXPECT_EQ(1, alternate_deletes);

    scoper_construct.reset();
    EXPECT_EQ(1, deletes);
    EXPECT_EQ(2, alternate_deletes);
  }
}

TEST(ScopedPtrTest, ScopedPtrWithArray) {
  static const int kNumLoggers = 12;

  int constructed = 0;

  {
    scoped_ptr<ConDecLogger[]> scoper(new ConDecLogger[kNumLoggers]);
    EXPECT_TRUE(scoper);
    EXPECT_EQ(&scoper[0], scoper.get());
    for (int i = 0; i < kNumLoggers; ++i) {
      scoper[i].SetPtr(&constructed);
    }
    EXPECT_EQ(12, constructed);

    EXPECT_EQ(10, scoper.get()->SomeMeth(10));
    EXPECT_EQ(10, scoper[2].SomeMeth(10));
  }
  EXPECT_EQ(0, constructed);

  // Test reset() and release()
  {
    scoped_ptr<ConDecLogger[]> scoper;
    EXPECT_FALSE(scoper.get());
    EXPECT_FALSE(scoper.release());
    EXPECT_FALSE(scoper.get());
    scoper.reset();
    EXPECT_FALSE(scoper.get());

    scoper.reset(new ConDecLogger[kNumLoggers]);
    for (int i = 0; i < kNumLoggers; ++i) {
      scoper[i].SetPtr(&constructed);
    }
    EXPECT_EQ(12, constructed);
    scoper.reset();
    EXPECT_EQ(0, constructed);

    scoper.reset(new ConDecLogger[kNumLoggers]);
    for (int i = 0; i < kNumLoggers; ++i) {
      scoper[i].SetPtr(&constructed);
    }
    EXPECT_EQ(12, constructed);
    ConDecLogger* ptr = scoper.release();
    EXPECT_EQ(12, constructed);
    delete[] ptr;
    EXPECT_EQ(0, constructed);
  }
  EXPECT_EQ(0, constructed);

  // Test swap() and type-safe Boolean.
  {
    scoped_ptr<ConDecLogger[]> scoper1;
    scoped_ptr<ConDecLogger[]> scoper2;
    EXPECT_TRUE(scoper1.get() == scoper2.get());
    EXPECT_FALSE(scoper1.get() != scoper2.get());

    ConDecLogger* loggers = new ConDecLogger[kNumLoggers];
    for (int i = 0; i < kNumLoggers; ++i) {
      loggers[i].SetPtr(&constructed);
    }
    scoper1.reset(loggers);
    EXPECT_TRUE(scoper1);
    EXPECT_EQ(loggers, scoper1.get());
    EXPECT_FALSE(scoper2);
    EXPECT_FALSE(scoper2.get());
    EXPECT_FALSE(scoper1.get() == scoper2.get());
    EXPECT_TRUE(scoper1.get() != scoper2.get());

    scoper2.swap(scoper1);
    EXPECT_EQ(loggers, scoper2.get());
    EXPECT_FALSE(scoper1.get());
    EXPECT_FALSE(scoper1.get() == scoper2.get());
    EXPECT_TRUE(scoper1.get() != scoper2.get());
  }
  EXPECT_EQ(0, constructed);

  {
    ConDecLogger* loggers = new ConDecLogger[kNumLoggers];
    scoped_ptr<ConDecLogger[]> scoper(loggers);
    EXPECT_TRUE(scoper);
    for (int i = 0; i < kNumLoggers; ++i) {
      scoper[i].SetPtr(&constructed);
    }
    EXPECT_EQ(kNumLoggers, constructed);

    // Test moving with constructor;
    scoped_ptr<ConDecLogger[]> scoper2(std::move(scoper));
    EXPECT_EQ(kNumLoggers, constructed);

    // Test moving with assignment;
    scoped_ptr<ConDecLogger[]> scoper3;
    scoper3 = std::move(scoper2);
    EXPECT_EQ(kNumLoggers, constructed);
    EXPECT_FALSE(scoper);
    EXPECT_FALSE(scoper2);
    EXPECT_TRUE(scoper3);
  }
  EXPECT_EQ(0, constructed);
}

TEST(ScopedPtrTest, MoveBehavior) {
  int constructed = 0;
  {
    ConDecLogger* logger = new ConDecLogger(&constructed);
    scoped_ptr<ConDecLogger> scoper(logger);
    EXPECT_EQ(1, constructed);

    // Test moving with constructor;
    scoped_ptr<ConDecLogger> scoper2(std::move(scoper));
    EXPECT_EQ(1, constructed);

    // Test moving with assignment;
    scoped_ptr<ConDecLogger> scoper3;
    scoper3 = std::move(scoper2);
    EXPECT_EQ(1, constructed);
    EXPECT_FALSE(scoper.get());
    EXPECT_FALSE(scoper2.get());
    EXPECT_TRUE(scoper3.get());
  }

#if !defined(OS_ANDROID) && !defined(OS_LINUX) && !defined(OS_MACOSX)
  // Test uncaught Pass() does not have side effects, because Pass()
  // is implemented by std::move().
  // TODO(danakj): Remove this test case when we remove Pass().
  {
    ConDecLogger* logger = new ConDecLogger(&constructed);
    scoped_ptr<ConDecLogger> scoper(logger);
    EXPECT_EQ(1, constructed);

    // Should auto-destruct logger by end of scope.
    scoped_ptr<ConDecLogger>&& rvalue = scoper.Pass();
    // The Pass() function mimics std::move(), which does not have side-effects.
    EXPECT_TRUE(scoper.get());
    EXPECT_TRUE(rvalue);
  }
  EXPECT_EQ(0, constructed);
#endif

  // Test that passing to function which does nothing does not leak.
  {
    ConDecLogger* logger = new ConDecLogger(&constructed);
    scoped_ptr<ConDecLogger> scoper(logger);
    EXPECT_EQ(1, constructed);

    // Should auto-destruct logger by end of scope.
    GrabAndDrop(std::move(scoper));
    EXPECT_FALSE(scoper.get());
  }
  EXPECT_EQ(0, constructed);
}

TEST(ScopedPtrTest, ReturnTypeBehavior) {
  int constructed = 0;

  // Test that we can return a scoped_ptr.
  {
    ConDecLogger* logger = new ConDecLogger(&constructed);
    scoped_ptr<ConDecLogger> scoper(logger);
    EXPECT_EQ(1, constructed);

    PassThru(std::move(scoper));
    EXPECT_FALSE(scoper.get());
  }
  EXPECT_EQ(0, constructed);

  // Test uncaught return type not leak.
  {
    ConDecLogger* logger = new ConDecLogger(&constructed);
    scoped_ptr<ConDecLogger> scoper(logger);
    EXPECT_EQ(1, constructed);

    // Should auto-destruct logger by end of scope.
    PassThru(std::move(scoper));
    EXPECT_FALSE(scoper.get());
  }
  EXPECT_EQ(0, constructed);

  // Call TestReturnOfType() so the compiler doesn't warn for an unused
  // function.
  {
    TestReturnOfType(&constructed);
  }
  EXPECT_EQ(0, constructed);
}

TEST(ScopedPtrTest, CustomDeleter) {
  double dummy_value;  // Custom deleter never touches this value.
  int deletes = 0;
  int alternate_deletes = 0;

  // Normal delete support.
  {
    deletes = 0;
    scoped_ptr<double, CountingDeleter> scoper(&dummy_value,
                                               CountingDeleter(&deletes));
    EXPECT_EQ(0, deletes);
    EXPECT_TRUE(scoper.get());
  }
  EXPECT_EQ(1, deletes);

  // Test reset() and release().
  deletes = 0;
  {
    scoped_ptr<double, CountingDeleter> scoper(NULL,
                                               CountingDeleter(&deletes));
    EXPECT_FALSE(scoper.get());
    EXPECT_FALSE(scoper.release());
    EXPECT_FALSE(scoper.get());
    scoper.reset();
    EXPECT_FALSE(scoper.get());
    EXPECT_EQ(0, deletes);

    scoper.reset(&dummy_value);
    scoper.reset();
    EXPECT_EQ(1, deletes);

    scoper.reset(&dummy_value);
    EXPECT_EQ(&dummy_value, scoper.release());
  }
  EXPECT_EQ(1, deletes);

  // Test get_deleter().
  deletes = 0;
  alternate_deletes = 0;
  {
    scoped_ptr<double, CountingDeleter> scoper(&dummy_value,
                                               CountingDeleter(&deletes));
    // Call deleter manually.
    EXPECT_EQ(0, deletes);
    scoper.get_deleter()(&dummy_value);
    EXPECT_EQ(1, deletes);

    // Deleter is still there after reset.
    scoper.reset();
    EXPECT_EQ(2, deletes);
    scoper.get_deleter()(&dummy_value);
    EXPECT_EQ(3, deletes);

    // Deleter can be assigned into (matches C++11 unique_ptr<> spec).
    scoper.get_deleter() = CountingDeleter(&alternate_deletes);
    scoper.reset(&dummy_value);
    EXPECT_EQ(0, alternate_deletes);

  }
  EXPECT_EQ(3, deletes);
  EXPECT_EQ(1, alternate_deletes);

  // Test operator= deleter support.
  deletes = 0;
  alternate_deletes = 0;
  {
    double dummy_value2;
    scoped_ptr<double, CountingDeleter> scoper(&dummy_value,
                                               CountingDeleter(&deletes));
    scoped_ptr<double, CountingDeleter> scoper2(
        &dummy_value2,
        CountingDeleter(&alternate_deletes));
    EXPECT_EQ(0, deletes);
    EXPECT_EQ(0, alternate_deletes);

    // Pass the second deleter through a constructor and an operator=. Then
    // reinitialize the empty scopers to ensure that each one is deleting
    // properly.
    scoped_ptr<double, CountingDeleter> scoper3(std::move(scoper2));
    scoper = std::move(scoper3);
    EXPECT_EQ(1, deletes);

    scoper2.reset(&dummy_value2);
    scoper3.reset(&dummy_value2);
    EXPECT_EQ(0, alternate_deletes);

  }
  EXPECT_EQ(1, deletes);
  EXPECT_EQ(3, alternate_deletes);

  // Test swap(), and type-safe Boolean.
  {
    scoped_ptr<double, CountingDeleter> scoper1(NULL,
                                                CountingDeleter(&deletes));
    scoped_ptr<double, CountingDeleter> scoper2(NULL,
                                                CountingDeleter(&deletes));
    EXPECT_TRUE(scoper1.get() == scoper2.get());
    EXPECT_FALSE(scoper1.get() != scoper2.get());

    scoper1.reset(&dummy_value);
    EXPECT_TRUE(scoper1);
    EXPECT_EQ(&dummy_value, scoper1.get());
    EXPECT_FALSE(scoper2);
    EXPECT_FALSE(scoper2.get());
    EXPECT_FALSE(scoper1.get() == scoper2.get());
    EXPECT_TRUE(scoper1.get() != scoper2.get());

    scoper2.swap(scoper1);
    EXPECT_EQ(&dummy_value, scoper2.get());
    EXPECT_FALSE(scoper1.get());
    EXPECT_FALSE(scoper1.get() == scoper2.get());
    EXPECT_TRUE(scoper1.get() != scoper2.get());
  }
}

// Sanity check test for overloaded new and delete operators. Does not do full
// coverage of reset/release/move operations as that is redundant with the
// above.
TEST(ScopedPtrTest, OverloadedNewAndDelete) {
  {
    OverloadedNewAndDelete::ResetCounters();
    scoped_ptr<OverloadedNewAndDelete> scoper(new OverloadedNewAndDelete());
    EXPECT_TRUE(scoper.get());

    scoped_ptr<OverloadedNewAndDelete> scoper2(std::move(scoper));
  }
  EXPECT_EQ(1, OverloadedNewAndDelete::delete_count());
  EXPECT_EQ(1, OverloadedNewAndDelete::new_count());
}

scoped_ptr<int> NullIntReturn() {
  return nullptr;
}

TEST(ScopedPtrTest, Nullptr) {
  scoped_ptr<int> scoper1(nullptr);
  scoped_ptr<int> scoper2(new int);
  scoper2 = nullptr;
  scoped_ptr<int> scoper3(NullIntReturn());
  scoped_ptr<int> scoper4 = NullIntReturn();
  EXPECT_EQ(nullptr, scoper1.get());
  EXPECT_EQ(nullptr, scoper2.get());
  EXPECT_EQ(nullptr, scoper3.get());
  EXPECT_EQ(nullptr, scoper4.get());
}

scoped_ptr<int[]> NullIntArrayReturn() {
  return nullptr;
}

TEST(ScopedPtrTest, NullptrArray) {
  scoped_ptr<int[]> scoper1(nullptr);
  scoped_ptr<int[]> scoper2(new int[3]);
  scoper2 = nullptr;
  scoped_ptr<int[]> scoper3(NullIntArrayReturn());
  scoped_ptr<int[]> scoper4 = NullIntArrayReturn();
  EXPECT_EQ(nullptr, scoper1.get());
  EXPECT_EQ(nullptr, scoper2.get());
  EXPECT_EQ(nullptr, scoper3.get());
  EXPECT_EQ(nullptr, scoper4.get());
}

class Super {};
class Sub : public Super {};

scoped_ptr<Sub> SubClassReturn() {
  return make_scoped_ptr(new Sub);
}

TEST(ScopedPtrTest, Conversion) {
  scoped_ptr<Sub> sub1(new Sub);
  scoped_ptr<Sub> sub2(new Sub);

  // Upcast with move works.
  scoped_ptr<Super> super1 = std::move(sub1);
  super1 = std::move(sub2);

  // Upcast with an rvalue works.
  scoped_ptr<Super> super2 = SubClassReturn();
  super2 = SubClassReturn();
}

// Logging a scoped_ptr<T> to an ostream shouldn't convert it to a boolean
// value first.
TEST(ScopedPtrTest, LoggingDoesntConvertToBoolean) {
  scoped_ptr<int> x(new int);
  std::stringstream s1;
  s1 << x;

  std::stringstream s2;
  s2 << x.get();

  EXPECT_EQ(s2.str(), s1.str());
}

TEST(ScopedPtrTest, ReferenceCycle) {
  struct StructB;
  struct StructA {
    scoped_ptr<StructB> b;
  };

  struct StructB {
    scoped_ptr<StructA> a;
  };

  // Create a reference cycle.
  StructA* a = new StructA;
  a->b.reset(new StructB);
  a->b->a.reset(a);

  // Break the cycle by calling reset(). This will cause |a| (and hence, |a->b|)
  // to be deleted before the call to reset() returns. This tests that the
  // implementation of scoped_ptr::reset() doesn't access |this| after it
  // deletes the underlying pointer. This behaviour is consistent with the
  // definition of unique_ptr::reset in C++11.
  a->b.reset();

  // Go again, but this time, break the cycle by invoking |a|'s destructor. This
  // tests that the implementation of ~scoped_ptr doesn't infinitely recurse
  // into the destructors of |a| and |a->b|. Note, deleting |a| instead will
  // cause |a| to be double-free'd because |a->b| owns |a| and deletes it via
  // its destructor.
  a = new StructA;
  a->b.reset(new StructB);
  a->b->a.reset(a);
  a->~StructA();
}

TEST(ScopedPtrTest, Operators) {
  struct Parent {};
  struct Child : public Parent {};

  scoped_ptr<Parent> p(new Parent);
  scoped_ptr<Parent> p2(new Parent);
  scoped_ptr<Child> c(new Child);
  scoped_ptr<Parent> pnull;

  // Operator==.
  EXPECT_TRUE(p == p);
  EXPECT_FALSE(p == c);
  EXPECT_FALSE(p == p2);
  EXPECT_FALSE(p == pnull);

  EXPECT_FALSE(p == nullptr);
  EXPECT_FALSE(nullptr == p);
  EXPECT_TRUE(pnull == nullptr);
  EXPECT_TRUE(nullptr == pnull);

  // Operator!=.
  EXPECT_FALSE(p != p);
  EXPECT_TRUE(p != c);
  EXPECT_TRUE(p != p2);
  EXPECT_TRUE(p != pnull);

  EXPECT_TRUE(p != nullptr);
  EXPECT_TRUE(nullptr != p);
  EXPECT_FALSE(pnull != nullptr);
  EXPECT_FALSE(nullptr != pnull);

  // Compare two scoped_ptr<T>.
  EXPECT_EQ(p.get() < p2.get(), p < p2);
  EXPECT_EQ(p.get() <= p2.get(), p <= p2);
  EXPECT_EQ(p.get() > p2.get(), p > p2);
  EXPECT_EQ(p.get() >= p2.get(), p >= p2);
  EXPECT_EQ(p2.get() < p.get(), p2 < p);
  EXPECT_EQ(p2.get() <= p.get(), p2 <= p);
  EXPECT_EQ(p2.get() > p.get(), p2 > p);
  EXPECT_EQ(p2.get() >= p.get(), p2 >= p);

  // And convertible scoped_ptr<T> and scoped_ptr<U>.
  EXPECT_EQ(p.get() < c.get(), p < c);
  EXPECT_EQ(p.get() <= c.get(), p <= c);
  EXPECT_EQ(p.get() > c.get(), p > c);
  EXPECT_EQ(p.get() >= c.get(), p >= c);
  EXPECT_EQ(c.get() < p.get(), c < p);
  EXPECT_EQ(c.get() <= p.get(), c <= p);
  EXPECT_EQ(c.get() > p.get(), c > p);
  EXPECT_EQ(c.get() >= p.get(), c >= p);

  // Compare to nullptr.
  EXPECT_TRUE(p > nullptr);
  EXPECT_FALSE(nullptr > p);
  EXPECT_FALSE(pnull > nullptr);
  EXPECT_FALSE(nullptr > pnull);

  EXPECT_TRUE(p >= nullptr);
  EXPECT_FALSE(nullptr >= p);
  EXPECT_TRUE(pnull >= nullptr);
  EXPECT_TRUE(nullptr >= pnull);

  EXPECT_FALSE(p < nullptr);
  EXPECT_TRUE(nullptr < p);
  EXPECT_FALSE(pnull < nullptr);
  EXPECT_FALSE(nullptr < pnull);

  EXPECT_FALSE(p <= nullptr);
  EXPECT_TRUE(nullptr <= p);
  EXPECT_TRUE(pnull <= nullptr);
  EXPECT_TRUE(nullptr <= pnull);
};

TEST(ScopedPtrTest, ArrayOperators) {
  struct Parent {};
  struct Child : public Parent {};

  scoped_ptr<Parent[]> p(new Parent[1]);
  scoped_ptr<Parent[]> p2(new Parent[1]);
  scoped_ptr<Child[]> c(new Child[1]);
  scoped_ptr<Parent[]> pnull;

  // Operator==.
  EXPECT_TRUE(p == p);
  EXPECT_FALSE(p == c);
  EXPECT_FALSE(p == p2);
  EXPECT_FALSE(p == pnull);

  EXPECT_FALSE(p == nullptr);
  EXPECT_FALSE(nullptr == p);
  EXPECT_TRUE(pnull == nullptr);
  EXPECT_TRUE(nullptr == pnull);

  // Operator!=.
  EXPECT_FALSE(p != p);
  EXPECT_TRUE(p != c);
  EXPECT_TRUE(p != p2);
  EXPECT_TRUE(p != pnull);

  EXPECT_TRUE(p != nullptr);
  EXPECT_TRUE(nullptr != p);
  EXPECT_FALSE(pnull != nullptr);
  EXPECT_FALSE(nullptr != pnull);

  // Compare two scoped_ptr<T>.
  EXPECT_EQ(p.get() < p2.get(), p < p2);
  EXPECT_EQ(p.get() <= p2.get(), p <= p2);
  EXPECT_EQ(p.get() > p2.get(), p > p2);
  EXPECT_EQ(p.get() >= p2.get(), p >= p2);
  EXPECT_EQ(p2.get() < p.get(), p2 < p);
  EXPECT_EQ(p2.get() <= p.get(), p2 <= p);
  EXPECT_EQ(p2.get() > p.get(), p2 > p);
  EXPECT_EQ(p2.get() >= p.get(), p2 >= p);

  // And convertible scoped_ptr<T> and scoped_ptr<U>.
  EXPECT_EQ(p.get() < c.get(), p < c);
  EXPECT_EQ(p.get() <= c.get(), p <= c);
  EXPECT_EQ(p.get() > c.get(), p > c);
  EXPECT_EQ(p.get() >= c.get(), p >= c);
  EXPECT_EQ(c.get() < p.get(), c < p);
  EXPECT_EQ(c.get() <= p.get(), c <= p);
  EXPECT_EQ(c.get() > p.get(), c > p);
  EXPECT_EQ(c.get() >= p.get(), c >= p);

  // Compare to nullptr.
  EXPECT_TRUE(p > nullptr);
  EXPECT_FALSE(nullptr > p);
  EXPECT_FALSE(pnull > nullptr);
  EXPECT_FALSE(nullptr > pnull);

  EXPECT_TRUE(p >= nullptr);
  EXPECT_FALSE(nullptr >= p);
  EXPECT_TRUE(pnull >= nullptr);
  EXPECT_TRUE(nullptr >= pnull);

  EXPECT_FALSE(p < nullptr);
  EXPECT_TRUE(nullptr < p);
  EXPECT_FALSE(pnull < nullptr);
  EXPECT_FALSE(nullptr < pnull);

  EXPECT_FALSE(p <= nullptr);
  EXPECT_TRUE(nullptr <= p);
  EXPECT_TRUE(pnull <= nullptr);
  EXPECT_TRUE(nullptr <= pnull);
}

// Boolean tests can be performed.
TEST(ScopedPtrTest, BooleanTesting) {
  scoped_ptr<int> ptr_to_an_instance(new int);
  EXPECT_TRUE(ptr_to_an_instance);
  EXPECT_FALSE(!ptr_to_an_instance);

  if (ptr_to_an_instance) {
  } else {
    ADD_FAILURE() << "Pointer to an instance should result in true.";
  }

  if (!ptr_to_an_instance) {  // check for operator!().
    ADD_FAILURE() << "Pointer to an instance should result in !x being false.";
  }

  scoped_ptr<int> null_ptr;
  EXPECT_FALSE(null_ptr);
  EXPECT_TRUE(!null_ptr);

  if (null_ptr) {
    ADD_FAILURE() << "Null pointer should result in false.";
  }

  if (!null_ptr) {  // check for operator!().
  } else {
    ADD_FAILURE() << "Null pointer should result in !x being true.";
  }
}
