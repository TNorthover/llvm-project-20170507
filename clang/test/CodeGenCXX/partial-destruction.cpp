// RUN: %clang_cc1 %s -triple=x86_64-apple-darwin10 -emit-llvm -o - -fcxx-exceptions -fexceptions | FileCheck %s

// Test IR generation for partial destruction of aggregates.

void opaque();

// Initializer lists.
namespace test0 {
  struct A { A(int); A(); ~A(); void *v; };
  void test() {
    A as[10] = { 5, 7 };
    opaque();
  }
  // CHECK:    define void @_ZN5test04testEv()
  // CHECK:      [[AS:%.*]] = alloca [10 x [[A:%.*]]], align
  // CHECK-NEXT: [[ENDVAR:%.*]] = alloca [[A]]*
  // CHECK-NEXT: [[EXN:%.*]] = alloca i8*
  // CHECK-NEXT: [[SEL:%.*]] = alloca i32
  // CHECK-NEXT: [[CLEANUP:%.*]] = alloca i32

  // Initialize.
  // CHECK-NEXT: [[E_BEGIN:%.*]] = getelementptr inbounds [10 x [[A]]]* [[AS]], i64 0, i64 0
  // CHECK-NEXT: store [[A]]* [[E_BEGIN]], [[A]]** [[ENDVAR]]
  // CHECK-NEXT: invoke void @_ZN5test01AC1Ei([[A]]* [[E_BEGIN]], i32 5)
  // CHECK:      [[E1:%.*]] = getelementptr inbounds [[A]]* [[E_BEGIN]], i64 1
  // CHECK-NEXT: store [[A]]* [[E1]], [[A]]** [[ENDVAR]]
  // CHECK-NEXT: invoke void @_ZN5test01AC1Ei([[A]]* [[E1]], i32 7)
  // CHECK:      [[E2:%.*]] = getelementptr inbounds [[A]]* [[E1]], i64 1
  // CHECK-NEXT: store [[A]]* [[E2]], [[A]]** [[ENDVAR]]
  // CHECK-NEXT: [[E_END:%.*]] = getelementptr inbounds [[A]]* [[E_BEGIN]], i64 10
  // CHECK-NEXT: br label
  // CHECK:      [[E_CUR:%.*]] = phi [[A]]* [ [[E2]], {{%.*}} ], [ [[E_NEXT:%.*]], {{%.*}} ]
  // CHECK-NEXT: invoke void @_ZN5test01AC1Ev([[A]]* [[E_CUR]])
  // CHECK:      [[E_NEXT]] = getelementptr inbounds [[A]]* [[E_CUR]], i64 1
  // CHECK-NEXT: store [[A]]* [[E_NEXT]], [[A]]** [[ENDVAR]]
  // CHECK-NEXT: [[T0:%.*]] = icmp eq [[A]]* [[E_NEXT]], [[E_END]]
  // CHECK-NEXT: br i1 [[T0]],

  // Run.
  // CHECK:      invoke void @_Z6opaquev()

  // Normal destroy.
  // CHECK:      [[ED_BEGIN:%.*]] = getelementptr inbounds [10 x [[A]]]* [[AS]], i32 0, i32 0
  // CHECK-NEXT: [[ED_END:%.*]] = getelementptr inbounds [[A]]* [[ED_BEGIN]], i64 10
  // CHECK-NEXT: br label
  // CHECK:      [[ED_AFTER:%.*]] = phi [[A]]* [ [[ED_END]], {{%.*}} ], [ [[ED_CUR:%.*]], {{%.*}} ]
  // CHECK-NEXT: [[ED_CUR]] = getelementptr inbounds [[A]]* [[ED_AFTER]], i64 -1
  // CHECK-NEXT: invoke void @_ZN5test01AD1Ev([[A]]* [[ED_CUR]])
  // CHECK:      [[T0:%.*]] = icmp eq [[A]]* [[ED_CUR]], [[ED_BEGIN]]
  // CHECK-NEXT: br i1 [[T0]],
  // CHECK:      ret void

  // Partial destroy for initialization.
  // CHECK:      llvm.eh.selector({{.*}}, i32 0)
  // CHECK:      [[PARTIAL_END:%.*]] = load [[A]]** [[ENDVAR]]
  // CHECK-NEXT: [[T0:%.*]] = icmp eq [[A]]* [[E_BEGIN]], [[PARTIAL_END]]
  // CHECK-NEXT: br i1 [[T0]],
  // CHECK:      [[E_AFTER:%.*]] = phi [[A]]* [ [[PARTIAL_END]], {{%.*}} ], [ [[E_CUR:%.*]], {{%.*}} ]
  // CHECK-NEXT: [[E_CUR]] = getelementptr inbounds [[A]]* [[E_AFTER]], i64 -1
  // CHECK-NEXT: invoke void @_ZN5test01AD1Ev([[A]]* [[E_CUR]])
  // CHECK:      [[T0:%.*]] = icmp eq [[A]]* [[E_CUR]], [[E_BEGIN]]
  // CHECK-NEXT: br i1 [[T0]],

  // Primary EH destructor.
  // CHECK:      llvm.eh.selector({{.*}}, i32 0)
  // CHECK:      [[E0:%.*]] = getelementptr inbounds [10 x [[A]]]* [[AS]], i32 0, i32 0
  // CHECK-NEXT: [[E_END:%.*]] = getelementptr inbounds [[A]]* [[E0]], i64 10
  // CHECK-NEXT: br label

  // Partial destructor for primary normal destructor.
  // FIXME: There's some really bad block ordering here which causes
  // the partial destroy for the primary normal destructor to fall
  // within the primary EH destructor.
  // CHECK:      llvm.eh.selector({{.*}}, i32 0)
  // CHECK:      [[T0:%.*]] = icmp eq [[A]]* [[ED_BEGIN]], [[ED_CUR]]
  // CHECK-NEXT: br i1 [[T0]]
  // CHECK:      [[EDD_AFTER:%.*]] = phi [[A]]* [ [[ED_CUR]], {{%.*}} ], [ [[EDD_CUR:%.*]], {{%.*}} ]
  // CHECK-NEXT: [[EDD_CUR]] = getelementptr inbounds [[A]]* [[EDD_AFTER]], i64 -1
  // CHECK-NEXT: invoke void @_ZN5test01AD1Ev([[A]]* [[EDD_CUR]])
  // CHECK:      [[T0:%.*]] = icmp eq [[A]]* [[EDD_CUR]], [[ED_BEGIN]]
  // CHECK-NEXT: br i1 [[T0]]

  // Back to the primary EH destructor.
  // CHECK:      [[E_AFTER:%.*]] = phi [[A]]* [ [[E_END]], {{%.*}} ], [ [[E_CUR:%.*]], {{%.*}} ]
  // CHECK-NEXT: [[E_CUR]] = getelementptr inbounds [[A]]* [[E_AFTER]], i64 -1
  // CHECK-NEXT: invoke void @_ZN5test01AD1Ev([[A]]* [[E_CUR]])
  // CHECK:      [[T0:%.*]] = icmp eq [[A]]* [[E_CUR]], [[E0]]
  // CHECK-NEXT: br i1 [[T0]],

}
