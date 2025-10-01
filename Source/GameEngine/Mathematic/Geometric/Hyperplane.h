// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2018
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2016/07/28)

#ifndef HYPERPLANE_H
#define HYPERPLANE_H

#include "Mathematic/Algebra/Vector.h"
#include "Mathematic/Function/Constants.h"
#include "Mathematic/NumericalMethod/SingularValueDecomposition.h"

// The plane is represented as Dot(U,X) = c where U is a unit-length normal
// vector, c is the plane constant, and X is any point on the plane.  The user
// must ensure that the normal vector is unit length.

template <int N, typename Real>
class Hyperplane
{
public:
    // Construction and destruction.  The default constructor sets the normal
    // to (0,...,0,1) and the constant to zero (plane z = 0).
    Hyperplane();

    // Specify U and c directly.
    Hyperplane(Vector<N, Real> const& inNormal, Real inConstant);

    // U is specified, c = Dot(U,p) where p is a point on the hyperplane.
    Hyperplane(Vector<N, Real> const& inNormal, Vector<N, Real> const& p);

	// U is a unit-length vector in the orthogonal complement of the set
	// {p[1]-p[0],...,p[n-1]-p[0]} and c = Dot(U,p[0]), where the p[i] are
	// pointson the hyperplane.
	Hyperplane(std::array<Vector<N, Real>, N> const& p);

	void SetPlane(const Vector<N, Real>& point, const Vector<N, Real>& normal);

	void SetPlane(const Vector<N, Real>& normal, Real constant);

	void SetPlane(const Vector<N, Real>& point1,
		const Vector<N, Real>& point2, const Vector<N, Real>& point3);

	//! Get an intersection with a line.
	/** \param lineVect Vector of the line to intersect with.
	\param linePoint Point of the line to intersect with.
	\param outIntersection Place to store the intersection point, if there is one.
	\return True if there was an intersection, false if there was not.
	*/
	bool Intersect(const Vector<N, Real>& linePoint,
		const Vector<N, Real>& lineVect, Vector<N, Real>& outIntersection) const;

	//! Recalculates the distance from origin by applying a new member point to the plane.
	void RecalculateConstant(const Vector<N, Real>& memberPoint);

	//! Gets a member point of the plane.
	Vector<N, Real> GetMemberPoint() const;

	//! Tests if there is an intersection with the other plane
	/** \return True if there is a intersection. */
	bool ExistsIntersection(const Hyperplane<N, Real>& other) const;

	//! Intersects this plane with another.
	/** \param other Other plane to intersect with.
	\param outLinePoint Base point of intersection line.
	\param outLineVect Vector of intersection.
	\return True if there is a intersection, false if not. */
	bool Intersect(const Hyperplane<N, Real>& other,
		Vector<N, Real>& outLinePoint, Vector<N, Real>& outLineVect) const;

	//! Get the intersection point with two other planes if there is one.
	bool Intersect(const Hyperplane<N, Real>& o1,
		const Hyperplane<N, Real>& o2, Hyperplane<N, Real>& outPoint) const;

	//! Test if the triangle would be front or backfacing from any point.
	/** Thus, this method assumes a camera position from
	which the triangle is definitely visible when looking into
	the given direction.
	Note that this only works if the normal is Normalized.
	Do not use this method with points as it will give wrong results!
	\param lookDirection: Look direction.
	\return True if the plane is front facing and
	false if it is backfacing. */
	bool IsFrontFacing(const Vector<N, Real>& lookDirection) const;

	//! Get the distance to a point.
	/** Note that this only works if the normal is normalized. */
	Real GetDistanceTo(const Vector<N, Real>& point) const;

    // Public member access.
    Vector<N, Real> mNormal;
    Real mConstant;

public:

    // Comparisons to support sorted containers.
    bool operator==(Hyperplane const& hyperplane) const;
    bool operator!=(Hyperplane const& hyperplane) const;
    bool operator< (Hyperplane const& hyperplane) const;
    bool operator<=(Hyperplane const& hyperplane) const;
    bool operator> (Hyperplane const& hyperplane) const;
    bool operator>=(Hyperplane const& hyperplane) const;
};

// Template alias for convenience.
template <typename Real>
using Plane3 = Hyperplane<3, Real>;

template <int N, typename Real>
Hyperplane<N, Real>::Hyperplane()
    :
    mConstant((Real)0)
{
    mNormal.MakeUnit(N - 1);
}

template <int N, typename Real>
Hyperplane<N, Real>::Hyperplane(Vector<N, Real> const& inNormal,
    Real inConstant)
    :
    mNormal(inNormal),
    mConstant(inConstant)
{
}

template <int N, typename Real>
Hyperplane<N, Real>::Hyperplane(Vector<N, Real> const& inNormal,
    Vector<N, Real> const& p)
    :
    mNormal(inNormal),
    mConstant(Dot(inNormal, p))
{
}

template <int N, typename Real>
Hyperplane<N, Real>::Hyperplane(std::array<Vector<N, Real>, N> const& p)
{
	Matrix<N, N - 1, Real> edge;
	for (int i = 0; i < N - 1; ++i)
		edge.SetCol(i, p[i + 1] - p[0]);

	// Compute the 1-dimensional orthogonal complement of the edges of the
	// simplex formed by the points p[].
	SingularValueDecomposition<Real> svd(N, N - 1, 32);
	svd.Solve(&edge[0], -1);
	svd.GetUColumn(N - 1, &mNormal[0]);

	mConstant = Dot(mNormal, p[0]);
}

template <int N, typename Real>
void Hyperplane<N, Real>::SetPlane(const Vector<N, Real>& point, const Vector<N, Real>& normal)
{
	mNormal = normal;
	RecalculateD(point);
}

template <int N, typename Real>
void Hyperplane<N, Real>::SetPlane(const Vector<N, Real>& normal, Real constant)
{
	mNormal = normal;
	mConstant = constant;
}

template <int N, typename Real>
void Hyperplane<N, Real>::SetPlane(const Vector<N, Real>& point1,
	const Vector<N, Real>& point2, const Vector<N, Real>& point3)
{
	// creates the plane from 3 memberpoints
	mNormal = Cross((point2 - point1), (point3 - point1));
	Normalize(mNormal);

	RecalculateConstant(point1);
}

template <int N, typename Real>
bool Hyperplane<N, Real>::Intersect(const Vector<N, Real>& linePoint,
	const Vector<N, Real>& lineVect, Vector<N, Real>& outIntersection) const
{
	Real t2 = Dot(mNormal, lineVect);

	if (t2 == 0)
		return false;

	Real t = -(Dot(mNormal, linePoint) + mConstant) / t2;
	outIntersection = linePoint + (lineVect * t);
	return true;
}

template <int N, typename Real>
void Hyperplane<N, Real>::RecalculateConstant(const Vector<N, Real>& memberPoint)
{
	mConstant = -Dot(memberPoint, mNormal);
}

template <int N, typename Real>
Vector<N, Real> Hyperplane<N, Real>::GetMemberPoint() const
{
	return mNormal * -mConstant;
}

template <int N, typename Real>
bool Hyperplane<N, Real>::ExistsIntersection(const Hyperplane<N, Real>& other) const
{
	Vector<N, Real> cross = other.mNormal.CrossProduct(Normal);
	return Length(cross) > GE_ROUNDING_ERROR;
}

template <int N, typename Real>
bool Hyperplane<N, Real>::Intersect(const Hyperplane<N, Real>& other,
	Vector<N, Real>& outLinePoint, Vector<N, Real>& outLineVect) const
{
	const Real fn00 = Length(mNormal);
	const Real fn01 = mNormal.Dot(other.mNormal);
	const Real fn11 = Length(other.mNormal);
	const double det = fn00 * fn11 - fn01 * fn01;

	if (fabs(det) < GE_ROUNDING_ERROR)
		return false;

	const double invdet = 1.0 / det;
	const double fc0 = (fn11 * -mConstant + fn01 * other.mConstant) * invdet;
	const double fc1 = (fn00 * -other.mConstant + fn01 * mConstant) * invdet;

	outLineVect = mNormal.CrossProduct(other.mNormal);
	outLinePoint = mNormal * (Real)fc0 + other.mNormal * (Real)fc1;
	return true;
}

template <int N, typename Real>
bool Hyperplane<N, Real>::Intersect(const Hyperplane<N, Real>& o1,
	const Hyperplane<N, Real>& o2, Hyperplane<N, Real>& outPoint) const
{
	Vector<N, Real> linePoint, lineVect;
	if (Intersect(o1, linePoint, lineVect))
		return o2.Intersect(linePoint, lineVect, outPoint);

	return false;
}

template <int N, typename Real>
bool Hyperplane<N, Real>::IsFrontFacing(const Vector<N, Real>& lookDirection) const
{
	const float d = Dot(mNormal, lookDirection);
	return d <= 0.0f;
}

template <int N, typename Real>
Real Hyperplane<N, Real>::GetDistanceTo(const Vector<N, Real>& point) const
{
	return Dot(point, mNormal) + mConstant;
}

template <int N, typename Real>
bool Hyperplane<N, Real>::operator==(Hyperplane const& hyperplane) const
{
    return mNormal == hyperplane.mNormal && mConstant == hyperplane.mConstant;
}

template <int N, typename Real>
bool Hyperplane<N, Real>::operator!=(Hyperplane const& hyperplane) const
{
    return !operator==(hyperplane);
}

template <int N, typename Real>
bool Hyperplane<N, Real>::operator<(Hyperplane const& hyperplane) const
{
    if (mNormal < hyperplane.mNormal)
        return true;

    if (mNormal > hyperplane.mNormal)
        return false;

    return mConstant < hyperplane.mConstant;
}

template <int N, typename Real>
bool Hyperplane<N, Real>::operator<=(Hyperplane const& hyperplane) const
{
    return operator<(hyperplane) || operator==(hyperplane);
}

template <int N, typename Real>
bool Hyperplane<N, Real>::operator>(Hyperplane const& hyperplane) const
{
    return !operator<=(hyperplane);
}

template <int N, typename Real>
bool Hyperplane<N, Real>::operator>=(Hyperplane const& hyperplane) const
{
    return !operator<(hyperplane);
}


#endif