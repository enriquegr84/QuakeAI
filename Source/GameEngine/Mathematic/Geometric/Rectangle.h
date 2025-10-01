// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2017
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#ifndef RECTANGLE_H
#define RECTANGLE_H

#include "Mathematic/Algebra/Vector.h"

enum RectangleAxisOrientation
{
    //! Horizontal axis
    RAO_HORIZONTAL = 0,
    //! Vertical axis
    RAO_VERTICAL,
    //! not used, counts the number of enumerated items
    RAO_COUNT
};

enum RectangleVerticePosition
{
    RVP_UPPERLEFT = 0,
    RVP_UPPERRIGHT,
    RVP_LOWERLEFT,
    RVP_LOWERRIGHT,
    RVP_COUNT
};

// Points are R(s0,s1) = C + s0*A0 + s1*A1, where C is the center of the
// rectangle and A0 and A1 are unit-length and perpendicular axes.  The
// parameters s0 and s1 are constrained by |s0| <= e0 and |s1| <= e1,
// where e0 > 0 and e1 > 0 are the extents of the rectangle.
template <int N, typename Real>
class RectangleShape
{
public:
    // Construction and destruction.  The default constructor sets the origin
    // to (0,...,0), axis A0 to (1,0,...,0), axis A1 to (0,1,0,...0), and both
    // extents to 1.
    RectangleShape();
    RectangleShape(Vector<N, Real> const& inCenter,
        std::array<Vector<N, Real>, 2> const& inAxis,
        Vector<2, Real> const& inExtent);

    bool IsValid() const;
	bool IsPointInside(const Vector2<Real>& point) const;
    bool IsColliding(const RectangleShape<N, Real>& other) const;

    // Compute the vertices of the rectangle.  If index i has the bit pattern
    // i = b[1]b[0], then
    //   vertex[i] = center + sum_{d=0}^{1} sign[d] * extent[d] * axis[d]
    // where sign[d] = 2*b[d] - 1.
    Vector<N, Real> GetVertice(RectangleVerticePosition position) const;

    //! Adds a point to the rectangle
    /** Causes the rectangle to grow bigger if point is outside of
    the box
    \param p Point to add to the box. */
    void AddInternalPoint(const Vector<N, Real>& point);

    //! Clips this rectangle with another one.
    /** \param other Rectangle to clip with */
    void ClipAgainst(const RectangleShape<N, Real>& other);

    //! Moves this rectangle to fit inside another one.
    /** \return True on success, false if not possible */
    bool ConstrainTo(const RectangleShape<N, Real>& other);

    //! Returns size of rectangle
    Real GetArea() const { return mExtent[0] * mExtent[1]; }

    std::array<Vector<N, Real>, 2> mAxis;
    Vector<N, Real> mExtent, mCenter;

public:
    // Comparisons to support sorted containers.
    bool operator==(RectangleShape const& rectangle) const;
    bool operator!=(RectangleShape const& rectangle) const;
    bool operator< (RectangleShape const& rectangle) const;
    bool operator<=(RectangleShape const& rectangle) const;
    bool operator> (RectangleShape const& rectangle) const;
    bool operator>=(RectangleShape const& rectangle) const;
};

// Template alias for convenience.
template <typename Real>
using Rectangle3 = RectangleShape<3, Real>;


template <int N, typename Real>
RectangleShape<N, Real>::RectangleShape()
{
    mCenter.MakeZero();
    for (int i = 0; i < RAO_COUNT; ++i)
    {
        mAxis[i].MakeUnit(i);
        mExtent[i] = (Real)0;
    }
}

template <int N, typename Real>
RectangleShape<N, Real>::RectangleShape(Vector<N, Real> const& inCenter,
	std::array<Vector<N, Real>, 2> const& inAxis,
    Vector<2, Real> const& inExtent)
    :
    mCenter(inCenter),
    mAxis(inAxis),
    mExtent(inExtent)
{
}

template <int N, typename Real>
bool RectangleShape<N, Real>::IsPointInside(const Vector2<Real>& point) const
{
	return (
        GetVertice(RVP_UPPERLEFT)[0] <= point[0] &&
        GetVertice(RVP_LOWERRIGHT)[0] >= point[0] &&
        GetVertice(RVP_UPPERLEFT)[1] <= point[1] &&
        GetVertice(RVP_LOWERRIGHT)[1] >= point[1]);
}

template <int N, typename Real>
bool RectangleShape<N, Real>::IsColliding(const RectangleShape<N, Real>& other) const
{
    return (
        GetVertice(RVP_LOWERRIGHT)[0] > other.GetVertice(RVP_UPPERLEFT)[0] &&
        GetVertice(RVP_UPPERLEFT)[0] < other.GetVertice(RVP_LOWERRIGHT)[0] &&
        GetVertice(RVP_LOWERRIGHT)[1] > other.GetVertice(RVP_UPPERLEFT)[1] &&
        GetVertice(RVP_UPPERLEFT)[1] < other.GetVertice(RVP_LOWERRIGHT)[1]);
}

template <int N, typename Real>
Vector<N, Real> RectangleShape<N, Real>::GetVertice(RectangleVerticePosition position) const
{
    Vector<N, Real> product0;
    Vector<N, Real> product1;
    Vector<N, Real> vertice;
    switch (position)
    {
        case RVP_UPPERLEFT:
            product0 = (mExtent[0] / 2) * mAxis[RAO_HORIZONTAL];
            product1 = (mExtent[1] / 2) * mAxis[RAO_VERTICAL];
            vertice = mCenter - product0 - product1;
            break;
        case RVP_UPPERRIGHT:
            product0 = (Real)round(mExtent[0] / 2.f) * mAxis[RAO_HORIZONTAL];
            product1 = (mExtent[1] / 2) * mAxis[RAO_VERTICAL];
            vertice = mCenter + product0 - product1;
            break;
        case RVP_LOWERLEFT:
            product0 = (mExtent[0] / 2) * mAxis[RAO_HORIZONTAL];
            product1 = (Real)round(mExtent[1] / 2.f) * mAxis[RAO_VERTICAL];
            vertice = mCenter - product0 + product1;
            break;
        case RVP_LOWERRIGHT:
            product0 = (Real)round(mExtent[0] / 2.f) * mAxis[RAO_HORIZONTAL];
            product1 = (Real)round(mExtent[1] / 2.f) * mAxis[RAO_VERTICAL];
            vertice = mCenter + product0 + product1;
            break;
        default:
            break;
    }

    return vertice;
}

//! Returns if the rect is valid to draw.
/** It would be invalid if the UpperLeftCorner is lower or more
right than the LowerRightCorner. */
template <int N, typename Real>
bool RectangleShape<N, Real>::IsValid() const
{
    return mExtent >= Vector<N, Real>::Zero();
}


//! Adds a point to the rectangle
/** Causes the rectangle to grow bigger if point is outside of the box
\param p Point to add to the box. */
template <int N, typename Real>
void RectangleShape<N, Real>::AddInternalPoint(const Vector<N, Real>& point)
{
    Vector<N, Real> lowerRightCorner = GetVertice(RVP_LOWERRIGHT);
    Vector<N, Real> upperLeftCorner = GetVertice(RVP_UPPERLEFT);

    //point compare with lowerright
    if (point[0] > lowerRightCorner[0])
        lowerRightCorner[0] = point[0];
    if (point[1] > lowerRightCorner[1])
        lowerRightCorner[1] = point[1];

    //point compare with upperleft
    if (point[0] < upperLeftCorner[0])
        upperLeftCorner[0] = point[0];
    if (point[1] < upperLeftCorner[1])
        upperLeftCorner[1] = point[1];

    mExtent[0] = lowerRightCorner[0] - upperLeftCorner[0];
    mCenter[0] = upperLeftCorner[0] + mExtent[0] / 2;
    mExtent[1] = lowerRightCorner[1] - upperLeftCorner[1];
    mCenter[1] = upperLeftCorner[1] + mExtent[1] / 2;
}


//! Clips this rectangle with another one.
/** \param other Rectangle to clip with */
template <int N, typename Real>
void RectangleShape<N, Real>::ClipAgainst(const RectangleShape<N, Real>& other)
{
    Vector<N, Real> lowerRightCorner = GetVertice(RVP_LOWERRIGHT);
    Vector<N, Real> upperLeftCorner = GetVertice(RVP_UPPERLEFT);
    Vector<N, Real> otherLowerRightCorner = other.GetVertice(RVP_LOWERRIGHT);
    Vector<N, Real> otherUpperLeftCorner = other.GetVertice(RVP_UPPERLEFT);

    //other lowerright compare with this lowerright
    if (otherLowerRightCorner[0] < lowerRightCorner[0])
        lowerRightCorner[0] = otherLowerRightCorner[0];
    if (otherLowerRightCorner[1] < lowerRightCorner[1])
        lowerRightCorner[1] = otherLowerRightCorner[1];

    //other upperleft compare with this lowerright
    if (otherUpperLeftCorner[0] > lowerRightCorner[0])
        lowerRightCorner[0] = otherUpperLeftCorner[0];
    if (otherUpperLeftCorner[1] > lowerRightCorner[1])
        lowerRightCorner[1] = otherUpperLeftCorner[1];

    //other lowerright compare with this upperleft
    if (otherLowerRightCorner[0] < upperLeftCorner[0])
        upperLeftCorner[0] = otherLowerRightCorner[0];
    if (otherLowerRightCorner[1] < upperLeftCorner[1])
        upperLeftCorner[1] = otherLowerRightCorner[1];

    //other upperleft compare with this upperleft
    if (otherUpperLeftCorner[0] > upperLeftCorner[0])
        upperLeftCorner[0] = otherUpperLeftCorner[0];
    if (otherUpperLeftCorner[1] > upperLeftCorner[1])
        upperLeftCorner[1] = otherUpperLeftCorner[1];

    mExtent[0] = lowerRightCorner[0] - upperLeftCorner[0];
    mCenter[0] = upperLeftCorner[0] + mExtent[0] / 2;
    mExtent[1] = lowerRightCorner[1] - upperLeftCorner[1];
    mCenter[1] = upperLeftCorner[1] + mExtent[1] / 2;
}


//! Moves this rectangle to fit inside another one.
/** \return True on success, false if not possible */
template <int N, typename Real>
bool RectangleShape<N, Real>::ConstrainTo(const RectangleShape<N, Real>& other)
{
    if (other.mExtent[0] < mExtent[0] || other.mExtent[1] < mExtent[1])
        return false;

    Vector<N, Real> lowerRightCorner = GetVertice(RVP_LOWERRIGHT);
    Vector<N, Real> upperLeftCorner = GetVertice(RVP_UPPERLEFT);
    Vector<N, Real> otherLowerRightCorner = other.GetVertice(RVP_LOWERRIGHT);
    Vector<N, Real> otherUpperLeftCorner = other.GetVertice(RVP_UPPERLEFT);

    Real diff = otherLowerRightCorner[0] - lowerRightCorner[0];
    if (diff < 0)
        mCenter[0] += diff;

    diff = otherLowerRightCorner[1] - lowerRightCorner[1];
    if (diff < 0)
        mCenter[1] += diff;

    diff = upperLeftCorner[0] - otherUpperLeftCorner[0];
    if (diff < 0)
        mCenter[0] -= diff;

    diff = upperLeftCorner[1] - otherUpperLeftCorner[1];
    if (diff < 0)
        mCenter[1] -= diff;

    return true;
}

template <int N, typename Real>
bool RectangleShape<N, Real>::operator==(RectangleShape const& rectangle) const
{
    if (mCenter != rectangle.mCenter)
        return false;

    for (int i = 0; i < RAO_COUNT; ++i)
        if (mAxis[i] != rectangle.mAxis[i])
            return false;

    for (int i = 0; i < RAO_COUNT; ++i)
        if (mExtent[i] != rectangle.mExtent[i])
            return false;

    return true;
}

template <int N, typename Real>
bool RectangleShape<N, Real>::operator!=(RectangleShape const& rectangle) const
{
    return !operator==(rectangle);
}

template <int N, typename Real>
bool RectangleShape<N, Real>::operator<(RectangleShape const& rectangle) const
{
    if (mCenter < rectangle.mCenter)
        return true;

    if (mCenter > rectangle.mCenter)
        return false;

    if (mAxis < rectangle.mAxis)
        return true;

    if (mAxis > rectangle.mAxis)
        return false;

    return mExtent < rectangle.mExtent;
}

template <int N, typename Real>
bool RectangleShape<N, Real>::operator<=(RectangleShape const& rectangle) const
{
    return operator<(rectangle) || operator==(rectangle);
}

template <int N, typename Real>
bool RectangleShape<N, Real>::operator>(RectangleShape const& rectangle) const
{
    return !operator<=(rectangle);
}

template <int N, typename Real>
bool RectangleShape<N, Real>::operator>=(RectangleShape const& rectangle) const
{
    return !operator<(rectangle);
}

#endif