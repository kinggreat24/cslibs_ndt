#ifndef CSLIBS_NDT_2D_CONVERSION_PROBABILITY_GRIDMAP_HPP
#define CSLIBS_NDT_2D_CONVERSION_PROBABILITY_GRIDMAP_HPP

#include <cslibs_ndt_2d/dynamic_maps/gridmap.hpp>
#include <cslibs_ndt_2d/dynamic_maps/occupancy_gridmap.hpp>

#include <cslibs_ndt_2d/conversion/gridmap.hpp>
#include <cslibs_ndt_2d/conversion/occupancy_gridmap.hpp>

#include <cslibs_gridmaps/static_maps/probability_gridmap.h>

namespace cslibs_ndt_2d {
namespace conversion {
inline void from(
        const cslibs_ndt_2d::dynamic_maps::Gridmap::Ptr &src,
        cslibs_gridmaps::static_maps::ProbabilityGridmap::Ptr &dst,
        const double &sampling_resolution)
{
    if (!src)
        return;

    using index_t = std::array<int, 2>;
    const index_t min_bi = src->getMinDistributionIndex();
    const index_t max_bi = src->getMaxDistributionIndex();
    if (min_bi[0] == std::numeric_limits<int>::max() ||
            min_bi[1] == std::numeric_limits<int>::max() ||
            max_bi[0] == std::numeric_limits<int>::min() ||
            max_bi[1] == std::numeric_limits<int>::min())
        return;

    using src_map_t = cslibs_ndt_2d::dynamic_maps::Gridmap;
    using dst_map_t = cslibs_gridmaps::static_maps::ProbabilityGridmap;
    dst.reset(new dst_map_t(src->getOrigin(),
                            sampling_resolution,
                            src->getHeight() / sampling_resolution,
                            src->getWidth()  / sampling_resolution));
    std::fill(dst->getData().begin(), dst->getData().end(), 0);

    const double bundle_resolution = src->getBundleResolution();
    const int chunk_step = static_cast<int>(bundle_resolution / sampling_resolution);

    auto sample = [](const cslibs_math_2d::Point2d &p, const src_map_t::distribution_bundle_t &bundle) {
        return 0.25 * (bundle.at(0)->getHandle()->data().sampleNonNormalized(p) +
                       bundle.at(1)->getHandle()->data().sampleNonNormalized(p) +
                       bundle.at(2)->getHandle()->data().sampleNonNormalized(p) +
                       bundle.at(3)->getHandle()->data().sampleNonNormalized(p));
    };

    auto process_bundle = [&dst, &bundle_resolution, &sampling_resolution, &chunk_step, &min_bi, &sample]
                  (const index_t &bi, const src_map_t::distribution_bundle_t &b){
        for (int k = 0 ; k < chunk_step ; ++ k) {
            for (int l = 0 ; l < chunk_step ; ++ l) {
                const int dst_x = (bi[0] - min_bi[0]) * chunk_step + k;
                const int dst_y = (bi[1] - min_bi[1]) * chunk_step + l;
                if (dst_x < 0 || dst_y < 0 ||
                        dst_x >= static_cast<int>(dst->getWidth()) || dst_y >= static_cast<int>(dst->getHeight()))
                    return;

                const cslibs_math_2d::Point2d p(bi[0] * bundle_resolution + k * sampling_resolution,
                                                bi[1] * bundle_resolution + l * sampling_resolution);
                dst->at(dst_x, dst_y) = sample(p, b);
            }
        }
    };
    src->traverse(process_bundle);
}

inline void from(
        const cslibs_ndt_2d::dynamic_maps::OccupancyGridmap::Ptr &src,
        cslibs_gridmaps::static_maps::ProbabilityGridmap::Ptr &dst,
        const double &sampling_resolution,
        const cslibs_gridmaps::utility::InverseModel::Ptr &inverse_model)
{
    if (!src || !inverse_model)
        return;

    using index_t = std::array<int, 2>;
    const index_t min_bi = src->getMinDistributionIndex();
    const index_t max_bi = src->getMaxDistributionIndex();
    if (min_bi[0] == std::numeric_limits<int>::max() ||
            min_bi[1] == std::numeric_limits<int>::max() ||
            max_bi[0] == std::numeric_limits<int>::min() ||
            max_bi[1] == std::numeric_limits<int>::min())
        return;

    using src_map_t = cslibs_ndt_2d::dynamic_maps::OccupancyGridmap;
    using dst_map_t = cslibs_gridmaps::static_maps::ProbabilityGridmap;
    dst.reset(new dst_map_t(src->getOrigin(),
                            sampling_resolution,
                            src->getHeight() / sampling_resolution,
                            src->getWidth()  / sampling_resolution));
    std::fill(dst->getData().begin(), dst->getData().end(), 0);

    const double bundle_resolution = src->getBundleResolution();
    const int chunk_step = static_cast<int>(bundle_resolution / sampling_resolution);

    auto sample = [&inverse_model](const cslibs_math_2d::Point2d &p, const src_map_t::distribution_bundle_t &bundle) {
        auto sample = [&p, &inverse_model](const src_map_t::distribution_t *d) {
            auto do_sample = [&p, &inverse_model, &d]() {
                const auto &handle = d->getHandle();
                return handle->getDistribution() ?
                            handle->getDistribution()->sampleNonNormalized(p) * handle->getOccupancy(inverse_model) : 0.0;
            };
            return d ? do_sample() : 0.0;
        };
        return 0.25 * (sample(bundle.at(0)) +
                       sample(bundle.at(1)) +
                       sample(bundle.at(2)) +
                       sample(bundle.at(3)));
    };

    auto process_bundle = [&dst, &bundle_resolution, &sampling_resolution, &chunk_step, &min_bi, &sample]
                  (const index_t &bi, const src_map_t::distribution_bundle_t &b){
        for (int k = 0 ; k < chunk_step ; ++ k) {
            for (int l = 0 ; l < chunk_step ; ++ l) {
                const int dst_x = (bi[0] - min_bi[0]) * chunk_step + k;
                const int dst_y = (bi[1] - min_bi[1]) * chunk_step + l;
                if (dst_x < 0 || dst_y < 0 ||
                        dst_x >= static_cast<int>(dst->getWidth()) || dst_y >= static_cast<int>(dst->getHeight()))
                    return;

                const cslibs_math_2d::Point2d p(bi[0] * bundle_resolution + k * sampling_resolution,
                                                bi[1] * bundle_resolution + l * sampling_resolution);
                dst->at(dst_x, dst_y) = sample(p, b);
            }
        }
    };
    src->traverse(process_bundle);
}
}
}

#endif // CSLIBS_NDT_2D_CONVERSION_PROBABILITY_GRIDMAP_HPP
