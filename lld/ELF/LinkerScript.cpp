//===- LinkerScript.cpp ---------------------------------------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the parser/evaluator of the linker script.
// It does not construct an AST but consume linker script directives directly.
// Results are written to Driver or Config object.
//
//===----------------------------------------------------------------------===//

#include "LinkerScript.h"
#include "Config.h"
#include "Driver.h"
#include "InputSection.h"
#include "SymbolTable.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/StringSaver.h"

using namespace llvm;
using namespace llvm::object;
using namespace lld;
using namespace lld::elf2;

LinkerScript *elf2::Script;

template <class ELFT>
StringRef LinkerScript::getOutputSection(InputSectionBase<ELFT> *S) {
  for (SectionRule &R : Sections)
    if (R.match(S))
      return R.Dest;
  return "";
}

template <class ELFT>
bool LinkerScript::isDiscarded(InputSectionBase<ELFT> *S) {
  return getOutputSection(S) == "/DISCARD/";
}

// A compartor to sort output sections. Returns -1 or 1 if both
// A and B are mentioned in linker scripts. Otherwise, returns 0
// to use the default rule which is implemented in Writer.cpp.
int LinkerScript::compareSections(StringRef A, StringRef B) {
  auto E = SectionOrder.end();
  auto I = std::find(SectionOrder.begin(), E, A);
  auto J = std::find(SectionOrder.begin(), E, B);
  if (I == E || J == E)
    return 0;
  return I < J ? -1 : 1;
}

// Returns true if S matches T. S may contain a meta character '*'
// which matches zero or more occurrences of any character.
static bool matchStr(StringRef S, StringRef T) {
  for (;;) {
    if (S.empty())
      return T.empty();
    if (S[0] == '*') {
      S = S.substr(1);
      if (S.empty())
        // Fast path. If a pattern is '*', it matches anything.
        return true;
      for (size_t I = 0, E = T.size(); I < E; ++I)
        if (matchStr(S, T.substr(I)))
          return true;
      return false;
    }
    if (T.empty() || S[0] != T[0])
      return false;
    S = S.substr(1);
    T = T.substr(1);
  }
}

template <class ELFT> bool SectionRule::match(InputSectionBase<ELFT> *S) {
  return matchStr(SectionPattern, S->getSectionName());
}

class elf2::ScriptParser {
public:
  ScriptParser(BumpPtrAllocator *A, StringRef S, bool B)
      : Saver(*A), Tokens(tokenize(S)), IsUnderSysroot(B) {
    Cmd["ENTRY"] = &ScriptParser::readEntry;
    Cmd["EXTERN"] = &ScriptParser::readExtern;
    Cmd["GROUP"] = &ScriptParser::readGroup;
    Cmd["INCLUDE"] = &ScriptParser::readInclude;
    Cmd["INPUT"] = &ScriptParser::readGroup;
    Cmd["OUTPUT"] = &ScriptParser::readOutput;
    Cmd["OUTPUT_ARCH"] = &ScriptParser::readOutputArch;
    Cmd["OUTPUT_FORMAT"] = &ScriptParser::readOutputFormat;
    Cmd["SEARCH_DIR"] = &ScriptParser::readSearchDir;
    Cmd["SECTIONS"] = &ScriptParser::readSections;
    Cmd[";"] = [](ScriptParser &) {};
  }

  void run();

private:
  void setError(const Twine &Msg);
  static std::vector<StringRef> tokenize(StringRef S);
  static StringRef skipSpace(StringRef S);
  bool atEOF();
  StringRef next();
  bool skip(StringRef Tok);
  void expect(StringRef Expect);

  void addFile(StringRef Path);

  void readAsNeeded();
  void readEntry();
  void readExtern();
  void readGroup();
  void readInclude();
  void readOutput();
  void readOutputArch();
  void readOutputFormat();
  void readSearchDir();
  void readSections();

  void readOutputSectionDescription();

  StringSaver Saver;
  std::vector<StringRef> Tokens;
  llvm::StringMap<std::function<void(ScriptParser &)>> Cmd;
  size_t Pos = 0;
  bool IsUnderSysroot;
  bool Error = false;
};

void ScriptParser::run() {
  while (!atEOF()) {
    StringRef Tok = next();
    auto It = Cmd.find(Tok);
    if (It != Cmd.end()) {
      std::function<void(ScriptParser &)> &Handler = It->second;
      Handler(*this);
    } else {
      setError("unknown directive: " + Tok);
    }
  }
}

// We don't want to record cascading errors. Keep only the first one.
void ScriptParser::setError(const Twine &Msg) {
  if (Error)
    return;
  error(Msg);
  Error = true;
}

// Split S into linker script tokens.
std::vector<StringRef> ScriptParser::tokenize(StringRef S) {
  std::vector<StringRef> Ret;
  for (;;) {
    S = skipSpace(S);
    if (S.empty())
      return Ret;

    // Quoted token
    if (S.startswith("\"")) {
      size_t E = S.find("\"", 1);
      if (E == StringRef::npos) {
        error("unclosed quote");
        return {};
      }
      Ret.push_back(S.substr(1, E - 1));
      S = S.substr(E + 1);
      continue;
    }

    // Unquoted token
    size_t Pos = S.find_first_not_of(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
        "0123456789_.$/\\~=+[]*?-:");
    // A character that cannot start a word (which is usually a
    // punctuation) forms a single character token.
    if (Pos == 0)
      Pos = 1;
    Ret.push_back(S.substr(0, Pos));
    S = S.substr(Pos);
  }
}

// Skip leading whitespace characters or /**/-style comments.
StringRef ScriptParser::skipSpace(StringRef S) {
  for (;;) {
    if (S.startswith("/*")) {
      size_t E = S.find("*/", 2);
      if (E == StringRef::npos) {
        error("unclosed comment in a linker script");
        return "";
      }
      S = S.substr(E + 2);
      continue;
    }
    size_t Size = S.size();
    S = S.ltrim();
    if (S.size() == Size)
      return S;
  }
}

// An errneous token is handled as if it were the last token before EOF.
bool ScriptParser::atEOF() { return Error || Tokens.size() == Pos; }

StringRef ScriptParser::next() {
  if (Error)
    return "";
  if (atEOF()) {
    setError("unexpected EOF");
    return "";
  }
  return Tokens[Pos++];
}

bool ScriptParser::skip(StringRef Tok) {
  if (Error)
    return false;
  if (atEOF()) {
    setError("unexpected EOF");
    return false;
  }
  if (Tok != Tokens[Pos])
    return false;
  ++Pos;
  return true;
}

void ScriptParser::expect(StringRef Expect) {
  if (Error)
    return;
  StringRef Tok = next();
  if (Tok != Expect)
    setError(Expect + " expected, but got " + Tok);
}

void ScriptParser::addFile(StringRef S) {
  if (IsUnderSysroot && S.startswith("/")) {
    SmallString<128> Path;
    (Config->Sysroot + S).toStringRef(Path);
    if (sys::fs::exists(Path)) {
      Driver->addFile(Saver.save(Path.str()));
      return;
    }
  }

  if (sys::path::is_absolute(S)) {
    Driver->addFile(S);
  } else if (S.startswith("=")) {
    if (Config->Sysroot.empty())
      Driver->addFile(S.substr(1));
    else
      Driver->addFile(Saver.save(Config->Sysroot + "/" + S.substr(1)));
  } else if (S.startswith("-l")) {
    Driver->addLibrary(S.substr(2));
  } else if (sys::fs::exists(S)) {
    Driver->addFile(S);
  } else {
    std::string Path = findFromSearchPaths(S);
    if (Path.empty())
      setError("Unable to find " + S);
    else
      Driver->addFile(Saver.save(Path));
  }
}

void ScriptParser::readAsNeeded() {
  expect("(");
  bool Orig = Config->AsNeeded;
  Config->AsNeeded = true;
  while (!Error) {
    StringRef Tok = next();
    if (Tok == ")")
      break;
    addFile(Tok);
  }
  Config->AsNeeded = Orig;
}

void ScriptParser::readEntry() {
  // -e <symbol> takes predecence over ENTRY(<symbol>).
  expect("(");
  StringRef Tok = next();
  if (Config->Entry.empty())
    Config->Entry = Tok;
  expect(")");
}

void ScriptParser::readExtern() {
  expect("(");
  while (!Error) {
    StringRef Tok = next();
    if (Tok == ")")
      return;
    Config->Undefined.push_back(Tok);
  }
}

void ScriptParser::readGroup() {
  expect("(");
  while (!Error) {
    StringRef Tok = next();
    if (Tok == ")")
      return;
    if (Tok == "AS_NEEDED") {
      readAsNeeded();
      continue;
    }
    addFile(Tok);
  }
}

void ScriptParser::readInclude() {
  StringRef Tok = next();
  auto MBOrErr = MemoryBuffer::getFile(Tok);
  if (!MBOrErr) {
    setError("cannot open " + Tok);
    return;
  }
  std::unique_ptr<MemoryBuffer> &MB = *MBOrErr;
  StringRef S = Saver.save(MB->getMemBufferRef().getBuffer());
  std::vector<StringRef> V = tokenize(S);
  Tokens.insert(Tokens.begin() + Pos, V.begin(), V.end());
}

void ScriptParser::readOutput() {
  // -o <file> takes predecence over OUTPUT(<file>).
  expect("(");
  StringRef Tok = next();
  if (Config->OutputFile.empty())
    Config->OutputFile = Tok;
  expect(")");
}

void ScriptParser::readOutputArch() {
  // Error checking only for now.
  expect("(");
  next();
  expect(")");
}

void ScriptParser::readOutputFormat() {
  // Error checking only for now.
  expect("(");
  next();
  StringRef Tok = next();
  if (Tok == ")")
   return;
  if (Tok != ",") {
    setError("unexpected token: " + Tok);
    return;
  }
  next();
  expect(",");
  next();
  expect(")");
}

void ScriptParser::readSearchDir() {
  expect("(");
  Config->SearchPaths.push_back(next());
  expect(")");
}

void ScriptParser::readSections() {
  expect("{");
  while (!Error && !skip("}"))
    readOutputSectionDescription();
}

void ScriptParser::readOutputSectionDescription() {
  StringRef OutSec = next();
  Script->SectionOrder.push_back(OutSec);
  expect(":");
  expect("{");
  while (!Error && !skip("}")) {
    next(); // Skip input file name.
    expect("(");
    while (!Error && !skip(")"))
      Script->Sections.push_back({OutSec, next()});
  }
}

static bool isUnderSysroot(StringRef Path) {
  if (Config->Sysroot == "")
    return false;
  for (; !Path.empty(); Path = sys::path::parent_path(Path))
    if (sys::fs::equivalent(Config->Sysroot, Path))
      return true;
  return false;
}

// Entry point. The other functions or classes are private to this file.
void LinkerScript::read(MemoryBufferRef MB) {
  StringRef Path = MB.getBufferIdentifier();
  ScriptParser(&Alloc, MB.getBuffer(), isUnderSysroot(Path)).run();
}

template StringRef LinkerScript::getOutputSection(InputSectionBase<ELF32LE> *);
template StringRef LinkerScript::getOutputSection(InputSectionBase<ELF32BE> *);
template StringRef LinkerScript::getOutputSection(InputSectionBase<ELF64LE> *);
template StringRef LinkerScript::getOutputSection(InputSectionBase<ELF64BE> *);

template bool LinkerScript::isDiscarded(InputSectionBase<ELF32LE> *);
template bool LinkerScript::isDiscarded(InputSectionBase<ELF32BE> *);
template bool LinkerScript::isDiscarded(InputSectionBase<ELF64LE> *);
template bool LinkerScript::isDiscarded(InputSectionBase<ELF64BE> *);

template bool SectionRule::match(InputSectionBase<ELF32LE> *);
template bool SectionRule::match(InputSectionBase<ELF32BE> *);
template bool SectionRule::match(InputSectionBase<ELF64LE> *);
template bool SectionRule::match(InputSectionBase<ELF64BE> *);
