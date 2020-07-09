#pragma once

#include <tensorview/kernel_utils.h>
#include <tensorview/tensorview.h>
#include <torch/script.h>

namespace spconv {
template <typename Index, unsigned NDim>
__global__ void scatterPointToGridKernel(
    tv::TensorView<const float> points, tv::TensorView<const Index> indexes,
    tv::TensorView<float> grids, tv::TensorView<Index> numPointsPerGrid,
    tv::TensorView<Index> pointIndex,
    const tv::SimpleVector<Index, NDim> gridShape) {
  Index index;
  int numPoints = points.dim(0);
  int numFeatures = points.dim(1);

  for (int ix : tv::KernelLoopX<int>(numPoints)) {
    index = tv::ArrayIndexRowMajor<NDim, NDim>::runPtrs(
        indexes.data() + ix * NDim, gridShape.data(), 0);
    pointIndex(ix) = index;
    atomicAdd(numPointsPerGrid.data() + index, Index(1));
#pragma unroll
    for (int k = 0; k != numFeatures; ++k) {
      atomicAdd(grids.data() + index * numFeatures + k,
                *(points.data() + ix * numFeatures + k));
    }
  }
}

template <typename Index, unsigned NDim>
__global__ void
gatherPointFromGridKernel(tv::TensorView<const float> grids,
                          tv::TensorView<const Index> numPointsPerGrid,
                          tv::TensorView<const Index> pointIndexUnique,
                          tv::TensorView<float> voxels,
                          tv::TensorView<Index> coors,
                          const tv::SimpleVector<Index, NDim> gridShape) {
  Index index;
  int numVoxels = voxels.dim(0);
  int numFeatures = grids.dim(1);

  for (int ix : tv::KernelLoopX<int>(numVoxels)) {
    index = pointIndexUnique(ix);
#pragma unroll
    for (int k = 0; k != numFeatures; ++k) {
      voxels(ix, k) = grids(index, k) / numPointsPerGrid(index);
    }
    index = tv::rowArrayIdxInv<Index, NDim>(index, coors.data() + ix * NDim,
                                            gridShape.data());
  }
}

template <typename Index>
__global__ void resetGridKernel(tv::TensorView<float> grids,
                                tv::TensorView<Index> numPointsPerGrid,
                                tv::TensorView<Index> pointIndexUnique) {
  Index index;
  int numVoxels = pointIndexUnique.dim(0) - 1;
  int numFeatures = grids.dim(1);

  for (int ix : tv::KernelLoopX<int>(numVoxels)) {
    index = pointIndexUnique(ix);
#pragma unroll
    for (int k = 0; k != numFeatures; ++k) {
      grids(index, k) = 0;
      numPointsPerGrid(index) = 0;
    }
  }
}

template <typename Index>
__global__ void resetPointIndexKernel(tv::TensorView<Index> pointIndex,
                                      const Index gridVolume) {
  int num_max_points = pointIndex.dim(0) - 1;

  for (int ix : tv::KernelLoopX<int>(num_max_points)) {
    pointIndex(ix) = gridVolume;
  }
}
} // namespace spconv