#pragma once

#include "data_structures.hpp"
#include "gfx/generators/voxel/shared_data_structures.slang"

//

#include <boost/geometry/algorithms/append.hpp>
#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/algorithms/assign.hpp>
#include <boost/geometry/algorithms/azimuth.hpp>
#include <boost/geometry/algorithms/buffer.hpp>
#include <boost/geometry/algorithms/centroid.hpp>
#include <boost/geometry/algorithms/clear.hpp>
#include <boost/geometry/algorithms/closest_points.hpp>
#include <boost/geometry/algorithms/comparable_distance.hpp>
#include <boost/geometry/algorithms/convert.hpp>
#include <boost/geometry/algorithms/convex_hull.hpp>
#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/covered_by.hpp>
#include <boost/geometry/algorithms/crosses.hpp>
#include <boost/geometry/algorithms/densify.hpp>
#include <boost/geometry/algorithms/difference.hpp>
#include <boost/geometry/algorithms/discrete_frechet_distance.hpp>
#include <boost/geometry/algorithms/discrete_hausdorff_distance.hpp>
#include <boost/geometry/algorithms/disjoint.hpp>
#include <boost/geometry/algorithms/distance.hpp>
#include <boost/geometry/algorithms/envelope.hpp>
#include <boost/geometry/algorithms/equals.hpp>
#include <boost/geometry/algorithms/expand.hpp>
#include <boost/geometry/algorithms/for_each.hpp>
#include <boost/geometry/algorithms/intersection.hpp>
#include <boost/geometry/algorithms/intersects.hpp>
#include <boost/geometry/algorithms/is_convex.hpp>
#include <boost/geometry/algorithms/is_empty.hpp>
#include <boost/geometry/algorithms/is_simple.hpp>
#include <boost/geometry/algorithms/is_valid.hpp>
#include <boost/geometry/algorithms/length.hpp>
#include <boost/geometry/algorithms/line_interpolate.hpp>
#include <boost/geometry/algorithms/make.hpp>
#include <boost/geometry/algorithms/num_geometries.hpp>
#include <boost/geometry/algorithms/num_interior_rings.hpp>
#include <boost/geometry/algorithms/num_points.hpp>
#include <boost/geometry/algorithms/num_segments.hpp>
#include <boost/geometry/algorithms/overlaps.hpp>
#include <boost/geometry/algorithms/perimeter.hpp>
#include <boost/geometry/algorithms/point_on_surface.hpp>
#include <boost/geometry/algorithms/relate.hpp>
#include <boost/geometry/algorithms/relation.hpp>
#include <boost/geometry/algorithms/remove_spikes.hpp>
#include <boost/geometry/algorithms/reverse.hpp>
#include <boost/geometry/algorithms/simplify.hpp>
#include <boost/geometry/algorithms/sym_difference.hpp>
#include <boost/geometry/algorithms/touches.hpp>
#include <boost/geometry/algorithms/transform.hpp>
#include <boost/geometry/algorithms/union.hpp>
#include <boost/geometry/algorithms/unique.hpp>
#include <boost/geometry/algorithms/within.hpp>
#include <boost/geometry/arithmetic/arithmetic.hpp>
#include <boost/geometry/arithmetic/dot_product.hpp>
#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/closure.hpp>
#include <boost/geometry/core/coordinate_dimension.hpp>
#include <boost/geometry/core/coordinate_system.hpp>
#include <boost/geometry/core/coordinate_type.hpp>
#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/core/exterior_ring.hpp>
#include <boost/geometry/core/geometry_types.hpp>
#include <boost/geometry/core/interior_rings.hpp>
#include <boost/geometry/core/interior_type.hpp>
#include <boost/geometry/core/point_order.hpp>
#include <boost/geometry/core/point_type.hpp>
#include <boost/geometry/core/radian_access.hpp>
#include <boost/geometry/core/radius.hpp>
#include <boost/geometry/core/ring_type.hpp>
#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/core/tag_cast.hpp>
#include <boost/geometry/core/tags.hpp>
#include <boost/geometry/core/topological_dimension.hpp>
#include <boost/geometry/core/visit.hpp>
#include <boost/geometry/geometries/concepts/check.hpp>
#include <boost/geometry/io/dsv/write.hpp>
#include <boost/geometry/io/io.hpp>
#include <boost/geometry/io/svg/write.hpp>
#include <boost/geometry/io/wkt/read.hpp>
#include <boost/geometry/io/wkt/write.hpp>
#include <boost/geometry/srs/srs.hpp>
#include <boost/geometry/util/for_each_coordinate.hpp>
#include <boost/geometry/util/math.hpp>
#include <boost/geometry/util/select_coordinate_type.hpp>
#include <boost/geometry/util/select_most_precise.hpp>
#include <boost/geometry/views/box_view.hpp>
#include <boost/geometry/views/closeable_view.hpp>
#include <boost/geometry/views/identity_view.hpp>
#include <boost/geometry/views/reversible_view.hpp>
#include <boost/geometry/views/segment_view.hpp>

namespace gfx::generators::voxel
{
    class LightInfluenceStorage
    {
    public:

        explicit LightInfluenceStorage(usize maxLightId);
        ~LightInfluenceStorage();

        LightInfluenceStorage(const LightInfluenceStorage&)             = delete;
        LightInfluenceStorage(LightInfluenceStorage&&)                  = delete;
        LightInfluenceStorage& operator= (const LightInfluenceStorage&) = delete;
        LightInfluenceStorage& operator= (LightInfluenceStorage&&)      = delete;

        // these lightIds must be below the constructor parameter maxLightId

        void insert(u16 lightId, GpuRaytracedLight);
        void update(u16 lightId, GpuRaytracedLight);
        void remove(u16 lightId);

        // returns true if any lights changed
        bool pack();

        std::vector<u16> poll(AlignedChunkCoordinate aC);

    private:

        using R3Point = boost::geometry::model::point<float, 3, boost::geometry::cs::cartesian>;
        using R3Box   = boost::geometry::model::box<R3Point>;

        using LightWithId = std::pair<R3Box, u16>;

        bool                                                                          would_pack_benefit;
        boost::geometry::index::rtree<LightWithId, boost::geometry::index::rstar<16>> tree_storage;
        std::vector<GpuRaytracedLight>                                                light_lookup;

        friend R3Box getAABBFromLight(GpuRaytracedLight);
    };
} // namespace gfx::generators::voxel