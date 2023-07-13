#include "common.hpp"

int getIdxFrom3dIdx(const int i, const int j, const int k, const int N, const int P)
{
  return i * N * P + j * P + k;
}

void get3dIdxFromIdx(int &i, int &j, int &k, const int idx, const int N, const int P)
{
  i = idx / (N * P);
  j = (idx / P) % N;
  k = idx % P;
}

template <typename T>
void common<T>::getSprMatData(std::vector<int> &csrRowOffsets, std::vector<int> &csrColInd, std::vector<T> &csrValues, const std::vector<double> &k_x, const std::vector<double> &k_y, const std::vector<double> &k_z)
{
  int M{dims[0]}, N{dims[1]}, P{dims[2]};
  int size{M * N * P}, row{0};

  csrRowOffsets[0] = 0;
#pragma omp parallel for
  for (row = 0; row < size; ++row) {
    int    i{0}, j{0}, k{0}, col{0};
    double mean_k = 0.0;

    get3dIdxFromIdx(i, j, k, row, N, P);
    csrRowOffsets[row + 1] = 0;
    // cols order, 0, z-, z+, y-, y+, x-, x+
    csrColInd[row * STENCIL_WIDTH] = row;
    csrValues[row * STENCIL_WIDTH] = static_cast<T>(0.0);
    if (k - 1 >= 0) {
      col = getIdxFrom3dIdx(i, j, k - 1, N, P);
      // col    = i * N * P + j * P + k - 1;
      mean_k = 2.0 / (1.0 / k_z[row] + 1.0 / k_z[col]);
      csrValues[row * STENCIL_WIDTH] += static_cast<T>(mean_k);
      csrRowOffsets[row + 1]++;
      csrColInd[row * STENCIL_WIDTH + csrRowOffsets[row + 1]] = col;
      csrValues[row * STENCIL_WIDTH + csrRowOffsets[row + 1]] = static_cast<T>(-mean_k);
    }
    if (k + 1 < P) {
      col = getIdxFrom3dIdx(i, j, k + 1, N, P);
      // col    = i * N * P + j * P + k + 1;
      mean_k = 2.0 / (1.0 / k_z[row] + 1.0 / k_z[col]);
      csrValues[row * STENCIL_WIDTH] += static_cast<T>(mean_k);
      csrRowOffsets[row + 1]++;
      csrColInd[row * STENCIL_WIDTH + csrRowOffsets[row + 1]] = col;
      csrValues[row * STENCIL_WIDTH + csrRowOffsets[row + 1]] = static_cast<T>(-mean_k);
    }
    if (j - 1 >= 0) {
      col = getIdxFrom3dIdx(i, j - 1, k, N, P);
      // col    = i * N * P + (j - 1) * P + k;
      mean_k = 2.0 / (1.0 / k_y[row] + 1.0 / k_y[col]);
      csrValues[row * STENCIL_WIDTH] += static_cast<T>(mean_k);
      csrRowOffsets[row + 1]++;
      csrColInd[row * STENCIL_WIDTH + csrRowOffsets[row + 1]] = col;
      csrValues[row * STENCIL_WIDTH + csrRowOffsets[row + 1]] = static_cast<T>(-mean_k);
    }
    if (j + 1 < N) {
      col = getIdxFrom3dIdx(i, j + 1, k, N, P);
      // col    = i * N * P + (j + 1) * P + k;
      mean_k = 2.0 / (1.0 / k_y[row] + 1.0 / k_y[col]);
      csrValues[row * STENCIL_WIDTH] += static_cast<T>(mean_k);
      csrRowOffsets[row + 1]++;
      csrColInd[row * STENCIL_WIDTH + csrRowOffsets[row + 1]] = col;
      csrValues[row * STENCIL_WIDTH + csrRowOffsets[row + 1]] = static_cast<T>(-mean_k);
    }
    if (i - 1 >= 0) {
      col = getIdxFrom3dIdx(i - 1, j, k, N, P);
      // col    = (i - 1) * N * P + j * P + k;
      mean_k = 2.0 / (1.0 / k_x[row] + 1.0 / k_x[col]);
      csrValues[row * STENCIL_WIDTH] += static_cast<T>(mean_k);
      csrRowOffsets[row + 1]++;
      csrColInd[row * STENCIL_WIDTH + csrRowOffsets[row + 1]] = col;
      csrValues[row * STENCIL_WIDTH + csrRowOffsets[row + 1]] = static_cast<T>(-mean_k);
    }
    if (i + 1 < M) {
      col = getIdxFrom3dIdx(i + 1, j, k, N, P);
      // col    = (i + 1) * N * P + j * P + k;
      mean_k = 2.0 / (1.0 / k_x[row] + 1.0 / k_x[col]);
      csrValues[row * STENCIL_WIDTH] += static_cast<T>(mean_k);
      csrRowOffsets[row + 1]++;
      csrColInd[row * STENCIL_WIDTH + csrRowOffsets[row + 1]] = col;
      csrValues[row * STENCIL_WIDTH + csrRowOffsets[row + 1]] = static_cast<T>(-mean_k);
    }
    if (k == 0 || k == P - 1) csrValues[row * STENCIL_WIDTH] += static_cast<T>(2.0 * k_z[row]);
    csrRowOffsets[row + 1]++;
  }

  // Clean the unused memory.
  for (row = 0; row < size; ++row) {
    std::memmove(&csrColInd[csrRowOffsets[row]], &csrColInd[row * STENCIL_WIDTH], sizeof(int) * csrRowOffsets[row + 1]);
    std::memmove(&csrValues[csrRowOffsets[row]], &csrValues[row * STENCIL_WIDTH], sizeof(T) * csrRowOffsets[row + 1]);
    csrRowOffsets[row + 1] += csrRowOffsets[row];
  }
}

template <typename T>
void common<T>::getStdRhsVec(std::vector<T> &rhs, const std::vector<double> &k_z, const double delta_p)
{
  int M{dims[0]}, N{dims[1]}, P{dims[2]};
  int size{M * N * P}, row{0};

#pragma omp parallel for
  for (int row{0}; row < size; ++row) {
    int i{0}, j{0}, k{0};
    get3dIdxFromIdx(i, j, k, row, N, P);
    rhs[row] = 0;
    if (k == 0) rhs[row] += 2 * static_cast<T>(k_z[row] * delta_p);
  }
}

template <typename T>
void common<T>::getHomoCoeffZ(T &homoCoeffZ, const std::vector<T> &p, const std::vector<double> &k_z, const double delta_p, const double lenZ)
{
  int    M{dims[0]}, N{dims[1]}, P{dims[2]}, i{0}, j{0}, row{0};
  double temp{0.0};

  homoCoeffZ = 0;
#pragma omp parallel for reduction(+ : homoCoeffZ)
  for (i = 0; i < M; ++i)
    for (j = 0; j < N; ++j) {
      row = getIdxFrom3dIdx(i, j, 0, N, P);
      // row  = i * N * P + j * P;
      temp = 2.0 * lenZ * lenZ / (M * N * P) * k_z[row] * (static_cast<double>(p[row]) - delta_p) / delta_p;
      homoCoeffZ += static_cast<T>(temp);
    }
}

template <typename T>
void common<T>::getTridSolverData(std::vector<T> &dl, std::vector<T> &d, std::vector<T> &du, const std::vector<T> &homoParas)
{
  int M{dims[0]}, N{dims[1]}, P{dims[2]};
  T   myPi{static_cast<T>(M_PI)};
  T   k_x_ref{homoParas[0]}, k_y_ref{homoParas[1]}, k_z_ref{homoParas[2]};
  T   k_in_ref{homoParas[3]}, k_out_ref{homoParas[4]};
#pragma omp parallel for
  for (int matIdx{0}; matIdx < M * N; ++matIdx) {
    int i{matIdx / N}, j{matIdx % N};
    T   temp_i{2 * (1 - std::cos(static_cast<T>(i) / M * myPi))};
    T   temp_j{2 * (1 - std::cos(static_cast<T>(j) / N * myPi))};
    dl[matIdx * P] = 0;
    d[matIdx * P]  = (k_x_ref * temp_i) + (k_y_ref * temp_j) + k_z_ref + 2 * k_in_ref;
    du[matIdx * P] = -k_z_ref;
    for (int k = 1; k < P - 1; ++k) {
      dl[matIdx * P + k] = -k_z_ref;
      d[matIdx * P + k]  = (k_x_ref * temp_i) + (k_y_ref * temp_j) + (2 * k_z_ref);
      du[matIdx * P + k] = -k_z_ref;
    }
    dl[(matIdx + 1) * P - 1] = -k_z_ref;
    d[(matIdx + 1) * P - 1]  = (k_x_ref * temp_i) + (k_y_ref * temp_j) + k_z_ref + (2 * k_out_ref);
    du[(matIdx + 1) * P - 1] = 0;
  }
}

template <typename T>
void common<T>::setTestVecs(std::vector<T> &v, std::vector<T> &v_hat)
{
  int M{dims[0]}, N{dims[1]}, P{dims[2]};
  int size{M * N * P};
  int i{0}, j{0}, k{0}, i_t{1}, j_t{2};
  T   myPi{static_cast<T>(M_PI)}, myHalf{static_cast<T>(0.5)};
  for (int row{0}; row < size; ++row) {
    get3dIdxFromIdx(i, j, k, row, N, P);
    if (i_t == i && j_t == j) v_hat[row] = 1;
    else v_hat[row] = 0;
    v[row] = static_cast<T>(4) / static_cast<T>(M * N);
    v[row] *= std::cos(myPi * (static_cast<T>(i) + myHalf) * static_cast<T>(i_t) / static_cast<T>(M));
    v[row] *= std::cos(myPi * (static_cast<T>(j) + myHalf) * static_cast<T>(j_t) / static_cast<T>(N));
  }
}

template <typename T>
void common<T>::setTestPrecondSolver(std::vector<T> &u, std::vector<T> &rhs, const T k_x, const T k_y, const T k_z)
{
  int M{dims[0]}, N{dims[1]}, P{dims[2]};
  T   h_x{static_cast<T>(1) / M}, h_y{static_cast<T>(1) / N}, h_z{static_cast<T>(1) / P};
  T   myHalf{static_cast<T>(0.5)}, myPi{static_cast<T>(M_PI)};
  for (int idx{0}; idx < M * N * P; ++idx) {
    int i{0}, j{0}, k{0};
    get3dIdxFromIdx(i, j, k, idx, N, P);
    T x{(i + myHalf) * h_x}, y{(j + myHalf) * h_y}, z{(k + myHalf) * h_z};
    /* This is the solution. */
    u[idx]   = std::cos(x * myPi) * std::cos(y * myPi) * std::exp(z);
    rhs[idx] = k_x * myPi * myPi * std::cos(x * myPi) * std::cos(y * myPi) * std::exp(z);
    rhs[idx] += k_y * myPi * myPi * std::cos(x * myPi) * std::cos(y * myPi) * std::exp(z);
    rhs[idx] -= k_z * std::cos(x * myPi) * std::cos(y * myPi) * std::exp(z);
    if (0 == k) {
      z = static_cast<T>(0);
      rhs[idx] += 2 * k_z / (h_z * h_z) * std::cos(x * myPi) * std::cos(y * myPi) * std::exp(z);
    }
    if (P - 1 == k) {
      z = static_cast<T>(1);
      rhs[idx] += 2 * k_z / (h_z * h_z) * std::cos(x * myPi) * std::cos(y * myPi) * std::exp(z);
    }
  }
}

template struct common<float>;

template struct common<double>;
