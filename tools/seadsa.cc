///
// seadsda -- Print heap graph computed by DSA
///

#include "llvm/LinkAllPasses.h"
#include "llvm/PassManager.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/Verifier.h"

#include "sea_dsa/DsaAnalysis.hh"

static llvm::cl::opt<std::string>
InputFilename(llvm::cl::Positional, llvm::cl::desc("<input LLVM bitcode file>"),
              llvm::cl::Required, llvm::cl::value_desc("filename"));

static llvm::cl::opt<std::string>
AsmOutputFilename("oll", llvm::cl::desc("Output analyzed bitcode"),
               llvm::cl::init(""), llvm::cl::value_desc("filename"));

static llvm::cl::opt<std::string>
DefaultDataLayout("-data-layout",
        llvm::cl::desc("data layout string to use if not specified by module"),
        llvm::cl::init(""), llvm::cl::value_desc("layout-string"));

static llvm::cl::opt<bool> 
PrintDsaStats ("sea-dsa-stats",
               llvm::cl::desc ("Print Dsa statistics"), 
               llvm::cl::init(false));

static llvm::cl::opt<bool>
MemDot("sea-dsa-dot",
       llvm::cl::desc("Print memory graph of each function to dot format"),
       llvm::cl::init(false));

static llvm::cl::opt<bool>
MemViewer("sea-dsa-viewer",
	  llvm::cl::desc("View memory graph of each function to dot format"),
	  llvm::cl::init(false));

int main(int argc, char **argv) {

  llvm::llvm_shutdown_obj shutdown;  // calls llvm_shutdown() on exit
  llvm::cl::ParseCommandLineOptions(argc, argv, "Heap Analysis");
                                    

  llvm::sys::PrintStackTraceOnErrorSignal();
  llvm::PrettyStackTraceProgram PSTP(argc, argv);
  llvm::EnableDebugBuffering = true;

  std::error_code error_code;
  llvm::SMDiagnostic err;
  llvm::LLVMContext &context = llvm::getGlobalContext();
  std::unique_ptr<llvm::Module> module;
  std::unique_ptr<llvm::tool_output_file> asmOutput;

  module = llvm::parseIRFile(InputFilename, err, context);
  if (module.get() == 0)
  {
    if (llvm::errs().has_colors()) llvm::errs().changeColor(llvm::raw_ostream::RED);
    llvm::errs() << "error: "
                 << "Bitcode was not properly read; " << err.getMessage() << "\n";
    if (llvm::errs().has_colors()) llvm::errs().resetColor();
    return 3;
  }

  if (!AsmOutputFilename.empty ())
    asmOutput =
      llvm::make_unique<llvm::tool_output_file>(AsmOutputFilename.c_str(), error_code,
                                                llvm::sys::fs::F_Text);
  if (error_code) {
    if (llvm::errs().has_colors())
      llvm::errs().changeColor(llvm::raw_ostream::RED);
    llvm::errs() << "error: Could not open " << AsmOutputFilename << ": "
                 << error_code.message () << "\n";
    if (llvm::errs().has_colors()) llvm::errs().resetColor();
    return 3;
  }

  ///////////////////////////////
  // initialise and run passes //
  ///////////////////////////////
  
  llvm::PassManager pass_manager;

  llvm::PassRegistry &Registry = *llvm::PassRegistry::getPassRegistry();
  llvm::initializeAnalysis(Registry);
  /// call graph and other IPA passes
  llvm::initializeIPA (Registry);

  // add an appropriate DataLayout instance for the module
  const llvm::DataLayout *dl = module->getDataLayout ();
  if (!dl && !DefaultDataLayout.empty ()) {
    module->setDataLayout (DefaultDataLayout);
    dl = module->getDataLayout ();
  }
  
  if (dl)
    pass_manager.add (new llvm::DataLayoutPass ());

  pass_manager.add (llvm::createVerifierPass());
  
  if (MemDot)
    pass_manager.add (sea_dsa::createDsaPrinterPass ());
  
  if (MemViewer)
    pass_manager.add (sea_dsa::createDsaViewerPass ());

  if (PrintDsaStats)
    pass_manager.add (sea_dsa::createDsaPrintStatsPass ());    
    
  if (!AsmOutputFilename.empty ())
    pass_manager.add (createPrintModulePass (asmOutput->os ()));
  
  pass_manager.run(*module.get());

  if (!AsmOutputFilename.empty ())
    asmOutput->keep ();
  
  return 0;
}
