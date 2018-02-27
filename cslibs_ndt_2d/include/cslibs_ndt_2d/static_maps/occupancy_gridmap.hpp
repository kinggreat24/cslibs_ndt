#ifndef CSLIBS_NDT_2D_STATIC_MAPS_OCCUPANCY_GRIDMAP_HPP
#define CSLIBS_NDT_2D_STATIC_MAPS_OCCUPANCY_GRIDMAP_HPP

#include <array>
#include <vector>
#include <cmath>
#include <memory>

#include <cslibs_math_2d/linear/pose.hpp>
#include <cslibs_math_2d/linear/point.hpp>

#include <cslibs_ndt/common/occupancy_distribution.hpp>
#include <cslibs_ndt/common/bundle.hpp>

#include <cslibs_math/linear/pointcloud.hpp>
#include <cslibs_math/common/array.hpp>
#include <cslibs_math/common/div.hpp>
#include <cslibs_math/common/mod.hpp>

#include <cslibs_indexed_storage/storage.hpp>
#include <cslibs_indexed_storage/backend/array/array.hpp>

#include <cslibs_math_2d/algorithms/bresenham.hpp>
#include <cslibs_math_2d/algorithms/simple_iterator.hpp>
#include <cslibs_gridmaps/utility/inverse_model.hpp>

namespace cis = cslibs_indexed_storage;

namespace cslibs_ndt_2d {
namespace static_maps {
class OccupancyGridmap
{
public:
    using Ptr                               = std::shared_ptr<OccupancyGridmap>;
    using pose_t                            = cslibs_math_2d::Pose2d;
    using transform_t                       = cslibs_math_2d::Transform2d;
    using point_t                           = cslibs_math_2d::Point2d;
    using index_t                           = std::array<int, 2>;
    using size_t                            = std::array<std::size_t, 2>;
    using mutex_t                           = std::mutex;
    using lock_t                            = std::unique_lock<mutex_t>;
    using distribution_t                    = cslibs_ndt::OccupancyDistribution<2>;
    using distribution_storage_t            = cis::Storage<distribution_t, index_t, cis::backend::array::Array>;
    using distribution_storage_ptr_t        = std::shared_ptr<distribution_storage_t>;
    using distribution_storage_array_t      = std::array<distribution_storage_ptr_t, 4>;
    using distribution_bundle_t             = cslibs_ndt::Bundle<distribution_t*, 4>;
    using distribution_const_bundle_t       = cslibs_ndt::Bundle<const distribution_t*, 4>;
    using distribution_bundle_storage_t     = cis::Storage<distribution_bundle_t, index_t, cis::backend::array::Array>;
    using distribution_bundle_storage_ptr_t = std::shared_ptr<distribution_bundle_storage_t>;
    using simple_iterator_t                 = cslibs_math_2d::algorithms::SimpleIterator;
    using inverse_sensor_model_t            = cslibs_gridmaps::utility::InverseModel;

    OccupancyGridmap(const pose_t &origin,
            const double           resolution,
            const size_t          &size) :
        resolution_(resolution),
        resolution_inv_(1.0 / resolution_),
        bundle_resolution_(0.5 * resolution_),
        bundle_resolution_inv_(1.0 / bundle_resolution_),
        w_T_m_(origin),
        m_T_w_(w_T_m_.inverse()),
        size_(size),
        storage_{{distribution_storage_ptr_t(new distribution_storage_t),
                  distribution_storage_ptr_t(new distribution_storage_t),
                  distribution_storage_ptr_t(new distribution_storage_t),
                  distribution_storage_ptr_t(new distribution_storage_t)}},
        bundle_storage_(new distribution_bundle_storage_t)
    {
        storage_[0]->template set<cis::option::tags::array_size>(size[0],
                                                                 size[1]);
        for(std::size_t i = 1 ; i < 4 ; ++i) {
            storage_[i]->template set<cis::option::tags::array_size>(size[0] + 1,
                                                                     size[1] + 1);
        }
        bundle_storage_->template set<cis::option::tags::array_size>(size[0] * 2,
                                                                     size[1] * 2);
    }

    OccupancyGridmap(const double  origin_x,
                     const double  origin_y,
                     const double  origin_phi,
                     const double  resolution,
                     const size_t &size) :
        resolution_(resolution),
        resolution_inv_(1.0 / resolution_),
        bundle_resolution_(0.5 * resolution_),
        bundle_resolution_inv_(1.0 / bundle_resolution_),
        w_T_m_(origin_x, origin_y, origin_phi),
        m_T_w_(w_T_m_.inverse()),
        size_(size),
        storage_{{distribution_storage_ptr_t(new distribution_storage_t),
                  distribution_storage_ptr_t(new distribution_storage_t),
                  distribution_storage_ptr_t(new distribution_storage_t),
                  distribution_storage_ptr_t(new distribution_storage_t)}},
        bundle_storage_(new distribution_bundle_storage_t)
    {
        storage_[0]->template set<cis::option::tags::array_size>(size[0],
                                                                 size[1]);
        for(std::size_t i = 1 ; i < 4 ; ++i) {
            storage_[i]->template set<cis::option::tags::array_size>(size[0] + 1,
                                                                     size[1] + 1);
        }
        bundle_storage_->template set<cis::option::tags::array_size>(size[0] * 2,
                                                                     size[1] * 2);
        /// fill the bundle storage
    }

    inline pose_t getOrigin() const
    {
        return w_T_m_;
    }

    template <typename line_iterator_t = simple_iterator_t>
    inline void add(const point_t &start_p,
                    const point_t &end_p)
    {
        const index_t &end_index = toBundleIndex(end_p);
        updateOccupied(end_index, end_p);

        line_iterator_t it(m_T_w_ * start_p, m_T_w_ * end_p, bundle_resolution_);
        while (!it.done()) {
            updateFree({{it.x(), it.y()}});
            ++ it;
        }
    }

    template <typename line_iterator_t = simple_iterator_t>
    inline void insert(const pose_t &origin,
                       const typename cslibs_math::linear::Pointcloud<point_t>::Ptr &points)
    {
        distribution_storage_t storage;
        for (const auto &p : *points) {
            const point_t pm = origin * p;
            if (pm.isNormal()) {
                const index_t &bi = toBundleIndex(pm);
                distribution_t *d = storage.get(bi);
                (d ? d : &storage.insert(bi, distribution_t()))->updateOccupied(pm);
            }
        }

        const point_t start_p = m_T_w_ * origin.translation();
        storage.traverse([this, &start_p](const index_t& bi, const distribution_t &d) {
            if (!d.getDistribution())
                return;
            updateOccupied(bi, d.getDistribution());

            line_iterator_t it(start_p, m_T_w_ * point_t(d.getDistribution()->getMean()), bundle_resolution_);//start_index, bi, 1.0);
            const std::size_t n = d.numOccupied();
            while (!it.done()) {
                updateFree({{it.x(), it.y()}}, n);
                ++ it;
            }
        });
    }

    template <typename line_iterator_t = simple_iterator_t>
    inline void insertVisible(const pose_t &origin,
                              const typename cslibs_math::linear::Pointcloud<point_t>::Ptr &points,
                              const inverse_sensor_model_t::Ptr &ivm,
                              const inverse_sensor_model_t::Ptr &ivm_visibility)
    {
        const index_t start_bi = toBundleIndex(origin.translation());
        auto occupancy = [this, &ivm](const index_t &bi) {
            const distribution_bundle_t *bundle = getDistributionBundle(bi);
            return 0.25 * (bundle->at(0)->getOccupancy(ivm) +
                           bundle->at(1)->getOccupancy(ivm) +
                           bundle->at(2)->getOccupancy(ivm) +
                           bundle->at(3)->getOccupancy(ivm));
        };
        auto current_visibility = [this, &start_bi, &ivm_visibility, &occupancy](const index_t &bi) {
            const double occlusion_prob =
                    std::min(occupancy({{bi[0] + ((bi[0] > start_bi[0]) ? -1 : 1), bi[1]}}),
                             occupancy({{bi[0], bi[1] + ((bi[1] > start_bi[1]) ? -1 : 1)}}));
            return ivm_visibility->getProbFree() * occlusion_prob +
                   ivm_visibility->getProbOccupied() * (1.0 - occlusion_prob);
        };

        distribution_storage_t storage;
        for (const auto &p : *points) {
            const point_t pm = origin * p;
            if (pm.isNormal()) {
                const index_t &bi = toBundleIndex(pm);
                distribution_t *d = storage.get(bi);
                (d ? d : &storage.insert(bi, distribution_t()))->updateOccupied(pm);
            }
        }

        const point_t start_p = m_T_w_ * origin.translation();
        storage.traverse([this, &ivm_visibility, &start_p, &current_visibility](const index_t& bi, const distribution_t &d) {
            if (!d.getDistribution())
                return;

            const point_t end_p = m_T_w_ * point_t(d.getDistribution()->getMean());
            line_iterator_t it(start_p, end_p, bundle_resolution_);

            const std::size_t n = d.numOccupied();
            double visibility = 1.0;
            while (!it.done()) {
                const index_t bit = {{it.x(), it.y()}};
                if ((visibility *= current_visibility(bit)) < ivm_visibility->getProbPrior())
                    return;

                updateFree(bit, n);
                ++ it;
            }

            if ((visibility *= current_visibility(bi)) >= ivm_visibility->getProbPrior())
                updateOccupied(bi, d.getDistribution());
        });
    }

    inline double sample(const point_t &p,
                         const inverse_sensor_model_t::Ptr &ivm) const
    {
        const index_t bi = toBundleIndex(p);
        return sample(p, bi, ivm);
    }

    inline double sample(const point_t &p,
                         const index_t &bi,
                         const inverse_sensor_model_t::Ptr &ivm) const
    {
        if (!ivm)
            throw std::runtime_error("[OccupancyGridMap]: inverse model not set");

        distribution_bundle_t *bundle;
        {
            lock_t(bundle_storage_mutex_);
            bundle = getAllocate(bi);
        }
        auto sample = [&p, &ivm](const distribution_t *d) {
            return (d && d->getDistribution()) ? d->getDistribution()->sample(p) *
                                                 d->getOccupancy(ivm) : 0.0;
        };
        auto evaluate = [&p, &bundle, &sample]() {
            return 0.25 * (sample(bundle->at(0)) +
                           sample(bundle->at(1)) +
                           sample(bundle->at(2)) +
                           sample(bundle->at(3)));
        };
        return evaluate();
    }

    inline double sampleNonNormalized(const point_t &p,
                                      const inverse_sensor_model_t::Ptr &ivm) const
    {
        const index_t bi = toBundleIndex(p);
        return sampleNonNormalized(p, bi, ivm);
    }

    inline double sampleNonNormalized(const point_t &p,
                                      const index_t &bi,
                                      const inverse_sensor_model_t::Ptr &ivm) const
    {
        if (!ivm)
            throw std::runtime_error("[OccupancyGridMap]: inverse model not set");

        distribution_bundle_t *bundle;
        {
            lock_t(bundle_storage_mutex_);
            bundle = getAllocate(bi);
        }
        auto sampleNonNormalized = [&p, &ivm](const distribution_t *d) {
            return (d && d->getDistribution()) ? d->getDistribution()->sampleNonNormalized(p) *
                                                 d->getOccupancy(ivm) : 0.0;
        };
        auto evaluate = [&p, &bundle, &sampleNonNormalized]() {
          return 0.25 * (sampleNonNormalized(bundle->at(0)) +
                         sampleNonNormalized(bundle->at(1)) +
                         sampleNonNormalized(bundle->at(2)) +
                         sampleNonNormalized(bundle->at(3)));
        };
        return evaluate();
    }

    inline const distribution_bundle_t* getDistributionBundle(const index_t &bi) const
    {
        return getAllocate(bi);
    }

    inline distribution_bundle_t* getDistributionBundle(const index_t &bi)
    {
        return getAllocate(bi);
    }

    inline double getBundleResolution() const
    {
        return bundle_resolution_;
    }

    inline double getResolution() const
    {
        return resolution_;
    }

    inline double getHeight() const
    {
        return size_[1] * resolution_;
    }

    inline double getWidth() const
    {
        return size_[0] * resolution_;
    }

    inline size_t getSize() const
    {
        return size_;
    }

    inline size_t getBundleSize() const
    {
        return {{size_[0] * 2, size_[1] * 2}};
    }

    inline distribution_storage_array_t const & getStorages() const
    {
        return storage_;
    }

    inline void getBundleIndices(std::vector<index_t> &indices) const
    {
        lock_t(bundle_storage_mutex_);
        auto add_index = [&indices](const index_t &i, const distribution_bundle_t &d) {
            indices.emplace_back(i);
        };
        bundle_storage_->traverse(add_index);
    }

    inline std::size_t getByteSize() const
    {
        return sizeof(*this) +
                bundle_storage_->byte_size() +
                storage_[0]->byte_size() +
                storage_[1]->byte_size() +
                storage_[2]->byte_size() +
                storage_[3]->byte_size();
    }

protected:
    const double                                    resolution_;
    const double                                    resolution_inv_;
    const double                                    bundle_resolution_;
    const double                                    bundle_resolution_inv_;
    const transform_t                               w_T_m_;
    const transform_t                               m_T_w_;
    const size_t                                    size_;

    mutable mutex_t                                 storage_mutex_;
    mutable distribution_storage_array_t            storage_;
    mutable mutex_t                                 bundle_storage_mutex_;
    mutable distribution_bundle_storage_ptr_t       bundle_storage_;

    inline distribution_t* getAllocate(const distribution_storage_ptr_t &s,
                                       const index_t &i) const
    {
        lock_t l(storage_mutex_);
        distribution_t *d = s->get(i);
        return d ? d : &(s->insert(i, distribution_t()));
    }

    inline distribution_bundle_t *getAllocate(const index_t &bi) const
    {
        distribution_bundle_t *bundle;
        {
            lock_t(bundle_storage_mutex_);
            bundle = bundle_storage_->get(bi);
        }

        auto allocate_bundle = [this, &bi]() {
            distribution_bundle_t b;
            const int divx = cslibs_math::common::div(bi[0], 2);
            const int divy = cslibs_math::common::div(bi[1], 2);
            const int modx = cslibs_math::common::mod(bi[0], 2);
            const int mody = cslibs_math::common::mod(bi[1], 2);

            const index_t storage_0_index = {{divx,        divy}};
            const index_t storage_1_index = {{divx + modx, divy}};        /// shifted to the left
            const index_t storage_2_index = {{divx,        divy + mody}}; /// shifted to the bottom
            const index_t storage_3_index = {{divx + modx, divy + mody}}; /// shifted diagonally

            b[0] = getAllocate(storage_[0], storage_0_index);
            b[1] = getAllocate(storage_[1], storage_1_index);
            b[2] = getAllocate(storage_[2], storage_2_index);
            b[3] = getAllocate(storage_[3], storage_3_index);

            lock_t(bundle_storage_mutex_);
            return &(bundle_storage_->insert(bi, b));
        };

        return bundle == nullptr ? allocate_bundle() : bundle;
    }

    inline void updateFree(const index_t &bi) const
    {
        distribution_bundle_t *bundle;
        {
            lock_t(bundle_storage_mutex_);
            bundle = getAllocate(bi);
        }
        bundle->at(0)->updateFree();
        bundle->at(1)->updateFree();
        bundle->at(2)->updateFree();
        bundle->at(3)->updateFree();
    }

    inline void updateFree(const index_t &bi,
                           const std::size_t &n) const
    {
        distribution_bundle_t *bundle;
        {
            lock_t(bundle_storage_mutex_);
            bundle = getAllocate(bi);
        }
        bundle->at(0)->updateFree(n);
        bundle->at(1)->updateFree(n);
        bundle->at(2)->updateFree(n);
        bundle->at(3)->updateFree(n);
    }

    inline void updateOccupied(const index_t &bi,
                               const point_t &p) const
    {
        distribution_bundle_t *bundle;
        {
            lock_t(bundle_storage_mutex_);
            bundle = getAllocate(bi);
        }
        bundle->at(0)->updateOccupied(p);
        bundle->at(1)->updateOccupied(p);
        bundle->at(2)->updateOccupied(p);
        bundle->at(3)->updateOccupied(p);
    }

    inline void updateOccupied(const index_t &bi,
                               const distribution_t::distribution_ptr_t &d) const
    {
        distribution_bundle_t *bundle;
        {
            lock_t(bundle_storage_mutex_);
            bundle = getAllocate(bi);
        }
        bundle->at(0)->updateOccupied(d);
        bundle->at(1)->updateOccupied(d);
        bundle->at(2)->updateOccupied(d);
        bundle->at(3)->updateOccupied(d);
    }

    inline index_t toBundleIndex(const point_t &p_w) const
    {
        const point_t p_m = m_T_w_ * p_w;
        return {{static_cast<int>(std::floor(p_m(0) * bundle_resolution_inv_)),
                 static_cast<int>(std::floor(p_m(1) * bundle_resolution_inv_))}};
    }
};
}
}
#endif // CSLIBS_NDT_2D_STATIC_MAPS_OCCUPANCY_GRIDMAP_HPP
