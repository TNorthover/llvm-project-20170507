//===-- LocalDebugDelegate.cpp ----------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "LocalDebugDelegate.h"
#include "ProcessWindows.h"

using namespace lldb;
using namespace lldb_private;

LocalDebugDelegate::LocalDebugDelegate(ProcessSP process)
    : m_process(process)
{
}

void
LocalDebugDelegate::OnExitProcess(uint32_t exit_code)
{
    ((ProcessWindows &)*m_process).OnExitProcess(exit_code);
}

void
LocalDebugDelegate::OnDebuggerConnected()
{
    ((ProcessWindows &)*m_process).OnDebuggerConnected();
}

ExceptionResult
LocalDebugDelegate::OnDebugException(bool first_chance, const ExceptionRecord &record)
{
    return ((ProcessWindows &)*m_process).OnDebugException(first_chance, record);
}

void
LocalDebugDelegate::OnCreateThread(const HostThread &thread)
{
    ((ProcessWindows &)*m_process).OnCreateThread(thread);
}

void
LocalDebugDelegate::OnExitThread(const HostThread &thread)
{
    ((ProcessWindows &)*m_process).OnExitThread(thread);
}

void
LocalDebugDelegate::OnLoadDll()
{
    ((ProcessWindows &)*m_process).OnLoadDll();
}

void
LocalDebugDelegate::OnUnloadDll()
{
    ((ProcessWindows &)*m_process).OnUnloadDll();
}

void
LocalDebugDelegate::OnDebugString(const std::string &string)
{
    ((ProcessWindows &)*m_process).OnDebugString(string);
}

void
LocalDebugDelegate::OnDebuggerError(const Error &error, uint32_t type)
{
    ((ProcessWindows &)*m_process).OnDebuggerError(error, type);
}
