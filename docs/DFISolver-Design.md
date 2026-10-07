# DFISolver: Design

Design for a generic `DFISolver` that solves any `psr::IFDSProblem` with the
DFS-interval reachability technique from Hsu, Hetzelt, Franz: "DFI: An
Interprocedural Value-Flow Analysis Framework that Scales to Large Codebases"
(CGO'23, arXiv:2209.02638). The MLIR part of the paper (`dfi.store`,
`dfi.call`) is out of scope; that is the job of the analysis problem (and
optionally a sparse ICFG).

## 1. What we take from DFI, and what we change

### 1.1 DFI in a nutshell

1. Value-flow is graph reachability on a sparse graph whose edges come from
   local transfer functions provided by the client.
2. A DFS from a set of client-chosen roots ("reversed roots") assigns each
   vertex a DFS-tree interval `<s, e>`. Tree reachability = interval
   subsumption, O(1).
3. Non-tree edges are handled by attaching an *interval set* `Pi_v` to each
   vertex: for a cross edge `s -> d`, `Pi_d` is merged into `s` and all its
   DFS ancestors; for a back edge, all vertices of the SCC get the same set.
   Adjacent intervals are coalesced.
4. Interprocedurally, a function's value-flow summary (arguments -> results)
   is computed with the intra-procedural index and propagated into all callers
   (Algorithm 1, worklist until fixpoint). Callers then contain summary edges.
5. Traversing in the reversed direction reduces the number of non-tree edges
   for SSA def-use graphs (non-tree edges ~ in-degree in traversal direction).

### 1.2 Adaptations for generic IFDS

| DFI paper | DFISolver |
|---|---|
| Vertex = SSA value | Vertex = exploded-supergraph (ESG) node `<n, d>` |
| Local transfer functions | `getNormal/Call/Ret/CallToRet/SummaryFlowFunction` |
| Graph built from the whole IR upfront | Graph is *discovered* on demand from the seeds (flow functions are forward-only and `d_t` is not enumerable) |
| Reversed roots chosen by client | `initialSeeds()` (+ unbalanced-return roots) |
| Timestamps `<s, e>` with two ticks per vertex | Pre-order *rank* + subtree end `[Lo, Hi)`, one tick per *target* vertex (see 1.3) |
| Query `Pi_j "subsumes" Pi_i` (set vs. set) | Point query `rank(y) in Pi_x` (exact, see 1.3) |
| Cross/back edges merged into ancestors eagerly | Tarjan SCC fold: sets computed once per SCC at SCC completion |
| `S_f` summaries over argument/result indices | IFDS end summaries: pairs `(<sp, d1>, <ep, d2>)` |
| Reachable-function summary `Psi` for cross-function queries | One global index over the graph with call + summary edges (exact) |

### 1.3 Two corrections/improvements of the index

**Exact point query.** The paper declares `v_i ~> v_j` iff some interval of
`Pi_j` subsumes some interval of `Pi_i`. This yields false positives: if
`x -> z` and `y -> z` (cross edges into an already finished `z`), both sets
contain `I_z`, so the test claims `x ~> y`. We instead test whether the
*own rank* of the target lies in the source's set:

```
x ~> y   <=>   rank(y) in Pi_x        (Pi_x = union of disjoint, sorted intervals)
```

Every interval in `Pi_x` is the DFS-subtree range of some vertex `w` with
`x ~> w`; every vertex in that range is reachable from `w`. Hence exact.
Cost: O(1) for the own range, otherwise O(log |Pi_x|) binary search (the paper
reports O(N^2) for its set test).

**Projected ranks.** The rank counter is incremented only for *target*
vertices (a predicate). A subtree of the DFS still covers a contiguous rank
range of the targets it contains. With few targets (e.g. function exits when
computing summaries), interval sets collapse to 0-2 intervals almost always,
and subtrees without targets get an empty range and contribute nothing. With
"all vertices are targets", ranks are plain pre-order numbers.

## 2. Graph model

For a given IFDS problem, the solver materializes the realizable part of the
ESG:

- **Vertices**: `<n, d>` reachable from a seed via a realizable path. Exactly
  the vertices whose facts IFDS reports, so `resultsAt(n)` = all vertices at
  `n` (no index needed for the standard IFDS results).
- **Local edges** (intra-procedural, in function `f`):
  - normal edges: `<n, d> -> <succ, d'>`, `d' in normalFF(n, succ)(d)`
  - call-to-return edges: `<c, d> -> <r, d'>`
  - summary-FF edges: `<c, d> -> <r, d'>` from `getSummaryFlowFunction`
  - summary edges: `<c, d> -> <r, d3>` iff `d1 in callFF(d)`,
    `<sp, d1> ~>_same-level <ep, d2>`, `d3 in retFF(d2)`
- **Call edges**: `<c, d> -> <sp_g, d1>`. Not used for summaries.
- **Return edges** are never stored; they are represented by summary edges.

Let `G*` = vertices + local edges + call edges. Reachability in `G*` from `x`
is exactly realizable reachability along paths that may descend into callees
but never return beyond `x`'s frame (same-level + descending). This is the
semantics of the query API. Unbalanced returns (Sec. 4.6) add explicit
unbalanced-return edges.

## 3. Algorithm overview

```
solve():
  init:     compress seeds, compute call-graph SCCs (callees first)
  Phase 1:  repeat until no work:
              F := pending function with the lowest CG-SCC id (callees first)
              if F has unexplored vertices: explore(F)
              elif F is dirty and its SCC + all lower SCCs are quiescent: summarize(F)
  Phase 2:  build result table (InstId -> facts)
            optionally build global interval index over G*
            release Phase 1 graph (unless KeepGraph)
```

Each ESG vertex is expanded (flow functions evaluated) **exactly once**. This
is the main difference to IFDS tabulation, which processes a vertex once per
reaching entry fact `d1` (path edges `<d1, n, d2>`).

### 3.1 explore(f)

The local worklist of `f` holds unexpanded vertices. Expansion is batched per
instruction: pop all pending vertices, sort by `InstId` (radix sort on u64
`Inst << 32 | Vtx`), and process each run so every flow-function object is
created once per `(n, succ)` per batch and applied to all facts. No
flow-function cache is needed.

```
expand(v = <n, d> in f):
  if isCallSite(n):
    Callees := getCalleesOfCallAt(n)
    for r in getReturnSitesOfCallAt(n):
      for d' in callToRetFF(n, r, Callees)(d):  addLocalEdge(v, <r, d'>)
    for g in Callees:
      if SFF := getSummaryFlowFunction(n, g):
        for r, d' in SFF(d) x ReturnSites:        addLocalEdge(v, <r, d'>)
        continue
      for d1 in callFF(n, g)(d), sp in getStartPointsOf(g):
        E := getOrCreateVertex(<sp, d1>, g)       // new -> push to g's WL, mark entry
        addCallEdge(v, E)                          // also registers v as caller of E
        for X in SummaryExits[E]: applySummary(v, E, X)
  else:
    for succ in getSuccsOf(n):
      for d' in normalFF(n, succ)(d):            addLocalEdge(v, <succ, d'>)
  (AutoAddZero: zero always maps to zero)

applySummary(v = <c, d>, E, X = <ep, d2>):
  for r in getReturnSitesOfCallAt(c):
    for d3 in retFF(c, fun(E), ep, r)(d2):        // memoized per (c, X, r)
      addSummaryEdge(v, <r, d3>)                  // new target -> push to caller's WL
  mark caller dirty
```

All edges created by `expand(v)` are produced in one go, so they are appended
contiguously to the edge array (append-only CSR: per vertex `EdgeBegin`,
`EdgeCount`). Summary edges arrive later and go to a per-function append-only
pair list.

`advanceToNextUser(n, d)` (sparse IFDS, `has_advanceToNextUser_v`) is applied
when creating a target vertex, exactly like `IterativeIDESolver`. This is the
generic analog of DFI's sparse def-use graph.

### 3.2 summarize(f)

Computes all same-level summaries `(entry, exit)` of `f`:

1. Build a transient local CSR over `f`'s vertices using local ids
   (`LocalIdx[v]`, assigned at vertex creation): local edges + summary edges,
   no call edges. Reused scratch buffers, O(|V_f| + |E_f|).
2. Run the interval-index kernel (Sec. 4) with
   roots = entry vertices of `f` (+ seed / unbalanced roots in `f`),
   targets = exit vertices of `f`.
3. For each entry `E`: enumerate exit ranks in `Pi(scc(E))`, map
   `rank -> exit vertex`, diff against the sorted `SummaryExits[E]`.
4. For each new pair `(E, X)` and each caller vertex `v` of `E`:
   `applySummary(v, E, X)`. Append `X` to `SummaryExits[E]`.

The local index is discarded afterwards; only the summary pairs persist.

### 3.3 Scheduling

- Call-graph SCCs via `computeCGSCCs` (Tarjan; SCC id 0 = leaves). Priority =
  SCC id, lowest first, i.e. callees before callers.
- `PendingSCCs: BitSet<SCCId>`; pick via find-first-set. Per SCC a small FIFO
  of functions with work.
- A function is summarized only when its SCC and all lower SCCs have empty
  local worklists. New entries only appear in callees (lower SCC ids or same
  SCC), new summary edges only in callers (higher or same SCC), so callers are
  typically summarized once per "wave". Recursion inside an SCC iterates to a
  fixpoint.

### 3.4 Correctness sketch

- *Soundness w.r.t. IFDS*: every IFDS path edge `<sp, d1> -> <n, d2>` with
  `<sp, d1>` realizable implies `<n, d2>` is materialized: induction over the
  IFDS tabulation; the only non-local step is the return, which is covered by
  `applySummary` once the callee's index contains `(E, X)`. At fixpoint every
  function is summarized after its last change, so all summary pairs exist.
- *Precision*: vertices are only created from realizable vertices
  (normal/c2r/call targets) or via `applySummary` from a realizable call vertex
  and a same-level callee path. Flow functions are therefore never evaluated
  on unrealizable contexts, so side effects in flow functions (e.g. leak
  reporting) see the same `(n, d)` set as with IFDS.
- *Termination*: all sets grow monotonically over finite domains.

## 4. Interval index kernel

Generic, independent of IFDS; lives in `phasar/Utils/IntervalReachability.h`.
Input: any `is_const_graph` (typically `CsrGraph<VtxId>` from
`phasar/Utils/CsrGraph.h`), roots, target predicate. A single DFS computes
SCCs, ranks, and interval sets. It reuses Pearce's SCC algorithm from
`SCCGeneric.h` via `visitSCCsFrom()`, whose `OnDiscover` hook assigns ranks
and whose `OnSCC` hook computes the interval sets.

### 4.1 Per-SCC label

```
Pi(S) = Extras(S) U [Lo(S), Hi(S))
```

- `[Lo, Hi)`: rank range of the DFS subtree of the SCC root (all members are
  in it). `Lo = RankCounter` at root discovery, `Hi = RankCounter` at SCC
  completion.
- `Extras(S)`: sorted, coalesced, disjoint intervals, **all `< Lo(S)`**
  (invariant). When the root finishes, every vertex reachable from it has
  been visited, so no reachable target has rank `>= Hi`; anything inside
  `[Lo, Hi)` is already covered. So extras only ever need clipping at `Lo`.

### 4.2 SCC completion

Tarjan completes SCCs in reverse topological order, so every successor SCC
`W != S` of a member is final when `S` completes.

```
onSccComplete(S, Members):
  Lo, Hi := RankBegin[root], RankCounter
  Cand := []
  for m in Members, w in succ(m):
    W := SccOf[w]
    if W == S or Seen[W] == S: continue    // stamp array dedups
    Seen[W] := S
    if Hi(W) <= Lo and Lo(W) < Hi(W): Cand += [Lo(W), Hi(W))   // cross edge
    Cand += clip(Extras(W), Lo)
  Extras(S) := fastPath or coalesce(sort(Cand))
```

**Fast path** (chains, loop bodies): exactly one distinct successor `W`, `W` is
inside the subtree, and `Extras(W).back().Hi <= Lo` -> reuse `ExtraSetId(W)`
without copying. Otherwise sort + coalesce (`[a, b) [b, c) -> [a, c)`), then
optionally hash-cons the resulting span in the interval pool.

### 4.3 Query

```
reaches(x, y):  S := SccOf[x], R := Rank[y]
  return Lo(S) <= R < Hi(S) || binarySearch(Extras(S), R)
```

Enumeration of everything reachable from `x` = walk `Pi(S)`; with
"all vertices are targets" and vertices renumbered by rank, each interval is a
contiguous slice of the vertex arrays.

### 4.4 Direction

The ESG is explored forward (forced by `computeTargets`). The kernel can index
`G*` or its reverse (`IndexDirection::Forward | Backward`, reverse CSR is one
counting sort). Same answers, different `|Pi|` and different enumeration
locality: forward makes "what does `x` reach" contiguous, backward makes
"what reaches `y`" (provenance) contiguous. The paper's in-degree argument is
specific to SSA def-use graphs; for ESGs (merge points vs. branch/gen
fan-out) this must be measured. Default: Forward.

## 5. Data layout

All ids are `uint32_t` strong typedefs (`PHASAR_STRONG_TYPEDEF`), containers
are `TypedVector` / `BitSet`. `InstId` comes from
`IRDB.getInstructionId(n)` (no hashing), `FactId` from a `Compressor<d_t>`
(zero fact = 0), `FunctionId` from `FunctionCompressor<f_t>`.

### 5.1 Phase 1 (mutable, append-only)

| Data | Type | Bytes/vertex | Notes |
|---|---|---|---|
| `VertexOf` | flat hash map `u64(Inst, Fact) -> VertexId` | ~12-16 | the only hash probe per edge target |
| `VtxInst`, `VtxFact` | `TypedVector<VertexId, u32>` x2 | 8 | SoA |
| `VtxEdgeBegin`, `VtxEdgeCount` | `TypedVector<VertexId, u32>` x2 | 8 | into `Edges` |
| `LocalIdx` | `TypedVector<VertexId, u32>` | 4 | index within `FunVertices[f]` |
| `Edges` | `std::vector<VertexId>` | ~4 x out-degree | normal + c2r + summary-FF |
| `FunVertices[f]` | `TypedVector<FunctionId, std::vector<VertexId>>` | 4 | |
| `FunSummaryEdges[f]` | vector of `(VertexId, VertexId)` | | turned into transient CSR in `summarize` |
| `FunEntries[f]`, `FunExits[f]` | vectors of `VertexId` | | exits = vertices at `isExitInst` |
| `EntryIdx` | `DenseMap<VertexId, EntryId>` (entries are rare) | | |
| `EntryCallers[E]`, `SummaryExits[E]` | `TypedVector<EntryId, SmallVector<VertexId, 2>>` | | `SummaryExits` kept sorted |
| `CallEdges` | vector of `(VertexId, VertexId)` | | for the global index only |
| `RetFFMemo` | map `(InstId c, VertexId X) -> span in target pool` | | avoids repeated `retFF` on the same exit fact |
| `FunWL[f]` | `std::vector<VertexId>` | | |
| `PendingSCCs`, `Dirty` | `BitSet` | | |

Per vertex ~36-40 bytes + edges. For comparison, `IterativeIDESolver` stores
one jump-function entry per path edge `<d1, n, d2>` (hash-table slot of
`combineIds(d1, d2)` per instruction + ValTab entry); path edges per vertex =
number of entry facts reaching it, which is what blows up on large programs.

### 5.2 Kernel scratch (reused across all `summarize` calls)

`Index[], Low[], RankBegin[], SccOf[]` (u32, sized to the largest local
graph), `OnStack` bit vector, Tarjan stack, DFS frame stack `{Vtx, EdgeCursor}`,
`Seen[]` stamps, candidate interval buffer. No allocation in steady state.

### 5.3 Final results (immutable)

| Data | Type | Bytes |
|---|---|---|
| `VtxInst`, `VtxFact` (renumbered by rank) | `u32` x2 | 8 / vertex |
| `InstOffsets` | CSR offsets | 4 / instruction |
| `VerticesAt` | ranks, sorted by `FactId` per instruction | 4 / vertex |
| `FactCompressor` | `d_t <-> FactId` | |
| *index:* `SccOf` | `u32` | 4 / vertex |
| *index:* `SccLo`, `SccHi`, `SccExtras` | `u32` x3 | 12 / SCC |
| *index:* `IntervalPool`, `SetOffsets` | CSR of `{u32 Lo, Hi}` | 8 / interval |

Phase 1 structures (hash map, edges, summaries) are released after Phase 2
unless `KeepGraph` is set.

## 6. API

```cpp
namespace psr {

enum class DfiIndexDirection : uint8_t { Forward, Backward };
enum class DfiQueryTargets : uint8_t { All, Interesting };

struct DFISolverConfig {
  static constexpr bool AutoAddZero = true;
  static constexpr bool BuildQueryIndex = true;
  static constexpr DfiIndexDirection IndexDirection = DfiIndexDirection::Forward;
  /// Interesting: rank only vertices at Problem.isInteresting(n) instructions
  static constexpr DfiQueryTargets QueryTargets = DfiQueryTargets::All;
  /// Keep the CSR of G* after solving (ESG export, witness paths)
  static constexpr bool KeepGraph = false;
  static constexpr bool HashConsIntervalSets = true;
  static constexpr bool EnableStatistics = false;

  template <typename K, typename V> using map_t = ...;
};

template <IFDSProblem ProblemTy, typename StaticSolverConfigTy = DFISolverConfig,
          ICFG ICFGTy = typename ProblemTy::ProblemAnalysisDomain::i_t>
class DFISolver
    : public IDESolverAPIMixin<DFISolver<ProblemTy, StaticSolverConfigTy, ICFGTy>> {
public:
  DFISolver(ProblemTy *Problem, const ICFGTy *ICF,
            StaticSolverConfigTy Config = {}) noexcept;

  auto solve() &;
  [[nodiscard]] auto solve() &&;

  /// Trivially-copyable view; usable as GenericSolverResults<n_t, d_t, BinaryDomain>
  [[nodiscard]] DFISolverResults<n_t, d_t> getSolverResults() const noexcept;
  [[nodiscard]] OwningDFISolverResults<n_t, d_t> consumeSolverResults();

  [[nodiscard]] DFIReachability<n_t, d_t> getReachability() const noexcept
    requires(StaticSolverConfigTy::BuildQueryIndex);

  [[nodiscard]] DFISolverStats getStats() const noexcept
    requires(StaticSolverConfigTy::EnableStatistics);

private:
  // IDESolverAPIMixin hooks: one expansion batch or one summarize per step
  void doInitialize();
  bool doNext();
  auto doFinalize() &;
  auto doFinalize() &&;
};

template <IFDSProblem ProblemTy>
[[nodiscard]] auto solveDFIProblem(ProblemTy &Problem, const ICFG auto &ICF);

/// Context-sensitive reachability between materialized ESG nodes.
/// Semantics: realizable paths that may descend into callees but do not
/// return past the frame of the source (plus unbalanced returns from seeds).
template <typename N, typename D> class DFIReachability {
public:
  [[nodiscard]] std::optional<DfiVertexRef> find(ByConstRef<N> Inst,
                                                 ByConstRef<D> Fact) const;
  [[nodiscard]] bool reaches(DfiVertexRef From, DfiVertexRef To) const noexcept;
  [[nodiscard]] bool reaches(ByConstRef<N> FromInst, ByConstRef<D> FromFact,
                             ByConstRef<N> ToInst, ByConstRef<D> ToFact) const;

  /// Facts at At that are reachable from From
  void forEachReachableAt(DfiVertexRef From, ByConstRef<N> At,
                          std::invocable<ByConstRef<D>> auto Handler) const;
  void forEachReachable(DfiVertexRef From,
                        std::invocable<ByConstRef<N>, ByConstRef<D>> auto Handler) const;
};

} // namespace psr
```

`DFISolverResults` implements what `GenericSolverResults` dispatches to:
`resultAt`, `resultsAt(n, StripZero)`, `ifdsResultsAt`, `size`,
`containsNode`, `foreachResultEntry`. `resultsAt(n)` is a slice of
`VerticesAt`; `resultAt(n, d)` is a binary search in it.

`forEachReachableAt(From, At)` intersects `Pi(From)` with the rank-sorted
vertices at `At` (merge of two sorted lists), the typical taint query
"which facts at sink `At` originate from source `From`".

## 7. IFDS feature mapping

| Feature | Handling |
|---|---|
| `AutoAddZero` | zero fact always propagated by the solver |
| `getSummaryFlowFunction` | local edges to return sites, callee not entered |
| Callee without body | only call-to-return flow |
| `followReturnsPastSeeds` | seeds (and derived unbalanced roots) are extra roots in `summarize`; exits reached from them return to *all* callers of `f` via `retFF`, creating unbalanced roots in the callers and unbalanced-return edges in `G*`; for functions without callers, `applyUnbalancedRetFlowFunctionSideEffects` if `UnbalancedRetSideEffectProvider` |
| `has_advanceToNextUser_v` | applied on target-vertex creation |
| `isInteresting(n)` | `QueryTargets::Interesting` restricts ranks to those vertices |
| `emitESG` / `recordEdges` | requires `KeepGraph`; dumps the CSR of `G*` |
| `computeValues` | not applicable (IFDS only, `l_t = BinaryDomain`) |
| `IDESolverAPIMixin` (`solveUntil`, cancellation) | `doNext` = one batch / one summarize |

**Witness paths** (with `KeepGraph`): reconstruct a path `x ~> y` greedily by
following any successor `s` with `reaches(s, y)`; no backtracking needed,
O(path length x out-degree x log |Pi|).

## 8. Complexity

- Flow-function applications: once per vertex (IFDS: once per path edge).
- Return flow: once per `(call site, exit vertex, return site)` via memo.
- `summarize(f)`: O(|V_f| + |E_f| + merge cost) per call; number of calls per
  function bounded by the number of change waves, small with callees-first
  scheduling except in large recursive SCCs.
- Global index: O(|V| + |E| + sum |Pi|). Sets are tiny in practice (paper:
  median 2, max 17), further reduced by coalescing ranks and the fast path.
- Worst case `sum |Pi| = O(|V|^2)`, same as any transitive-closure labeling.
  `BuildQueryIndex = false` skips it entirely if only IFDS results are needed.

## 9. Files and tests

New files:

- `include/phasar/Utils/CsrGraph.h`: `CsrGraph<VtxId>` + `GraphTraits`.
- `include/phasar/Utils/IntervalReachability.h`: kernel (Sec. 4), interval
  pool, query. No IFDS dependency. Built on `visitSCCsFrom()` in
  `SCCGeneric.h`.
- `include/phasar/DataFlow/IfdsIde/Solver/DFISolverConfig.h`
- `include/phasar/DataFlow/IfdsIde/Solver/DFISolver.h`
- `include/phasar/DataFlow/IfdsIde/Solver/DFISolverResults.h` (results view,
  owning results, `DFIReachability`)
- `include/phasar/DataFlow/IfdsIde/Solver/DFISolverStats.h`

Tests:

- Kernel: random graphs (DAGs, cycles, dense SCCs, empty target sets, single
  vertex) checked against brute-force BFS transitive closure, both
  directions, all-targets and projected targets.
- Solver: differential tests against `IterativeIDESolver` with the existing
  IFDS analyses (`IFDSTaintAnalysis`, `IFDSUninitializedVariables`,
  `IFDSConstAnalysis`, `IFDSTypeAnalysis`, ...) on the existing
  `test/llvm_test_code` programs: identical `ifdsResultsAt` everywhere,
  identical leak sets. Recursion and `followReturnsPastSeeds` cases included.
- Reachability: for small programs, compare `reaches` against a reference that
  re-runs `IterativeIDESolver` with `<n, d>` as single seed.

## 10. Open points

- **Incremental local index**: `summarize` currently rebuilds the local index
  of `f` from scratch. For large recursive SCCs an incremental variant (only
  new roots / new edges) may pay off. Measure first.
- **Parallelism**: functions in different subtrees of the CG-SCC DAG with
  pending work are independent in `explore`; `summarize` writes only into
  callers. A level-synchronous parallel scheduler is possible later.
- **Index direction and target selection** need empirical evaluation on ESGs
  (Sec. 4.4).
