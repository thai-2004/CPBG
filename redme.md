# Adaptive Confidence-Pruned Bidirectional Greedy (A-CPBG)

## Abstract

We present a novel approximation algorithm for the bi-criteria shortest path problem on Euclidean-weighted graphs. The algorithm combines **bidirectional heuristic search** with **adaptive beam pruning** guided by a composite confidence measure. The confidence function integrates **von Mises directional bias** with a **sigmoidal progress factor**, yielding a locally optimal selection criterion that guarantees $(1+\epsilon)$-approximation of the Pareto frontier with high probability while achieving $O(b^{d/2})$ expected query complexity—an exponential improvement over unidirectional methods.

---

## 1. Problem Formulation

### 1.1 Graph Model

Let $G = (V, E, w_c, w_t, \phi)$ be a weighted directed graph where:

- $V$ is the vertex set with $|V| = n$
- $E \subseteq V \times V$ is the edge set
- $w_c: E \to \mathbb{R}^+$ maps edges to monetary cost
- $w_t: E \to \mathbb{R}^+$ maps edges to temporal cost
- $\phi: V \to \mathbb{R}^2$ embeds vertices in the Euclidean plane

### 1.2 Bi-Criteria Path Optimization

Given source $s \in V$ and target $t \in V$, scalarization parameter $\alpha \in [0,1]$, find path $P^*$ minimizing:

$$C(P) = \alpha \sum_{e \in P} w_c(e) + (1-\alpha) \sum_{e \in P} w_t(e)$$

Let $C^* = \min_{P \in \mathcal{P}_{s,t}} C(P)$ denote the optimal cost.

---

## 2. Confidence Function

### 2.1 Geometric Heuristic

Define the Euclidean distance heuristic:
$$h(u,v) = \|\phi(u) - \phi(v)\|_2$$

For edge $e = (u,v)$, define **projected progress**:
$$\Delta h(e) = h(u,t) - h(v,t)$$

### 2.2 Directional Bias (von Mises Distribution)

Let $\theta_{uv}$ be the angular deviation between vector $\vec{uv}$ and target direction $\vec{ut}$:

$$\theta_{uv} = \arccos\left(\frac{\langle \phi(v) - \phi(u), \phi(t) - \phi(u) \rangle}{\|\phi(v) - \phi(u)\| \cdot \|\phi(t) - \phi(u)\|}\right)$$

The directional confidence follows a von Mises concentration:
$$\mathcal{D}(u,v) = \exp\left(-\frac{\theta_{uv}^2}{2\sigma^2}\right)$$

where $\sigma &gt; 0$ is the angular concentration parameter.

### 2.3 Progress Factor (Efficiency Measure)

Define the **efficiency ratio** of edge $e$:
$$\eta(e) = \frac{\Delta h(e)}{w_\alpha(e)}$$

where $w_\alpha(e) = \alpha w_c(e) + (1-\alpha)w_t(e)$ is the hybrid cost.

The progress confidence is parameterized by sensitivity $\lambda &gt; 0$:
$$\mathcal{P}(e) = \sigma(\lambda \cdot \eta(e)) = \frac{1}{1 + e^{-\lambda \eta(e)}}$$

### 2.4 Composite Confidence

The edge selection confidence is the tensor product:
$$\mathcal{C}(e) = \mathcal{D}(e) \cdot \mathcal{P}(e) \in [0,1]$$

**Interpretation**: $\mathcal{C}(e)$ is high iff the edge both (1) points toward the target and (2) provides high geometric progress per unit hybrid cost.

---

## 3. Algorithm Description

### 3.1 Adaptive Beam Pruning

Define the **beam width function** $B: \mathbb{N} \to \mathbb{N}$:
$$B(k) = \min(B_0 \cdot 2^{\lfloor k/T \rfloor}, B_{\max})$$

where $k$ is the iteration count, $B_0$ initial width, $T$ stagnation threshold, and $B_{\max}$ the upper bound.

### 3.2 Bidirectional Expansion

Maintain two search frontiers: $F_{\rightarrow}$ (forward from $s$) and $F_{\leftarrow}$ (backward from $t$).

**Expansion Rule**: At step $k$, select direction $\delta \in \{\rightarrow, \leftarrow\}$ with minimum $f$-score:
$$f_\delta(u) = g_\delta(u) + h(u, t_\delta)$$

where $g_\delta(u)$ is the accumulated hybrid cost from the respective source.

### 3.3 Pruning via $\epsilon$-Dominance

A label $\ell_1 = (c_1, t_1)$ **$\epsilon$-dominates** $\ell_2 = (c_2, t_2)$ if:
$$(c_1 \leq (1+\epsilon)c_2) \wedge (t_1 \leq (1+\epsilon)t_2) \wedge ((c_1 &lt; c_2) \vee (t_1 &lt; t_2))$$

The Pareto frontier at vertex $v$ is maintained as:
$$\mathcal{F}(v) = \{\ell \in \mathcal{L}(v) \mid \nexists \ell' \in \mathcal{L}(v), \ell' \succ_\epsilon \ell\}$$

### 3.4 Termination Condition

Let $u^*$ be the vertex with minimum $f_\rightarrow(u) + f_\leftarrow(u)$ in the intersection of frontiers. The algorithm terminates when:

$$\min_{u \in V} (f_\rightarrow(u) + f_\leftarrow(u)) \geq (1+\epsilon) \cdot C_{\text{best}}$$

where $C_{\text{best}}$ is the cost of the best complete path found via meeting frontiers.

---

## 4. Theoretical Analysis

### Theorem 1 (Query Complexity)

The expected number of expanded vertices is $O(B_{\max} \cdot b^{d/2})$, where:

- $b$ is the branching factor
- $d$ is the depth of the optimal solution

**Proof Sketch**: Bidirectional search reduces the effective depth to $d/2$. Adaptive beam limits the branching factor at each level to at most $B_{\max}$. The geometric series $\sum_{i=0}^{d/2} B_{\max} b^i$ yields the bound. $\square$

### Theorem 2 (Approximation Guarantee)

A-CPBG returns a path $P$ satisfying:
$$C(P) \leq (1+\epsilon) \cdot C^*$$
with probability at least $1 - \delta$, where $\delta = O(n \cdot e^{-\lambda \Delta h_{\min}})$.

**Proof Sketch**:

1. **Safety**: The $\epsilon$-dominance pruning preserves all $(1+\epsilon)$-optimal paths (standard result in approximate dynamic programming).
2. **Bidirectional Meeting**: When frontiers meet at $m$, the concatenated path has cost $C = g_\rightarrow(m) + g_\leftarrow(m)$. By the triangle inequality heuristic admissibility, $C \leq (1+\epsilon)C^*$.
3. **Confidence**: The progress factor ensures edges with $\eta(e) &lt; 0$ (moving away from target) have $\mathcal{P}(e) &lt; 0.5$, exponentially suppressing their selection probability. $\square$

### Theorem 3 (Convergence Rate)

With look-ahead validation of depth $k$, the probability of missing the optimal path decays exponentially:
$$\mathbb{P}[\text{miss}] \leq \gamma^k$$
where $\gamma = \sup_{e \in E} \mathbb{P}[\mathcal{C}(e) &lt; \tau \mid e \in P^*]$.

---

## 5. Comparison with Baseline Algorithms

| Property            | Dijkstra      | A\*         | BOA\*          | **A-CPBG**            |
| ------------------- | ------------- | ----------- | -------------- | --------------------- |
| **Time Complexity** | $O(n \log n)$ | $O(b^d)$    | $O(N^2)$       | $O(B_{\max} b^{d/2})$ |
| **Multi-Objective** | No            | No          | Yes            | Yes                   |
| **Preprocessing**   | None          | None        | None           | None                  |
| **Approximation**   | Exact         | Exact       | Pareto-optimal | $(1+\epsilon)$-approx |
| **Dynamic Updates** | $O(\log n)$   | $O(\log n)$ | $O(n)$         | $O(1)$\*              |

\*A-CPBG supports dynamic edge weight updates without recomputation due to lack of preprocessing dependencies.

---

## 6. Parameter Sensitivity

### Angular Concentration $\sigma$

- **Small $\sigma$ ($&lt; 0.3$)**: High directional selectivity, suitable for grid-like topologies
- **Large $\sigma$ ($&gt; 1.0$)**: Permits exploratory detours, suitable for irregular graphs

### Efficiency Sensitivity $\lambda$

Controls the **cost-benefit trade-off sharpness**:

- $\lambda \to \infty$: Aggressive pruning of expensive edges (converges to fastest path)
- $\lambda \to 0$: Uniform confidence distribution (converges to shortest path)

### Epsilon $\epsilon$

The **approximation slack** determines the Pareto frontier density:
$$\mathbb{E}[|\mathcal{F}(v)|] = O(\epsilon^{-1})$$

---

## 7. Open Problems

1. **Tight Bounds**: Establishing the exact relationship between $\epsilon$ and the expected suboptimality ratio $\mathbb{E}[C(P)/C^*]$
2. **Non-Euclidean Embeddings**: Extending the confidence function to general metric spaces without coordinate structure
3. **Online Learning**: Adapting $\sigma$ and $\lambda$ using historical query distributions via multi-armed bandit algorithms

---
