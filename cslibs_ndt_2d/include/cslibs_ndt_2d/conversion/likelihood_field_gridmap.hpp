#ifndef CSLIBS_NDT_2D_CONVERSION_LIKELIHOOD_FIELD_GRIDMAP_HPP
#define CSLIBS_NDT_2D_CONVERSION_LIKELIHOOD_FIELD_GRIDMAP_HPP

#include <cslibs_ndt_2d/dynamic_maps/gridmap.hpp>
#include <cslibs_ndt_2d/dynamic_maps/occupancy_gridmap.hpp>

#include <cslibs_ndt_2d/conversion/gridmap.hpp>
#include <cslibs_ndt_2d/conversion/occupancy_gridmap.hpp>

#include <cslibs_gridmaps/static_maps/likelihood_field_gridmap.h>
#include <cslibs_gridmaps/static_maps/algorithms/distance_transform.hpp>

namespace cslibs_ndt_2d {
namespace conversion {
inline void from(
        const cslibs_ndt_2d::dynamic_maps::Gridmap::Ptr &src,
        cslibs_gridmaps::static_maps::LikelihoodFieldGridmap::Ptr &dst,
        const double &sampling_resolution,
        const double &maximum_distance = 2.0,
        const double &sigma_hit        = 0.5,
        const double &threshold        = 0.5)
{
    assert(threshold <= 1.0);
    assert(threshold >= 0.0);
    const double exp_factor_hit = (0.5 * 1.0 / (sigma_hit * sigma_hit));

    using dst_map_t = cslibs_gridmaps::static_maps::LikelihoodFieldGridmap;
    dst.reset(new dst_map_t(src->getOrigin(),
                            sampling_resolution,
                            src->getHeight() / sampling_resolution,
                            src->getWidth()  / sampling_resolution,
                            maximum_distance,
                            sigma_hit));

    const double bundle_resolution = src->getBundleResolution();
    const int chunk_step = static_cast<int>(bundle_resolution / sampling_resolution);

    using index_t = std::array<int, 2>;
    const index_t min_distribution_index = src->getMinDistributionIndex();
    const index_t max_distribution_index = src->getMaxDistributionIndex();

    for (int i = min_distribution_index[0] ; i < max_distribution_index[0] ; ++ i) {
        for (int j = min_distribution_index[1] ; j < max_distribution_index[1] ; ++ j) {
            const int ci = (i - min_distribution_index[0]) * static_cast<int>(chunk_step);
            const int cj = (j - min_distribution_index[1]) * static_cast<int>(chunk_step);

            for (int k = 0 ; k < chunk_step ; ++k) {
                for (int l = 0 ; l < chunk_step ; ++l) {
                    const cslibs_math_2d::Point2d p(i * bundle_resolution + k * sampling_resolution,
                                                    j * bundle_resolution + l * sampling_resolution);

                    dst->at(static_cast<std::size_t>(ci + k),
                            static_cast<std::size_t>(cj + l)) =
                            1.0 - src->sampleNonNormalized(
                                src->getInitialOrigin() * p, {{static_cast<int>(i), static_cast<int>(j)}});
                }
            }
        }
    }

    std::vector<double> occ = dst->getData();

    cslibs_gridmaps::static_maps::algorithms::DistanceTransform<double> distance_transform(
                sampling_resolution, maximum_distance, threshold);
    distance_transform.apply(occ, dst->getWidth(), dst->getData());

    std::for_each(dst->getData().begin(),
                  dst->getData().end(),
                  [&exp_factor_hit] (double &z) {z = std::exp(-z * z * exp_factor_hit);});
}

inline void from(
        const cslibs_ndt_2d::dynamic_maps::OccupancyGridmap::Ptr &src,
        cslibs_gridmaps::static_maps::LikelihoodFieldGridmap::Ptr &dst,
        const double &sampling_resolution,
        const cslibs_gridmaps::utility::InverseModel::Ptr &inverse_model,
        const double &maximum_distance = 2.0,
        const double &sigma_hit        = 0.5,
        const double &threshold        = 0.5)
{
    assert(threshold <= 1.0);
    assert(threshold >= 0.0);
    const double exp_factor_hit = (0.5 * 1.0 / (sigma_hit * sigma_hit));

    using dst_map_t = cslibs_gridmaps::static_maps::LikelihoodFieldGridmap;
    dst.reset(new dst_map_t(src->getOrigin(),
                            sampling_resolution,
                            src->getHeight() / sampling_resolution,
                            src->getWidth()  / sampling_resolution,
                            maximum_distance,
                            sigma_hit));

    const double bundle_resolution = src->getBundleResolution();
    const int chunk_step = static_cast<int>(bundle_resolution / sampling_resolution);

    using index_t = std::array<int, 2>;
    const index_t min_distribution_index = src->getMinDistributionIndex();
    const index_t max_distribution_index = src->getMaxDistributionIndex();

    for (int i = min_distribution_index[0] ; i < max_distribution_index[0] ; ++ i) {
        for (int j = min_distribution_index[1] ; j < max_distribution_index[1] ; ++ j) {
            const int ci = (i - min_distribution_index[0]) * static_cast<int>(chunk_step);
            const int cj = (j - min_distribution_index[1]) * static_cast<int>(chunk_step);

            for (int k = 0 ; k < chunk_step ; ++k) {
                for (int l = 0 ; l < chunk_step ; ++l) {
                    const cslibs_math_2d::Point2d p(i * bundle_resolution + k * sampling_resolution,
                                                    j * bundle_resolution + l * sampling_resolution);

                    dst->at(static_cast<std::size_t>(ci + k),
                            static_cast<std::size_t>(cj + l)) =
                            1.0 - src->sampleNonNormalized(
                                src->getInitialOrigin() * p, {{static_cast<int>(i), static_cast<int>(j)}}, inverse_model);
                }
            }
        }
    }

    std::vector<double> occ = dst->getData();

    cslibs_gridmaps::static_maps::algorithms::DistanceTransform<double> distance_transform(
                sampling_resolution, maximum_distance, threshold);
    distance_transform.apply(occ, dst->getWidth(), dst->getData());

    std::for_each(dst->getData().begin(),
                  dst->getData().end(),
                  [&exp_factor_hit] (double &z) {z = std::exp(-z * z * exp_factor_hit);});
}
}
}

#endif // CSLIBS_NDT_2D_CONVERSION_LIKELIHOOD_FIELD_GRIDMAP_HPP