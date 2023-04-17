#include "phasar/PhasarLLVM/Utils/AnalysisPrinter.h"
#include "phasar/Domain/AnalysisDomain.h"
#include <vector>

namespace psr{
AnalysisPrinter::AnalysisPrinter(struct Results Res):Results(std::move(Res)){}

void AnalysisPrinter::getAnalysisResults(struct Results Res){
    Res = Results;
}

void AnalysisPrinter::

}