# Shared-memory parallel algorithm for the facility location problem

## 1. Problem formulation

### 1.1 Notation

| Symbol | Meaning | Program variable |
|---|---|---|
| N | number of demand points (towns) | `numDP` |
| m | number of preexisting facilities | `numPF` |
| n | number of candidate locations | `numCL` |
| k | number of new facilities | `numX` |
| wᵢ | population of town i | `demandPoints[i][2]` |
| d(i, j) | Haversine distance between towns i and j, km | `HaversineDistance(i, j)` |

Towns are indexed 0, 1, …, N−1 in decreasing order of population.
- The preexisting facilities are located in towns PF = {0, 1, …, m−1}.
- The candidate locations are the towns CL = {0, 1, …, n−1}.

A **solution** is a k-element subset of CL. It is stored as a strictly increasing vector

  X = (x₀, x₁, …, x_{k−1}),  0 ≤ x₀ < x₁ < … < x_{k−1} ≤ n−1.

The set of all feasible solutions 𝒳 contains |𝒳| = C(n, k) elements, where C(n, k) is the binomial coefficient.

### 1.2 Objective function

For a town i and a solution X, define:

- D_PF(i) = ⌊ min_{j ∈ PF} d(i, j) ⌋, the distance to the nearest preexisting facility;
- D_X(i) = ⌊ min_{j ∈ X} d(i, j) ⌋, the distance to the nearest new facility.

The floor ⌊·⌋ appears because the reference implementation stores both minima in `int` variables. This rounds distances down to whole kilometres. The parallel algorithm keeps this behaviour unchanged so that its results stay identical to the reference program.

The market share captured by solution X is

  U(X) = 100 · ( Σᵢ wᵢ · s(i, X) ) / ( Σᵢ wᵢ ),

where

  s(i, X) = 1 if D_X(i) < D_PF(i); 0.3 if D_X(i) = D_PF(i); 0 otherwise.

The task is to find

  X* = argmax_{X ∈ 𝒳} U(X).

If several solutions attain the maximum, the reference program returns the lexicographically smallest of them (see §2.3). We define X* to be exactly that solution, so that the parallel and sequential results can be compared directly.

## 2. Sequential algorithm

### 2.1 Distance matrix

Every distance is computed once. Since d(i, j) = d(j, i), only the lower triangle (j ≤ i) is stored. Row i holds i+1 values, so N(N+1)/2 Haversine evaluations are needed in total.

Time: Θ(N²). Memory: Θ(N²).

### 2.2 Complete enumeration

```
X ← (0, 1, …, k−1)                     // lexicographically first solution
bestX ← X;  bestU ← U(X)
while increaseX(X) succeeds:           // X ← lexicographic successor of X
    u ← U(X)
    if u > bestU:  bestU ← u;  bestX ← X
return bestX, bestU
```

One evaluation of U costs Θ(N·(m + k)) table lookups, so the total time of the search is

  T_search = Θ( C(n, k) · N · (m + k) ).

### 2.3 Properties of `increaseX` and of the sequential result

**Lemma 1 (successor).** `increaseX` transforms X into its immediate successor in the lexicographic order of 𝒳. It returns 0 if and only if X = (n−k, …, n−1), the last solution.

*Proof.* In a feasible solution, position i can hold at most the value n−k+i, because the k−1−i positions after it need distinct larger values.

The lexicographic successor is obtained as follows:
1. Find the rightmost position i with xᵢ < n−k+i.
2. Increase xᵢ by one.
3. Set every later position to the smallest feasible values, x_{j} = x_{j−1}+1 for j > i.

`increaseX(X, index, n)` implements exactly these steps:
- If x_index + 1 < n − (k − index − 1), position `index` can still grow. The function increments it and stops.
- Otherwise it recurses to position `index−1`. If that call succeeds, it resets x_index = x_{index−1}+1.
- If position 0 is also at its maximum n−k, no position can be increased, and the function returns 0.

Starting from position k−1 and moving left, the first position that can grow is the rightmost such position, which is the successor rule above. ∎

**Corollary 1.** Starting from (0, 1, …, k−1), the loop of §2.2 visits every element of 𝒳 exactly once, in increasing lexicographic order.

**Lemma 2 (tie-breaking of the sequential algorithm).** The sequential algorithm returns X* as defined in §1.2: the lexicographically smallest maximiser of U.

*Proof.* The solutions are visited in increasing lexicographic order (Corollary 1). `bestX` is replaced only on a strict improvement (u > bestU). Therefore, once the first maximiser in this order has been stored, no later solution with the same value can replace it. ∎

## 3. Parallel algorithm

### 3.1 Sequential and parallel parts

Following the task statement:
- Reading the data and computing the distance matrix form the **sequential part**.
- The enumeration of §2.2 forms the **parallelised part**. It is implemented in the function `parallelSearch()` using OpenMP.

### 3.2 Decomposition: groups by the first element

For f = 0, 1, …, n−k define the group

  G_f = { X ∈ 𝒳 : x₀ = f }.

**Lemma 3 (partition).** The groups G₀, …, G_{n−k} are pairwise disjoint, their union is 𝒳, and

  |G_f| = C(n−1−f, k−1).

*Proof.*
- **Disjoint and covering:** every solution has exactly one first element x₀. That element satisfies 0 ≤ x₀ ≤ n−k, because the k−1 larger elements must also fit in CL.
- **Size:** fixing x₀ = f, the remaining k−1 elements are an arbitrary (k−1)-subset of {f+1, …, n−1}, a set with n−1−f elements. ∎

**Consistency check (hockey-stick identity).** Summing the group sizes gives the total number of solutions:

  Σ_{f=0}^{n−k} C(n−1−f, k−1) = Σ_{r=k−1}^{n−1} C(r, k−1) = C(n, k) = |𝒳|.

**Lemma 4 (each group is a contiguous interval of the lexicographic order).** In lexicographic order, the elements of G_f form one contiguous block:
- it starts with (f, f+1, …, f+k−1);
- it ends with (f, n−k+1, …, n−1);
- the successor of its last element is (f+1, f+2, …, f+k) when f < n−k; when f = n−k, there is no successor.

*Proof.* Lexicographic order compares the first elements first. All solutions with x₀ = f therefore come after all solutions with x₀ < f and before all solutions with x₀ > f.
- **First element:** the smallest member of G_f has the smallest possible tail, f+1, …, f+k−1.
- **Last element:** the largest member has the largest possible tail, n−k+1, …, n−1.
- **Successor:** by Lemma 1, the successor of (f, n−k+1, …, n−1) has every position from 1 onwards at its maximum. Position 0 is therefore incremented and the tail is reset, which gives (f+1, f+2, …, f+k). For f = n−k this element is the last solution, and `increaseX` returns 0. ∎

### 3.3 Traversal of one group

A thread processes group f with the following loop:

```
X ← (f, f+1, …, f+k−1)
repeat
    evaluate X
until not increaseX(X)  or  x₀ ≠ f
```

**Lemma 5.** This loop evaluates exactly the elements of G_f, each once, in increasing lexicographic order.

*Proof.* The loop starts at the first element of G_f (Lemma 4). Each call to `increaseX` moves to the lexicographic successor (Lemma 1), so the loop walks through the contiguous block of G_f in order.
- For f < n−k: after the last element of G_f, the successor has x₀ = f+1 ≠ f, and the loop stops.
- For f = n−k: `increaseX` returns 0 after the only element of the group, and the loop stops.

Neither stop condition can fire earlier, because every element of the block before the last one has a successor with x₀ = f. ∎

### 3.4 Parallel algorithm

```
parallelSearch():
    bestU ← −1                                            // shared
    parallel region (p threads):
        localX, localBestX : private arrays of size k
        localBestU ← −1
        for f = 0 … n−k  with dynamic scheduling:         // omp for schedule(dynamic)
            traverse G_f (§3.3); for every evaluated X with u = U(X):
                if u > localBestU  or  (u = localBestU and X <_lex localBestX):
                    localBestU ← u;  localBestX ← X
        critical section:                                  // omp critical
            if localBestU > bestU  or  (localBestU = bestU and localBestX <_lex bestX):
                bestU ← localBestU;  bestX ← localBestX
```

Here <_lex denotes lexicographic comparison (`std::lexicographical_compare`).

The OpenMP implementation maps onto this pseudocode as follows:
- **`#pragma omp parallel`** starts a team of p threads (p = `OMP_NUM_THREADS`).
- **`#pragma omp for schedule(dynamic)`** hands out the n−k+1 iterations of the loop over f one at a time. A thread receives the next undistributed group as soon as it finishes the previous one.
- **`#pragma omp critical`** lets only one thread at a time merge its result into the shared `bestX` and `bestU`.

### 3.5 Absence of data races

The data in the parallel region falls into three classes:

| Data | Access in parallel region | Status |
|---|---|---|
| `distanceMatrix`, `demandPoints`, `numDP`, `numPF`, `numCL`, `numX` | read only | shared, safe |
| `localX`, `localBestX`, `localBestU`, `localU`, loop counter `f` | read/write | private to each thread |
| `bestX`, `bestU` | read/write | shared, accessed **only** inside `omp critical` |

`evaluateSolution` and `increaseX` use only their arguments, local variables and the read-only shared data. Each thread passes its own private array to them. No memory location is therefore written by one thread while another thread accesses it outside a critical section, so the program has no data races.

The global variables `X` and `u` of the reference program are not used by the parallel version. Sharing them would cause races, which is why each thread has its own copies.

### 3.6 Correctness of the parallel algorithm

**Theorem.** `parallelSearch()` returns the same solution X* and the same value U(X*) as the sequential algorithm, for any number of threads p ≥ 1 and any distribution of groups among the threads.

*Proof.*
1. **Every solution is evaluated exactly once.** The `omp for` construct assigns each iteration f to exactly one thread. By Lemma 3 the groups partition 𝒳, and by Lemma 5 each group is traversed completely and without repetition.

2. **Values are thread-independent.** U(X) is computed by the same deterministic sequence of floating-point operations, on the same read-only data, whichever thread evaluates it. Comparisons of U values in different threads are therefore consistent with the sequential program.

3. **Each thread finds the lexicographically smallest maximiser of its own set.** Let S_t be the set of solutions evaluated by thread t. Define the order ≺ on solutions:

  X ≺ Y  ⇔  U(X) > U(Y)  or  ( U(X) = U(Y)  and  X <_lex Y ).

  This is a strict total order on 𝒳, and its minimum is exactly X* from §1.2. The local update rule replaces `localBestX` by X exactly when X ≺ `localBestX`. So after the thread has processed all of S_t, `localBestX` is the ≺-minimum of S_t, whatever the order of processing. All values satisfy U ≥ 0 > −1, so the first evaluated solution always initialises `localBestX`.

4. **The merge yields the global minimum.** The critical section applies the same rule, so after all merges `bestX` is the ≺-minimum of the per-thread minima. The order in which threads enter the critical section does not matter. For a strict total order, the minimum over a union equals the minimum of the minima of its parts:

  min_≺ 𝒳 = min_≺ ( S₁ ∪ … ∪ S_p ) = min_≺ { min_≺ S₁, …, min_≺ S_p }.

  Threads that received no group (possible when p > n−k+1) keep localBestU = −1 and never change the result.

5. **Conclusion.** `bestX` = min_≺ 𝒳 = X*. By Lemma 2, X* is also the result of the sequential algorithm. ∎

### 3.7 Load balancing

All solutions cost the same to evaluate, Θ(N·(m+k)). The work of group f is therefore proportional to its size C(n−1−f, k−1), and group sizes decrease strictly with f. For n = 50, k = 3:
- group 0 contains C(49, 2) = 1176 solutions;
- group 47 contains a single one.

A static equal split of the iterations would therefore give the first thread far more work than the last. Dynamic scheduling avoids this:
- Iterations are handed out in increasing f, which is decreasing order of group size.
- Each group goes to the first thread that becomes free.
- This is the classical *Longest Processing Time first* (LPT) list-scheduling rule.

**Upper bound on the search time.** Let:
- W = C(n, k), the total work in evaluations;
- t_max = C(n−1, k−1), the largest group;
- T_p, the parallel search time in evaluations.

Consider the group that finishes last, with size t_j. Until it starts, all p threads are busy, so it starts no later than (W − t_j)/p. Therefore

  T_p ≤ (W − t_j)/p + t_j = W/p + (1 − 1/p)·t_j ≤ W/p + (1 − 1/p)·t_max.

This gives a guaranteed lower bound on the parallel efficiency of the search phase, E_p = (W/p)/T_p. The bound ignores memory and scheduling overheads.

The table compares that guaranteed bound with the efficiency obtained by simulating LPT scheduling of the actual group sizes with equal-cost evaluations:

| n | k | groups | W = C(n,k) | t_max | E₂ guaranteed / simulated | E₄ guaranteed / simulated | E₈ guaranteed / simulated |
|---|---|---|---|---|---|---|---|
| 50 | 3 | 48 | 19 600 | 1 176 | ≥ 94.3 % / 100 % | ≥ 84.7 % / 99.9 % | ≥ 70.4 % / 99.0 % |
| 100 | 3 | 98 | 161 700 | 4 851 | ≥ 97.1 % / 100 % | ≥ 91.7 % / 100 % | ≥ 82.6 % / 99.9 % |
| 150 | 3 | 148 | 551 300 | 11 026 | ≥ 98.0 % / 100 % | ≥ 94.3 % / 100 % | ≥ 87.7 % / 100 % |

The decomposition therefore leaves almost no idle time for p = 2 and p = 4. The remaining deviation from linear speedup in measurements comes from hardware effects such as shared memory bandwidth, cache contention and turbo frequency changes, and from the sequential part (§4).

### 3.8 Complexity and overheads

- **Work:** Θ(C(n, k) · N · (m + k)), the same as the sequential algorithm. The parallel algorithm performs no redundant evaluations.
- **Additional work per group:** Θ(k) to initialise X. Over the whole run this adds Θ((n−k+1)·k), which is negligible.
- **Synchronisation:**
  - the dynamic scheduler performs n−k+1 atomic iteration hand-outs;
  - each thread enters the critical section once, so there are p critical sections in total;
  - there is one implicit barrier at the end of the parallel region.
  
  Synchronisation is O(n + p) operations, independent of the number of solutions.
- **Extra memory:** 2k integers per thread.
- **Parallel time of the search:** T_search(p) ≈ T_search(1)/p + O((1−1/p)·t_max·c_eval), where c_eval is the time of one evaluation of U (see §3.7).

## 4. Theoretical speedup (Amdahl's law)

Let T_seq be the measured time of the sequential part (data loading and distance matrix) and T_par the time of the search, both measured with one thread. The parallelisable fraction is

  P = T_par / (T_seq + T_par).

Amdahl's law gives the theoretical speedup of the whole algorithm with p processors:

  S(p) = 1 / ( (1 − P) + P/p ),

and the maximum speedup, reached as p → ∞:

  S_max = lim_{p→∞} S(p) = 1 / (1 − P).

*Example:* T_seq = 4 s and T_par = 20 s give P = 20/24 ≈ 0.833. Then:
- S(2) = 1 / (0.167 + 0.417) ≈ 1.71;
- S(4) = 1 / (0.167 + 0.208) ≈ 2.67;
- S_max = 6.

The measured speedups are computed as S_exp(p) = T(1) / T(p). They are computed separately for the search (T_par) and for the whole algorithm (T_seq + T_par), and compared with the linear speedup S_lin(p) = p.
