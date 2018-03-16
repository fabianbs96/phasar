/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#ifndef ICFGPlugin_H_
#define ICFGPlugin_H_

#include "../../../../db/ProjectIRDB.h"
#include "../../../control_flow/ICFG.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <map>
#include <string>
using namespace std;

class ICFGPlugin
    : public ICFG<const llvm::Instruction *, const llvm::Function *> {
private:
  ProjectIRDB &IRDB;
  const vector<string> EntryPoints;

public:
  ICFGPlugin(ProjectIRDB &IRDB, const vector<string> EntryPoints)
      : IRDB(IRDB), EntryPoints(move(EntryPoints)) {}
};

extern map<string, unique_ptr<ICFGPlugin> (*)(ProjectIRDB &,
                                              const vector<string> EntryPoints)>
    ICFGPluginFactory;

#endif