; RUN: llvm-as < %s | llvm-dis | FileCheck %s
; RUN: verify-uselistorder %s
; Basic smoke test for dep type.

; CHECK: %myStruct = type { i64, dep, i8 }
%myStruct = type { i64, dep, i8 }

define dep @test_sel(dep %a, dep %b, i1 %tst) {
; CHECK-LABEL: define dep @test_sel(dep %a, dep %b, i1 %tst) {
; CHECK: %res = select i1 %tst, dep %a, dep %b
; CHECL: ret dep %res

  %res = select i1 %tst, dep %a, dep %b
  ret dep %res
}

define dep @test_phi(dep %a, dep %b, i1 %tst) {
; CHECK-LABEL: define dep @test_phi(dep %a, dep %b, i1 %tst) {
; CHECK: %res = phi dep [ %a, %true ], [ %b, %false ]

  br i1 %tst, label %true, label %false

true:
  br label %end

false:
  br label %end

end:
  %res = phi dep [%a, %true], [%b, %false]
  ret dep %res
}

define void @test_load_store(dep* %ptr) {
; CHECK-LABEL: define void @test_load_store(dep* %ptr) {
; CHECK: %local = alloca dep, align 8
; CHECK: %val = load dep, dep* %ptr
; CHECK: store dep %val, dep* %local

  %local = alloca dep, align 8
  %val = load dep, dep* %ptr
  store dep %val, dep* %local
  ret void
}

define dep* @test_gep(dep* %in) {
; CHECK-LABEL: define dep* @test_gep(dep* %in) {
; CHECK: %res = getelementptr dep, dep* %in, i32 5

  %res = getelementptr dep, dep* %in, i32 5
  ret dep* %res
}

define %myStruct @test_insert_extract(%myStruct* %ptr) {
; CHECK-LABEL: define %myStruct @test_insert_extract(%myStruct* %ptr) {
; CHECK: %dep_field = extractvalue %myStruct %val, 1
; CHECK: %res = insertvalue %myStruct undef, dep %dep_field, 1

  %val = load %myStruct, %myStruct* %ptr
  %dep_field = extractvalue %myStruct %val, 1

  %res = insertvalue %myStruct undef, dep %dep_field, 1
  ret %myStruct %res
}

define float @chase_dep(i8* %ptr) {
; CHECK-LABEL: define float @chase_dep(i8* %ptr) {

  %cell0 = call { i8*, dep } @llvm.consume.load.p0i8.p0i8(i8* %ptr)
  %field.addr = extractvalue { i8*, dep } %cell0, 0
  %dep0 = extractvalue { i8*, dep } %cell0, 1

  %cell1 = call { i8*, dep } @llvm.consume.dependent.load.p0i8.p0i8(i8* %field.addr, dep %dep0)
  %float.addr = extractvalue { i8*, dep } %cell1, 0
  %dep1 = extractvalue { i8*, dep } %cell1, 1

  %res = call float @llvm.dependent.load.f32.p0i8(i8* %float.addr, dep %dep1)

  ret float %res
}

define dep @test_combine(dep %a, dep %b) {
; CHECK-LABEL: define dep @test_combine(dep %a, dep %b) {
; CHECK: %res = call dep @llvm.combine.deps(dep %a, dep %b)

  %res = call dep @llvm.combine.deps(dep %a, dep %b)
  ret dep %res
}

declare { i8*, dep } @llvm.consume.load.p0i8.p0i8(i8*)
declare { i8*, dep } @llvm.consume.dependent.load.p0i8.p0i8(i8*, dep)
declare float @llvm.dependent.load.f32.p0i8(i8*, dep)
declare dep @llvm.combine.deps(dep, dep)
