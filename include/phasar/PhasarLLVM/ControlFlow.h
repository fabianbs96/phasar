/******************************************************************************
 * Copyright (c) 2023 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_CONTROLFLOW_H
#define PHASAR_PHASARLLVM_CONTROLFLOW_H

#include "phasar/PhasarLLVM/ControlFlow/EntryFunctionUtils.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedBackwardCFG.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedBackwardICFG.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCFG.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCallGraphBuilder.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/CHAResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/DefaultResolverPipeline.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/DirectCallResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/NOResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/OTFResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/RTAResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/Resolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/ResolverBase.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/SoundyFallbackResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/SparseLLVMBasedCFG.h"
#include "phasar/PhasarLLVM/ControlFlow/SparseLLVMBasedCFGProvider.h"
#include "phasar/PhasarLLVM/ControlFlow/SparseLLVMBasedICFG.h"
#include "phasar/PhasarLLVM/ControlFlow/SparseLLVMBasedICFGView.h"

#endif // PHASAR_PHASARLLVM_CONTROLFLOW_H
