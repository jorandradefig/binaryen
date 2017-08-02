/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// A WebAssembly optimizer, loads code, optionally runs passes on it,
// then writes it.
//

#include <memory>

#include "pass.h"
#include "support/command-line.h"
#include "support/file.h"
#include "wasm-printing.h"
#include "wasm-s-parser.h"
#include "wasm-validator.h"
#include "wasm-io.h"
#include "wasm-interpreter.h"
#include "wasm-binary.h"
#include "shell-interface.h"
#include "optimization-options.h"
#include "execution-results.h"
#include "translate-to-fuzz.h"
#include "js-wrapper.h"

using namespace wasm;

//
// main
//

int main(int argc, const char* argv[]) {
  Name entry;
  std::vector<std::string> passes;
  bool emitBinary = true;
  bool debugInfo = false;
  bool fuzzExec = false;
  bool fuzzBinary = false;
  bool translateToFuzz = false;
  std::string emitJSWrapper;

  OptimizationOptions options("wasm-opt", "Read, write, and optimize files");
  options
      .add("--output", "-o", "Output file (stdout if not specified)",
           Options::Arguments::One,
           [](Options* o, const std::string& argument) {
             o->extra["output"] = argument;
             Colors::disable();
           })
      .add("--emit-text", "-S", "Emit text instead of binary for the output file",
           Options::Arguments::Zero,
           [&](Options *o, const std::string &argument) { emitBinary = false; })
      .add("--debuginfo", "-g", "Emit names section and debug info",
           Options::Arguments::Zero,
           [&](Options *o, const std::string &arguments) { debugInfo = true; })
      .add("--fuzz-exec", "-fe", "Execute functions before and after optimization, helping fuzzing find bugs",
           Options::Arguments::Zero,
           [&](Options *o, const std::string &arguments) { fuzzExec = true; })
      .add("--fuzz-binary", "-fb", "Convert to binary and back after optimizations and before fuzz-exec, helping fuzzing find binary format bugs",
           Options::Arguments::Zero,
           [&](Options *o, const std::string &arguments) { fuzzBinary = true; })
      .add("--translate-to-fuzz", "-ttf", "Translate the input into a valid wasm module *somehow*, useful for fuzzing",
           Options::Arguments::Zero,
           [&](Options *o, const std::string &arguments) { translateToFuzz = true; })
      .add("--emit-js-wrapper", "-ejw", "Emit a JavaScript wrapper file that can run the wasm with some test values, useful for fuzzing",
           Options::Arguments::One,
           [&](Options *o, const std::string &arguments) { emitJSWrapper = arguments; })
      .add_positional("INFILE", Options::Arguments::One,
                      [](Options* o, const std::string& argument) {
                        o->extra["infile"] = argument;
                      });
  options.parse(argc, argv);

  Module wasm;

  if (options.debug) std::cerr << "reading...\n";

  if (!translateToFuzz) {
    ModuleReader reader;
    reader.setDebug(options.debug);
    try {
      reader.read(options.extra["infile"], wasm);
    } catch (ParseException& p) {
      p.dump(std::cerr);
      Fatal() << "error in parsing input";
    } catch (std::bad_alloc& b) {
      Fatal() << "error in building module, std::bad_alloc (possibly invalid request for silly amounts of memory)";
    }

    if (!WasmValidator().validate(wasm)) {
      Fatal() << "error in validating input";
    }
  } else {
    // translate-to-fuzz
    TranslateToFuzzReader reader(wasm);
    reader.read(options.extra["infile"]);
    if (!WasmValidator().validate(wasm)) {
      std::cerr << "translate-to-fuzz must always generate a valid module";
      abort();
    }
  }

  ExecutionResults results;
  if (fuzzExec) {
    results.get(wasm);
  }

  if (options.runningPasses()) {
    if (options.debug) std::cerr << "running passes...\n";
    PassRunner passRunner = options.getPassRunner(wasm);
    passRunner.run();
    assert(WasmValidator().validate(wasm));
  }

  if (fuzzExec) {
    auto* compare = &wasm;
    Module second;
    if (fuzzBinary) {
      compare = &second;
      BufferWithRandomAccess buffer(false);
      // write the binary
      WasmBinaryWriter writer(&wasm, buffer, false);
      writer.write();
      // read the binary
      auto input = buffer.getAsChars();
      WasmBinaryBuilder parser(second, input, false);
      parser.read();
      assert(WasmValidator().validate(second));
    }
    results.check(*compare);
  }

  if (options.extra.count("output") > 0) {
    if (options.debug) std::cerr << "writing..." << std::endl;
    ModuleWriter writer;
    writer.setDebug(options.debug);
    writer.setBinary(emitBinary);
    writer.setDebugInfo(debugInfo);
    writer.write(wasm, options.extra["output"]);
  }

  if (emitJSWrapper.size() > 0) {
    std::ofstream outfile;
    outfile.open(emitJSWrapper, std::ofstream::out);
    outfile << generateJSWrapper(wasm);
    outfile.close();
  }
}
