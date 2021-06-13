// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * Authors:
 *   Parth Pant <parthpant4@gmail.com>
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_DISTRIBUTION_SNAPPER_H
#define SEEN_DISTRIBUTION_SNAPPER_H

#include <2geom/affine.h>

#include "snap-enums.h"
#include "snapper.h"
#include "snap-candidate.h"
#include "boost/graph/adjacency_list.hpp"

class SPDesktop;
class SPNamedView;
class SPItem;
class SPObject;
class SPPath;
class SPDesktop;

namespace Inkscape
{

/**
 * Snapping equidistant objects
 */
class DistributionSnapper : public Snapper
{

public:
    DistributionSnapper(SnapManager *sm, Geom::Coord const d);
    ~DistributionSnapper() override;

    /**
     * @return true if this Snapper will snap at least one kind of point.
     */
    bool ThisSnapperMightSnap() const override;

    /**
     * @return Snap tolerance (desktop coordinates); depends on current zoom so that it's always the same in screen pixels.
     */
    Geom::Coord getSnapperTolerance() const override; //returns the tolerance of the snapper in screen pixels (i.e. independent of zoom)

    bool getSnapperAlwaysSnap() const override; //if true, then the snapper will always snap, regardless of its tolerance

    void freeSnap(IntermSnapResults &isr,
                  Inkscape::SnapCandidatePoint const &p,
                  Geom::OptRect const &bbox_to_snap,
                  std::vector<SPItem const *> const *it,
                  std::vector<SnapCandidatePoint> *unselected_nodes) const override;

    void constrainedSnap(IntermSnapResults &isr,
                  Inkscape::SnapCandidatePoint const &p,
                  Geom::OptRect const &bbox_to_snap,
                  SnapConstraint const &c,
                  std::vector<SPItem const *> const *it,
                  std::vector<SnapCandidatePoint> *unselected_nodes) const override;

private:
    std::vector<Geom::Rect> *_bboxes_left;
    std::vector<Geom::Rect> *_bboxes_right;
    std::vector<Geom::Rect> *_bboxes_down;
    std::vector<Geom::Rect> *_bboxes_up;

    /** Collects and caches bounding boxes to the left, right, up, and down of the
     * selected object.
     * @param bounding box of the selected object 
     * @param is the point first point in the selection?
     */
    void _collectBBoxes(Geom::OptRect const &bbox_to_snap, bool const &first_point) const;

    /** Finds and snaps to points that is equidistant from surrounding bboxes
     * @param interm snap results
     * @param source point to snap 
     * @param bounding box of the selecton to snap
     * @param unselected nodes in case editing nodes (never used here, remove?)
     * @param active snap constraint
     * @param projection of the source point on the constraint (never used, remove?)
     */
    void _snapEquidistantPoints(IntermSnapResults &isr,
                         SnapCandidatePoint const &p,
                         Geom::OptRect const &bbox_to_snap,
                         std::vector<SnapCandidatePoint> *unselected_nodes,
                         SnapConstraint const &c =  SnapConstraint(),
                         Geom::Point const &p_proj_on_constraint = Geom::Point()) const;

    /** When the selection has more than one objects in it, the bounding box of
     * the object that the selection is grabbed from (closest to the pointer) is
     * snapped to the center of the overall bounding box of the selection. This
     * function corrects the target point to be a point where the bounding box of
     * that particular object must be snapped to.
     * @param snap target point that need to be snapped to
     * @param source point to snap (this bbox midpoint of the object closest to the mouse pointer)
     * @param bounding box of the active selection to snap
     */
    void _correctSelectionBBox(Geom::Point &target, Geom::Point const &p, Geom::Rect const &bbox_to_snap) const;

    /** Finds and stores the bounding boxes that are at equal distance from each other
     * @param the distance between the object that needs to be snapped and the first
     * object in the sideways vectors.
     * @param first iterator of the sideways vector
     * @param end of the sideways vector
     * @param vector where the snapped bboxes will be stored
     * @param equal distance between consecutive vectors
     * @param snapped tolerance 
     * @param a function pointer to the distance function
     * @param level of recursion - do not pass this while calling the function
     */
    bool findSidewaysSnaps(Geom::Coord first_dist,
                             std::vector<Geom::Rect>::iterator it,
                             std::vector<Geom::Rect>::iterator end,
                             std::vector<Geom::Rect> &vec,
                             Geom::Coord &dist,
                             Geom::Coord tol,
                             Geom::Coord(*distance_func)(Geom::Rect const&, Geom::Rect const&),
                             int level = 0) const;

    // distance functions for different orientations
    static Geom::Coord distRight(Geom::Rect const &a, Geom::Rect const &b);
    static Geom::Coord distLeft(Geom::Rect const &a, Geom::Rect const &b);
    static Geom::Coord distUp(Geom::Rect const &a, Geom::Rect const &b);
    static Geom::Coord distDown(Geom::Rect const &a, Geom::Rect const &b);
}; // end of AlignmentSnapper class

} // end of namespace Inkscape

#endif