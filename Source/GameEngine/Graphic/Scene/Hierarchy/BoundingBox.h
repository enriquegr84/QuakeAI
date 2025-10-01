// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#ifndef BOUNDINGBOX_H
#define BOUNDINGBOX_H

#include "Graphic/Scene/Visibility/CullingPlane.h"

#include "Mathematic/Algebra/Transform.h"

#include <optional>

//! Axis aligned bounding box in 3d dimensional space.
//! Has some useful methods used with occlusion culling or clipping.
template <typename Real>
class GRAPHIC_ITEM BoundingBox
{
public:

    //! Default Constructor.
    BoundingBox();
    //! Constructor with only one point.
    BoundingBox(const Vector3<Real>& init);
    //! Constructor with min edge and max edge.
    BoundingBox(const Vector3<Real>& min, const Vector3<Real>& max);
    //! Constructor with min edge and max edge as single values, not vectors.
    BoundingBox(Real minx, Real miny, Real minz, Real maxx, Real maxy, Real maxz);
    //! Copy Constructor
    BoundingBox(BoundingBox<Real> const& box);

    ~BoundingBox();

    // operators
    //! Equality operator
    /** \param other box to compare with.
    \return True if both boxes are equal, else false. */
    bool operator==(const BoundingBox<Real>& other) const;
    //! Inequality operator
    /** \param other box to compare with.
    \return True if both boxes are different, else false. */
    bool operator!=(const BoundingBox<Real>& other) const;

    // Assignment.
    BoundingBox& operator= (BoundingBox<Real> const& box);

    //! Resets the bounding box to a one-point box.
    /** \param x X coord of the point.
    \param y Y coord of the point.
    \param z Z coord of the point. */
    void Reset(Real x, Real y, Real z);

    //! Resets the bounding box.
    /** \param initValue New box to set this one to. */
    void Reset(const BoundingBox<Real>& initValue);

    //! Resets the bounding box to a one-point box.
    /** \param initValue New point. */
    void Reset(const Vector3<Real>& initValue);

    // The "positive side" of the plane is the half space to which the
    // plane normal is directed.  The "negative side" is the other half
    // space.  The function returns +1 when the box is fully on the
    // positive side, -1 when the box is fully on the negative side, or
    // 0 when the box is transversely cut by the plane (box volume on
    // each side of plane is positive).
    int WhichSide(CullingPlane const& plane) const;

    //! Adds a point to the bounding box
    /** The box grows bigger, if point is outside of the box.
    \param x X coordinate of the point to add to this box.
    \param y Y coordinate of the point to add to this box.
    \param z Z coordinate of the point to add to this box. */
    void GrowToContain(Real x, Real y, Real z);
    //! Adds a point to the bounding box
    /** The box grows bigger, if point was outside of the box.
    \param point: Point to add into the box. */
    void GrowToContain(const Vector3<Real>& point);
    //! Adds another bounding box
    /** The box grows bigger, if the new box was outside of the box.
    \param box: Other bounding box to add into this box. */
    void GrowToContain(const BoundingBox<Real>& box);

    //! Transforms a axis aligned bounding box
    /** The result box of this operation should be accurate */
    void Transformation(const Matrix4x4<Real>& mat);

    //! Get center of the bounding box
    /** \return Center of the bounding box. */
    Vector3<Real> GetCenter() const;

    //! Get extent of the box (maximal distance of two points in the box)
    /** \return Extent of the bounding box. */
    Vector3<Real> GetExtent() const;

    //! Get radius of the bounding sphere
    /** \return Radius of the bounding sphere. */
    Real GetRadius() const;

    //! Check if the box is empty.
    /** This means that there is no space between the min and max edge.
    \return True if box is empty, else false. */
    bool IsEmpty() const;

    //! Get the volume enclosed by the box in cubed units
    Real GetVolume() const;

    //! Get the surface area of the box in squared units
    Real GetArea() const;

    //! Stores all 8 edges of the box into an array
    /** \param edges: Pointer to array of 8 edges. */
    void GetEdges(Vector3<Real> *edges) const;

    // Check if MaxEdge > MinEdge
    bool IsValid() const;

    //! Repairs the box.
    /** Necessary if for example MinEdge and MaxEdge are swapped. */
    void Repair();

    //! Calculates a new interpolated bounding box.
    /** d=0 returns other, d=1 returns this, all other values blend between
    the two boxes.
    \param other Other box to interpolate between
    \param d Value between 0.0f and 1.0f.
    \return Interpolated box. */
    BoundingBox<Real> GetInterpolated(const BoundingBox<Real>& other, float d) const;

    //! Determines if a point is within this box.
    /** Border is included (IS part of the box)!
    \param p: Point to check.
    \return True if the point is within the box and false if not */
    bool IsPointInside(const Vector3<Real>& p) const;

    //! Determines if a point is within this box and not its borders.
    /** Border is excluded (NOT part of the box)!
    \param p: Point to check.
    \return True if the point is within the box and false if not. */
    bool IsPointTotalInside(const Vector3<Real>& p) const;

    //! Check if this box is completely inside the 'other' box.
    /** \param other: Other box to check against.
    \return True if this box is completely inside the other box,
    otherwise false. */
    bool IsFullInside(const BoundingBox<Real>& other) const;

    //! Determines if the axis-aligned box intersects with another axis-aligned box.
    /** \param other: Other box to check a intersection with.
    \return True if there is an intersection with the other box,
    otherwise false. */
    bool Intersect(const BoundingBox<Real>& other) const;

    // compute the near and far intersections of the bounding box
    // no intersection means tNear > tFar
    bool Intersect(const Vector3<Real>& origin, const Vector3<Real>& dir);

    //! The near edge
    Vector3<Real> mMinEdge;

    //! The far edge
    Vector3<Real> mMaxEdge;
};

template <typename Real>
BoundingBox<Real>::BoundingBox()
    : mMinEdge{ -1, -1, -1 }, mMaxEdge{ 1, 1, 1 }
{

}

template <typename Real>
BoundingBox<Real>::BoundingBox(const Vector3<Real>& init)
    : mMinEdge(init), mMaxEdge(init)
{

}

template <typename Real>
BoundingBox<Real>::BoundingBox(const Vector3<Real>& min, const Vector3<Real>& max)
    : mMinEdge(min), mMaxEdge(max)
{

}

template <typename Real>
BoundingBox<Real>::BoundingBox(Real minx, Real miny, Real minz, Real maxx, Real maxy, Real maxz)
    : mMinEdge{ minx, miny, minz }, mMaxEdge{ maxx, maxy, maxz }
{

}

template <typename Real>
BoundingBox<Real>::BoundingBox(BoundingBox<Real> const& box)
    : mMinEdge(box.mMinEdge), mMaxEdge(box.mMaxEdge)
{

}

template <typename Real>
BoundingBox<Real>::~BoundingBox()
{

}

template <typename Real>
bool BoundingBox<Real>::operator==(const BoundingBox<Real>& other) const
{
    return (mMinEdge == other.mMinEdge && other.mMaxEdge == mMaxEdge);
}

template <typename Real>
bool BoundingBox<Real>::operator!=(const BoundingBox<Real>& other) const
{
    return !(mMinEdge == other.mMinEdge && other.mMaxEdge == mMaxEdge);
}

template <typename Real>
BoundingBox<Real>& BoundingBox<Real>::operator= (BoundingBox<Real> const& box)
{
    mMinEdge = box.mMinEdge;
    mMaxEdge = box.mMaxEdge;
    return *this;
}

template <typename Real>
void BoundingBox<Real>::Reset(Real x, Real y, Real z)
{
    mMaxEdge = { x, y, z };
    mMinEdge = mMaxEdge;
}

template <typename Real>
void BoundingBox<Real>::Reset(const BoundingBox<Real>& initValue)
{
    *this = initValue;
}

template <typename Real>
void BoundingBox<Real>::Reset(const Vector3<Real>& initValue)
{
    mMaxEdge = initValue;
    mMinEdge = initValue;
}

template <typename Real>
int BoundingBox<Real>::WhichSide(CullingPlane const& plane) const
{
    Vector3<Real> nearPoint(mMaxEdge);
    Vector3<Real> farPoint(mMinEdge);

    Vector4<float> normal = plane.GetNormal();
    if (normal[0] > (Real)0)
    {
        nearPoint[0] = mMinEdge[0];
        farPoint[0] = mMaxEdge[0];
    }

    if (normal[1] > (Real)0)
    {
        nearPoint[1] = mMinEdge[1];
        farPoint[1] = mMaxEdge[1];
    }

    if (normal[2] > (Real)0)
    {
        nearPoint[2] = mMinEdge[2];
        farPoint[2] = mMaxEdge[2];
    }

    if (Dot(normal, nearPoint) + plane.GetConstant() > (Real)0)
        return +1;

    if (Dot(normal, farPoint) + plane.GetConstant() > (Real)0)
        return 0;

    return -1;
}

template <typename Real>
void BoundingBox<Real>::GrowToContain(Real x, Real y, Real z)
{
    if (x > mMaxEdge[0]) mMaxEdge[0] = x;
    if (y > mMaxEdge[1]) mMaxEdge[1] = y;
    if (z > mMaxEdge[2]) mMaxEdge[2] = z;

    if (x < mMinEdge[0]) mMinEdge[0] = x;
    if (y < mMinEdge[1]) mMinEdge[1] = y;
    if (z < mMinEdge[2]) mMinEdge[2] = z;
}

template <typename Real>
void BoundingBox<Real>::GrowToContain(const Vector3<Real>& point)
{
    GrowToContain(point[0], point[1], point[2]);
}

template <typename Real>
void BoundingBox<Real>::GrowToContain(const BoundingBox<Real>& box)
{
    GrowToContain(box.mMaxEdge);
    GrowToContain(box.mMinEdge);
}

template <typename Real>
void BoundingBox<Real>::Transformation(const Matrix4x4<Real>& mat)
{
    const Real aMin[3] = { mMinEdge[0], mMinEdge[1], mMinEdge[2] };
    const Real aMax[3] = { mMaxEdge[0], mMaxEdge[1], mMaxEdge[2] };

    Real bMin[3];
    Real bMax[3];

    bMin[0] = bMax[0] = mat[3];
    bMin[1] = bMax[1] = mat[7];
    bMin[2] = bMax[2] = mat[11];

    for (unsigned int i = 0; i < 3; ++i)
    {
        for (unsigned int j = 0; j < 3; ++j)
        {
            const Real a = mat(j, i) * aMin[j];
            const Real b = mat(j, i) * aMax[j];

            if (a < b)
            {
                bMin[i] += a;
                bMax[i] += b;
            }
            else
            {
                bMin[i] += b;
                bMax[i] += a;
            }
        }
    }

    mMinEdge[0] = bMin[0];
    mMinEdge[1] = bMin[1];
    mMinEdge[2] = bMin[2];

    mMaxEdge[0] = bMax[0];
    mMaxEdge[1] = bMax[1];
    mMaxEdge[2] = bMax[2];
}

template <typename Real>
Vector3<Real> BoundingBox<Real>::GetCenter() const
{
    return (mMinEdge + mMaxEdge) / (Real)2;
}

template <typename Real>
Vector3<Real> BoundingBox<Real>::GetExtent() const
{
    return mMaxEdge - mMinEdge;
}

template <typename Real>
Real BoundingBox<Real>::GetRadius() const
{
    return Length(GetExtent()) / 2;
}

template <typename Real>
bool BoundingBox<Real>::IsEmpty() const
{
    return mMinEdge == mMaxEdge;
}

template <typename Real>
Real BoundingBox<Real>::GetVolume() const
{
    const Vector3<Real> extent = GetExtent();
    return extent[0] * extent[1] * extent[2];
}

template <typename Real>
Real BoundingBox<Real>::GetArea() const
{
    const Vector3<Real> extent = GetExtent();
    return 2 * (extent[0] * extent[1] * extent[2]);
}

template <typename Real>
void BoundingBox<Real>::GetEdges(Vector3<Real>* edges) const
{
    const Vector3<Real> middle = GetCenter();
    const Vector3<Real> diag = middle - mMaxEdge;

    edges[0] = { middle[0] + diag[0], middle[1] + diag[1], middle[2] + diag[2] };
    edges[1] = { middle[0] + diag[0], middle[1] - diag[1], middle[2] + diag[2] };
    edges[2] = { middle[0] + diag[0], middle[1] + diag[1], middle[2] - diag[2] };
    edges[3] = { middle[0] + diag[0], middle[1] - diag[1], middle[2] - diag[2] };
    edges[4] = { middle[0] - diag[0], middle[1] + diag[1], middle[2] + diag[2] };
    edges[5] = { middle[0] - diag[0], middle[1] - diag[1], middle[2] + diag[2] };
    edges[6] = { middle[0] - diag[0], middle[1] + diag[1], middle[2] - diag[2] };
    edges[7] = { middle[0] - diag[0], middle[1] - diag[1], middle[2] - diag[2] };
}


template <typename Real>
void BoundingBox<Real>::Repair()
{
    Real t;
    if (mMinEdge[0] > mMaxEdge[0])
    {
        t = mMinEdge[0];
        mMinEdge[0] = mMaxEdge[0];
        mMaxEdge[0] = t;
    }
    if (mMinEdge[1] > mMaxEdge[1])
    {
        t = mMinEdge[1];
        mMinEdge[1] = mMaxEdge[1];
        mMaxEdge[1] = t;
    }
    if (mMinEdge[2] > mMaxEdge[2])
    {
        t = mMinEdge[2];
        mMinEdge[2] = mMaxEdge[2];
        mMaxEdge[2] = t;
    }
}

template <typename Real>
bool BoundingBox<Real>::IsValid() const
{
    if (mMinEdge[0] > mMaxEdge[0]) return false;
    if (mMinEdge[1] > mMaxEdge[1]) return false;
    if (mMinEdge[2] > mMaxEdge[2]) return false;

    return true;
}

template <typename Real>
BoundingBox<Real> BoundingBox<Real>::GetInterpolated(const BoundingBox<Real>& other, float d) const
{
    float inv = 1.0f - d;
    return BoundingBox<Real>((other.mMinEdge*inv) + (mMinEdge*d), (other.mMaxEdge*inv) + (mMaxEdge*d));
}

template <typename Real>
bool BoundingBox<Real>::IsPointInside(const Vector3<Real>& p) const
{
    return (p[0] >= mMinEdge[0] && p[0] <= mMaxEdge[0] &&
        p[1] >= mMinEdge[1] && p[1] <= mMaxEdge[1] &&
        p[2] >= mMinEdge[2] && p[2] <= mMaxEdge[2]);
}

template <typename Real>
bool BoundingBox<Real>::IsPointTotalInside(const Vector3<Real>& p) const
{
    return (p[0] > mMinEdge[0] && p[0] < mMaxEdge[0] &&
        p[1] > mMinEdge[1] && p[1] < mMaxEdge[1] &&
        p[2] > mMinEdge[2] && p[2] < mMaxEdge[2]);
}

template <typename Real>
bool BoundingBox<Real>::IsFullInside(const BoundingBox<Real>& other) const
{
    return (mMinEdge[0] >= other.mMinEdge[0] &&
        mMinEdge[1] >= other.mMinEdge[1] &&
        mMinEdge[2] >= other.mMinEdge[2] &&
        mMaxEdge[0] <= other.mMaxEdge[0] &&
        mMaxEdge[1] <= other.mMaxEdge[1] &&
        mMaxEdge[2] <= other.mMaxEdge[2]);
}

template <typename Real>
bool BoundingBox<Real>::Intersect(const BoundingBox<Real>& other) const
{
    return (mMinEdge[0] <= other.mMaxEdge[0] &&
        mMinEdge[1] <= other.mMaxEdge[1] &&
        mMinEdge[2] <= other.mMaxEdge[2] &&
        mMaxEdge[0] >= other.mMinEdge[0] &&
        mMaxEdge[1] >= other.mMinEdge[1] &&
        mMaxEdge[2] >= other.mMinEdge[2]);
}

template <typename Real>
bool BoundingBox<Real>::Intersect(const Vector3<Real>& origin, const Vector3<Real>& dir)
{
    Vector3<Real> tMin = (mMinEdge - origin) / dir;
    Vector3<Real> tMax = (mMaxEdge - origin) / dir;
    Vector3<Real> t1 = Vector3<Real>{
        (Real)std::min(tMin[0], tMax[0]),
        (Real)std::min(tMin[1], tMax[1]),
        (Real)std::min(tMin[2], tMax[2]) };
    Vector3<Real> t2 = Vector3<Real>{
        (Real)std::max(tMin[0], tMax[0]),
        (Real)std::max(tMin[1], tMax[1]),
        (Real)std::max(tMin[2], tMax[2]) };
    Real tNear = (Real)std::max(std::max(t1[0], t1[1]), t1[2]);
    Real tFar = (Real)std::min(std::min(t2[0], t2[1]), t2[2]);

    // if tmax < 0, ray (line) is intersecting AABB, but the whole AABB is behind us
    if (tFar < 0)
        return false;

    // if tmin > tmax, ray doesn't intersect AABB
    if (tNear > tFar)
        return false;

    return true;
}

#endif