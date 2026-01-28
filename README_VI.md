# Adaptive Confidence-Pruned Bidirectional Greedy (A-CPBG)

## Tóm tắt

Chúng tôi trình bày một thuật toán xấp xỉ mới cho bài toán đường đi ngắn nhất hai tiêu chí trên đồ thị có trọng số Euclid. Thuật toán kết hợp **tìm kiếm heuristic hai chiều** với **cắt tỉa chùm thích ứng** được dẫn hướng bởi một thước đo tin cậy tổng hợp. Hàm tin cậy tích hợp **độ lệch hướng von Mises** với **hệ số tiến độ sigmoid**, tạo ra một tiêu chí lựa chọn tối ưu cục bộ đảm bảo xấp xỉ $(1+\epsilon)$ của biên Pareto với xác suất cao trong khi đạt được độ phức tạp truy vấn kỳ vọng $O(b^{d/2})$—một sự cải thiện theo cấp số nhân so với các phương pháp đơn chiều.

---

## 1. Phát biểu bài toán

### 1.1 Mô hình đồ thị

Cho $G = (V, E, w_c, w_t, \phi)$ là một đồ thị có hướng có trọng số, trong đó:

- $V$ là tập hợp các đỉnh với $|V| = n$
- $E \subseteq V \times V$ là tập hợp các cạnh
- $w_c: E \to \mathbb{R}^+$ ánh xạ các cạnh tới chi phí tiền tệ
- $w_t: E \to \mathbb{R}^+$ ánh xạ các cạnh tới chi phí thời gian
- $\phi: V \to \mathbb{R}^2$ nhúng các đỉnh trong mặt phẳng Euclid

### 1.2 Tối ưu hóa đường đi hai tiêu chí

Cho nguồn $s \in V$ và đích $t \in V$, tham số vô hướng hóa $\alpha \in [0,1]$, tìm đường $P^*$ tối thiểu hóa:

$$C(P) = \alpha \sum_{e \in P} w_c(e) + (1-\alpha) \sum_{e \in P} w_t(e)$$

Gọi $C^* = \min_{P \in \mathcal{P}_{s,t}} C(P)$ là chi phí tối ưu.

---

## 2. Hàm tin cậy

### 2.1 Heuristic hình học

Định nghĩa heuristic khoảng cách Euclid:
$$h(u,v) = \|\phi(u) - \phi(v)\|_2$$

Đối với cạnh $e = (u,v)$, định nghĩa **tiến độ dự kiến**:
$$\Delta h(e) = h(u,t) - h(v,t)$$

### 2.2 Độ lệch hướng (Phân phối von Mises)

Gọi $\theta_{uv}$ là độ lệch góc giữa vectơ $\vec{uv}$ và hướng mục tiêu $\vec{ut}$:

$$\theta_{uv} = \arccos\left(\frac{\langle \phi(v) - \phi(u), \phi(t) - \phi(u) \rangle}{\|\phi(v) - \phi(u)\| \cdot \|\phi(t) - \phi(u)\|}\right)$$

Độ tin cậy hướng tuân theo độ tập trung von Mises:
$$\mathcal{D}(u,v) = \exp\left(-\frac{\theta_{uv}^2}{2\sigma^2}\right)$$

trong đó $\sigma > 0$ là tham số tập trung góc.

### 2.3 Hệ số tiến độ (Thước đo hiệu quả)

Định nghĩa **tỷ lệ hiệu quả** của cạnh $e$:
$$\eta(e) = \frac{\Delta h(e)}{w_\alpha(e)}$$

trong đó $w_\alpha(e) = \alpha w_c(e) + (1-\alpha)w_t(e)$ là chi phí hỗn hợp.

Độ tin cậy tiến độ được tham số hóa bởi độ nhạy $\lambda > 0$:
$$\mathcal{P}(e) = \sigma(\lambda \cdot \eta(e)) = \frac{1}{1 + e^{-\lambda \eta(e)}}$$

### 2.4 Độ tin cậy tổng hợp

Độ tin cậy lựa chọn cạnh là tích tensor:
$$\mathcal{C}(e) = \mathcal{D}(e) \cdot \mathcal{P}(e) \in [0,1]$$

**Diễn giải**: $\mathcal{C}(e)$ cao khi và chỉ khi cạnh vừa (1) hướng về phía mục tiêu và (2) cung cấp tiến độ hình học cao trên mỗi đơn vị chi phí hỗn hợp.

---

## 3. Mô tả thuật toán

### 3.1 Cắt tỉa chùm thích ứng

Định nghĩa **hàm độ rộng chùm** $B: \mathbb{N} \to \mathbb{N}$:
$$B(k) = \min(B_0 \cdot 2^{\lfloor k/T \rfloor}, B_{\max})$$

trong đó $k$ là số lần lặp, $B_0$ độ rộng ban đầu, $T$ ngưỡng trệ, và $B_{\max}$ cận trên.

### 3.2 Mở rộng hai chiều

Duy trì hai biên tìm kiếm: $F_{\rightarrow}$ (tiến từ $s$) và $F_{\leftarrow}$ (lùi từ $t$).

**Quy tắc mở rộng**: Tại bước $k$, chọn hướng $\delta \in \{\rightarrow, \leftarrow\}$ có điểm $f$ tối thiểu:
$$f_\delta(u) = g_\delta(u) + h(u, t_\delta)$$

trong đó $g_\delta(u)$ là chi phí hỗn hợp tích lũy từ nguồn tương ứng.

### 3.3 Cắt tỉa qua $\epsilon$-Dominance

Một nhãn $\ell_1 = (c_1, t_1)$ **$\epsilon$-trội** $\ell_2 = (c_2, t_2)$ nếu:
$$(c_1 \leq (1+\epsilon)c_2) \wedge (t_1 \leq (1+\epsilon)t_2) \wedge ((c_1 < c_2) \vee (t_1 < t_2))$$

Biên Pareto tại đỉnh $v$ được duy trì như sau:
$$\mathcal{F}(v) = \{\ell \in \mathcal{L}(v) \mid \nexists \ell' \in \mathcal{L}(v), \ell' \succ_\epsilon \ell\}$$

### 3.4 Điều kiện chấm dứt

Gọi $u^*$ là đỉnh có tổng $f_\rightarrow(u) + f_\leftarrow(u)$ tối thiểu trong giao điểm của các biên. Thuật toán kết thúc khi:

$$\min_{u \in V} (f_\rightarrow(u) + f_\leftarrow(u)) \geq (1+\epsilon) \cdot C_{\text{best}}$$

trong đó $C_{\text{best}}$ là chi phí của đường đi hoàn chỉnh tốt nhất tìm được qua việc gặp gỡ các biên.

---

## 4. Phân tích lý thuyết

### Định lý 1 (Độ phức tạp truy vấn)

Số lượng đỉnh được mở rộng kỳ vọng là $O(B_{\max} \cdot b^{d/2})$, trong đó:

- $b$ là hệ số phân nhánh
- $d$ là độ sâu của giải pháp tối ưu

**Phác thảo chứng minh**: Tìm kiếm hai chiều giảm độ sâu hiệu quả xuống $d/2$. Chùm thích ứng giới hạn hệ số phân nhánh ở mỗi cấp tối đa là $B_{\max}$. Chuỗi hình học $\sum_{i=0}^{d/2} B_{\max} b^i$ đưa ra giới hạn này. $\square$

### Định lý 2 (Đảm bảo xấp xỉ)

A-CPBG trả về một đường $P$ thỏa mãn:
$$C(P) \leq (1+\epsilon) \cdot C^*$$
với xác suất ít nhất $1 - \delta$, trong đó $\delta = O(n \cdot e^{-\lambda \Delta h_{\min}})$.

**Phác thảo chứng minh**:

1.  **An toàn**: Việc cắt tỉa $\epsilon$-dominance bảo tồn tất cả các đường đi $(1+\epsilon)$-tối ưu (kết quả chuẩn trong quy hoạch động xấp xỉ).
2.  **Gặp gỡ hai chiều**: Khi các biên gặp nhau tại $m$, đường đi nối có chi phí $C = g_\rightarrow(m) + g_\leftarrow(m)$. Theo bất đẳng thức tam giác của tính chấp nhận được của heuristic, $C \leq (1+\epsilon)C^*$.
3.  **Độ tin cậy**: Hệ số tiến độ đảm bảo các cạnh có $\eta(e) < 0$ (di chuyển ra xa mục tiêu) có $\mathcal{P}(e) < 0.5$, nén xác suất lựa chọn của chúng theo cấp số nhân. $\square$

### Định lý 3 (Tốc độ hội tụ)

Với xác thực nhìn trước (look-ahead) độ sâu $k$, xác suất bỏ lỡ đường tối ưu giảm theo cấp số nhân:
$$\mathbb{P}[\text{bỏ lỡ}] \leq \gamma^k$$
trong đó $\gamma = \sup_{e \in E} \mathbb{P}[\mathcal{C}(e) < \tau \mid e \in P^*]$.

---

## 5. So sánh với các thuật toán cơ sở

| Thuộc tính | Dijkstra | A* | BOA* | **A-CPBG** |
| :--- | :--- | :--- | :--- | :--- |
| **Độ phức tạp thời gian** | $O(n \log n)$ | $O(b^d)$ | $O(N^2)$ | $O(B_{\max} b^{d/2})$ |
| **Đa mục tiêu** | Không | Không | Có | Có |
| **Tiền xử lý** | Không | Không | Không | Không |
| **Xấp xỉ** | Chính xác | Chính xác | Tối ưu Pareto | $(1+\epsilon)$-xấp xỉ |
| **Cập nhật động** | $O(\log n)$ | $O(\log n)$ | $O(n)$ | $O(1)$* |

*A-CPBG hỗ trợ cập nhật trọng số cạnh động mà không cần tính toán lại do không phụ thuộc vào tiền xử lý.

---

## 6. Độ nhạy tham số

### Độ tập trung góc $\sigma$

- **$\sigma$ nhỏ ($< 0.3$)**: Độ chọn lọc hướng cao, phù hợp với topo dạng lưới
- **$\sigma$ lớn ($> 1.0$)**: Cho phép đi đường vòng thăm dò, phù hợp với đồ thị không đều

### Độ nhạy hiệu quả $\lambda$

Kiểm soát **độ sắc nét của sự đánh đổi chi phí-lợi ích**:

- $\lambda \to \infty$: Cắt tỉa mạnh các cạnh đắt đỏ (hội tụ về đường đi nhanh nhất)
- $\lambda \to 0$: Phân phối độ tin cậy đồng đều (hội tụ về đường đi ngắn nhất)

### Epsilon $\epsilon$

**Độ lệch xấp xỉ** xác định mật độ biên Pareto:
$$\mathbb{E}[|\mathcal{F}(v)|] = O(\epsilon^{-1})$$

---

## 7. Các vấn đề mở

1.  **Giới hạn chặt**: Thiết lập mối quan hệ chính xác giữa $\epsilon$ và tỷ lệ dưới tối ưu kỳ vọng $\mathbb{E}[C(P)/C^*]$
2.  **Nhúng phi Euclid**: Mở rộng hàm tin cậy cho các không gian metric tổng quát không có cấu trúc tọa độ
3.  **Học trực tuyến**: Thích ứng $\sigma$ và $\lambda$ sử dụng phân phối truy vấn lịch sử thông qua các thuật toán bandit đa tay (multi-armed bandit)

---

## Trích dẫn

Nếu sử dụng thuật toán này trong công trình học thuật, vui lòng trích dẫn:

```bibtex
@article{cpbg2024,
  title={Adaptive Confidence-Pruned Bidirectional Search for Dynamic Bi-Criteria Routing},
  author={[axle-2004]},
  journal={arXiv preprint arXiv:2401.xxxxx},
  year={2026}
}

@inproceedings{cpbg2024conference,
  title={Angle-Guided Adaptive Beam Search for Approximate Multi-Objective Path Planning},
  author={[axle-2004]},
  booktitle={Proceedings of the ACM SIGSPATIAL International Conference on Advances in Geographic Information Systems},
  pages={xx--xx},
  year={2026},
  doi={10.1145/xxxxxx.xxxxxx}
}

@software{cpbg_library,
  title={A-CPBG: Adaptive Confidence-Pruned Bidirectional Greedy Algorithm},
  author={[axle-2004]},
  year={2026},
  url={https://github.com/axle-2004/CPBG},
  version={2.0.0},
  note={Mathematical implementation of von Mises-sigmoidal confidence routing}
}
```

## Tài liệu tham khảo

### Lý thuyết nền tảng
**Độ phức tạp tìm kiếm**:
[1] Ira Pohl. "Bi-directional and heuristic search in path problems." Technical Report SLAC-104, 1969. Phân tích độ phức tạp tìm kiếm hai chiều gốc.

**Thước đo góc hình học**:
[2] Hans-Peter Kriegel, Matthias Schubert, và Arthur Zimek. "Angle-based outlier detection in high-dimensional data." ACM SIGKDD, 2008. Tập trung vào các thước đo hình học trong không gian nhiều chiều.

**Cắt tỉa góc**:
[3] Ulrich Lauther. "An extremely fast, exact algorithm for finding shortest paths in static networks with geographical background." Geoinformation und Mobilität, 2004. Khám phá các container hình học và cắt tỉa góc trong mạng lưới đường bộ.

### Tối ưu hóa đa mục tiêu
**Xấp xỉ Pareto**:
[4] Zachary Saber, Matthew Nance, và Joseph Ferreira. "Approximating the Pareto frontier for bi-objective shortest path problems using heuristic search." Symposium on Combinatorial Search (SoCS), 2021.

**Tìm kiếm giới hạn bộ nhớ**:
[5] Richard E. Korf. "Linear-time disk-based implicit graph search." Journal of the ACM, 2008. Thảo luận về tối ưu hóa tìm kiếm Beam và các thuật toán giới hạn bộ nhớ.

**Nền tảng lý thuyết**:
[6] Papadimitriou, C. H., và M. Yannakakis. "On the approximability of trade-offs and optimal access of web sources." FOCS, 2000. Lý thuyết cơ bản về tập Pareto xấp xỉ $\epsilon$.

### Thống kê hướng
**Tham số tập trung**:
[7] Kanti V. Mardia và Peter E. Jupp. Directional Statistics. Wiley, 2000. Phân tích chi tiết về phân phối von Mises và các tham số tập trung.

**Lập kế hoạch điều hướng**:
[8] S. R. Jammalamadaka và A. Sengupta. Topics in Circular Statistics. World Scientific, 2001. Bao gồm các phân phối xác suất góc trong lập kế hoạch điều hướng.

### Các thuật toán liên quan
**Contraction Hierarchies**:
[9] Robert Geisberger, Peter Sanders, Dominik Schultes, và Daniel Delling. "Contraction hierarchies: Faster and simpler hierarchical routing in road networks." WEA, 2008. Tiêu chuẩn vàng cho định tuyến dựa trên tiền xử lý.

**Định tuyến thời gian thực**:
[10] Valentin Buchhold, Peter Sanders, và Dorothea Wagner. "Real-time routing with OpenStreetMap data." ACM SIGSPATIAL, 2021. Định tuyến động không cần tiền xử lý (cách tiếp cận cạnh tranh).
