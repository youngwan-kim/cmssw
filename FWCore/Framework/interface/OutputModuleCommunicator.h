#ifndef FWCore_Framework_OutputModuleCommunicator_h
#define FWCore_Framework_OutputModuleCommunicator_h
// -*- C++ -*-
//
// Package:     FWCore/Framework
// Class  :     OutputModuleCommunicator
//
/**\class edm::OutputModuleCommunicator OutputModuleCommunicator.h "FWCore/Framework/interface/OutputModuleCommunicator.h"

 Description: Base class used by the framework to communicate with an OutputModule

 Usage:
    <usage>

*/
//
// Original Author:  Chris Jones
//         Created:  Fri, 05 Jul 2013 17:36:51 GMT
//

// system include files
#include <map>
#include <string>
#include <vector>

// user include files
#include "DataFormats/Provenance/interface/SelectedProducts.h"
#include "FWCore/Common/interface/FWCoreCommonFwd.h"
#include "FWCore/Framework/interface/Frameworkfwd.h"

// forward declarations
namespace edm {

  class ActivityRegistry;
  class MergeableRunProductMetadata;
  class ProcessContext;
  class ThinnedAssociationsHelper;
  class WaitingTaskHolder;

  class OutputModuleCommunicator {
  public:
    OutputModuleCommunicator() = default;
    OutputModuleCommunicator(const OutputModuleCommunicator&) = delete;
    OutputModuleCommunicator& operator=(const OutputModuleCommunicator&) = delete;
    virtual ~OutputModuleCommunicator();

    virtual void closeFile() = 0;

    ///\return true if output module wishes to close its file
    virtual bool shouldWeCloseFile() const = 0;

    ///\return true if no event filtering is applied to OutputModule
    virtual bool wantAllEvents() const = 0;

    virtual void openFile(FileBlock const& fb) = 0;

    virtual void writeProcessBlockAsync(WaitingTaskHolder iTask,
                                        ProcessBlockPrincipal const&,
                                        ProcessContext const*,
                                        ActivityRegistry*) noexcept = 0;

    virtual void writeRunAsync(WaitingTaskHolder iTask,
                               RunPrincipal const&,
                               ProcessContext const*,
                               ActivityRegistry*,
                               MergeableRunProductMetadata const*) noexcept = 0;

    virtual void writeLumiAsync(WaitingTaskHolder iTask,
                                LuminosityBlockPrincipal const&,
                                ProcessContext const*,
                                ActivityRegistry*) noexcept = 0;

    ///\return true if OutputModule has reached its limit on maximum number of events it wants to see
    virtual bool limitReached() const = 0;

    virtual void configure(OutputModuleDescription const& desc) = 0;

    virtual SelectedProductsForBranchType const& keptProducts() const = 0;

    virtual void selectProducts(ProductRegistry const& preg,
                                ThinnedAssociationsHelper const&,
                                ProcessBlockHelperBase const&) = 0;

    virtual void setEventSelectionInfo(
        std::map<std::string, std::vector<std::pair<std::string, int> > > const& outputModulePathPositions,
        bool anyProductProduced) = 0;

    virtual ModuleDescription const& description() const = 0;

  private:
    // ---------- member data --------------------------------
  };
}  // namespace edm

#endif
