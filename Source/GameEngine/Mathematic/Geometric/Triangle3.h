// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef TRIANGLE3_H
#define TRIANGLE3_H

#include "Mathematic/Algebra/Vector3.h"

#include "AlignedBox.h"
#include "Line3.h"

//! 3d triangle template class for doing collision detection and other things.
template <typename Real>
class Triangle3
{
public:

	//! Constructor for an all 0 triangle
	Triangle3() {}
	//! Constructor for triangle with given three vertices
	Triangle3(Vector3<Real> v1, Vector3<Real> v2, Vector3<Real> v3) : mPointA(v1), mPointB(v2), mPointC(v3) {}

	//! Equality operator
	bool operator==(const Triangle3<Real>& other) const
	{
		return other.mPointA==mPointA && other.mPointB==mPointB && other.mPointC==mPointC;
	}

	//! Inequality operator
	bool operator!=(const Triangle3<Real>& other) const
	{
		return !(*this==other);
	}

	//! Determines if the triangle is totally inside a bounding box.
	/** \param box Box to check.
	\return True if triangle is within the box, otherwise false. */
	bool IsTotalInsideBox(const AlignedBox<3, Real>& box) const
	{
		return (box.IsPointInside(mPointA) &&
			box.IsPointInside(mPointB) &&
			box.IsPointInside(mPointC));
	}

	//! Determines if the triangle is totally outside a bounding box.
	/** \param box Box to check.
	\return True if triangle is outside the box, otherwise false. */
	bool IsTotalOutsideBox(const AlignedBox<3, Real>& box) const
	{
		return ((mPointA[0] > box.mMax[0] && mPointB[0] > box.mMax[0] && mPointC[0] > box.mMax[0]) ||
			(mPointA[1] > box.mMax[1] && mPointB[1] > box.mMax[1] && mPointC[1] > box.mMax[1]) ||
			(mPointA[2] > box.mMax[2] && mPointB[2] > box.mMax[2] && mPointC[2] > box.mMax[2]) ||
			(mPointA[0] < box.mMin[0] && mPointB[0] < box.mMin[0] && mPointC[0] < box.mMin[0]) ||
			(mPointA[1] < box.mMin[1] && mPointB[1] < box.mMin[1] && mPointC[1] < box.mMin[1]) ||
			(mPointA[2] < box.mMin[2] && mPointB[2] < box.mMin[2] && mPointC[2] < box.mMin[2]));
	}

	//! Get the closest point on a triangle to a point on the same plane.
	/** \param p Point which must be on the same plane as the triangle.
	\return The closest point of the triangle */
    Vector3<Real> ClosestPointOnTriangle(const Vector3<Real>& p) const
	{
		const Vector3<Real> rab = Line3<Real>(mPointA, mPointB).GetClosestPoint(p);
		const Vector3<Real> rbc = Line3<Real>(mPointB, mPointC).GetClosestPoint(p);
		const Vector3<Real> rca = Line3<Real>(mPointC, mPointA).GetClosestPoint(p);

		const Real d1 = Length(p - rab);
		const Real d2 = Length(p - rbc);
		const Real d3 = Length(p - rca);

		if (d1 < d2)
			return d1 < d3 ? rab : rca;

		return d2 < d3 ? rbc : rca;
	}

	//! Check if a point is inside the triangle (border-points count also as inside)
	/*
	\param p Point to test. Assumes that this point is already
	on the plane of the triangle.
	\return True if the point is inside the triangle, otherwise false. */
	bool IsPointInside(const Vector3<Real>& p) const
	{
		Vector3<double> adouble((double)mPointA[0], (double)mPointA[1], (double)mPointA[2]);
		Vector3<double> bdouble((double)mPointB[0], (double)mPointB[1], (double)mPointB[2]);
		Vector3<double> cdouble((double)mPointC[0], (double)mPointC[1], (double)mPointC[2]);
		Vector3<double> pdouble((double)p[0], (double)p[1], (double)p[2]);
		return (IsOnSameSide(pdouble, adouble, bdouble, cdouble) &&
 			IsOnSameSide(pdouble, bdouble, adouble, cdouble) &&
 			IsOnSameSide(pdouble, cdouble, adouble, bdouble));
	}

	//! Check if a point is inside the triangle (border-points count also as inside)
	/** This method uses a barycentric coordinate system.
	It is faster than IsPointInside but is more susceptible to floating point rounding
	errors. This will especially be noticable when the FPU is in single precision mode
	(which is for example set on default by Direct3D).
	\param p Point to test. Assumes that this point is already
	on the plane of the triangle.
	\return True if point is inside the triangle, otherwise false. */
	bool IsPointInsideFast(const Vector3<Real>& p) const
	{
		const Vector3<Real> a = mPointC - mPointA;
		const Vector3<Real> b = mPointB - mPointA;
		const Vector3<Real> c = p - mPointA;

        const double dotAA = Dot(a, a);
        const double dotAB = Dot(a, b);
        const double dotAC = Dot(a, c);
        const double dotBB = Dot(b, b);
        const double dotBC = Dot(b, c);

		// get coordinates in barycentric coordinate system
		const double invDenom =  1/(dotAA * dotBB - dotAB * dotAB);
		const double u = (dotBB * dotAC - dotAB * dotBC) * invDenom;
		const double v = (dotAA * dotBC - dotAB * dotAC ) * invDenom;

		// We count border-points as inside to keep downward compatibility.
		// Rounding-error also needed for some test-cases.
		return (u > -GE_ROUNDING_ERROR) && (v >= 0) && (u + v < 1+GE_ROUNDING_ERROR);
	}


	//! Get an intersection with a 3d line.
	/** \param line Line to intersect with.
	\param outIntersection Place to store the intersection point, if there is one.
	\return True if there was an intersection, false if not. */
	bool IntersectLine(const Line3<Real>& line, Vector3<Real>& outIntersection) const
	{
		return IntersectLine(line.mStart, line.GetVector(), outIntersection) &&
			outIntersection.IsBetweenPoints(line.mStart, line.mEnd);
	}


	//! Get an intersection with a 3d line.
	/** Please note that also points are returned as intersection which
	are on the line, but not between the start and end point of the line.
	If you want the returned point be between start and end
	use getIntersectionWithLimitedLine().
	\param linePoint Point of the line to intersect with.
	\param lineVect Vector of the line to intersect with.
	\param outIntersection Place to store the intersection point, if there is one.
	\return True if there was an intersection, false if there was not. */
	bool IntersectLine(const Vector3<Real>& linePoint,
		const Vector3<Real>& lineVect, Vector3<Real>& outIntersection) const
	{
		if (IntersectPlane(linePoint, lineVect, outIntersection))
			return IsPointInside(outIntersection);

		return false;
	}


	//! Calculates the intersection between a 3d line and the plane the triangle is on.
	/** \param lineVect Vector of the line to intersect with.
	\param linePoint Point of the line to intersect with.
	\param outIntersection Place to store the intersection point, if there is one.
	\return True if there was an intersection, else false. */
	bool IntersectPlane(const Vector3<Real>& linePoint,
		const Vector3<Real>& lineVect, Vector3<Real>& outIntersection) const
	{
		// Work with double to get more precise results (makes enough difference to be worth the casts).
		const Vector3<double> linePointdouble(linePoint[0], linePoint[1], linePoint[2]);
		const Vector3<double> lineVectdouble(lineVect[0], lineVect[1], lineVect[2]);
		Vector3<double> outIntersectiondouble;

		Triangle3<double> triangledouble(
            Vector3<double>((double)mPointA[0], (double)mPointA[1], (double)mPointA[2]),
            Vector3<double>((double)mPointB[0], (double)mPointB[1], (double)mPointB[2]), 
            Vector3<double>((double)mPointC[0], (double)mPointC[1], (double)mPointC[2]));
		Vector3<double> normaldouble = triangledouble.GetNormal()
        Normalize(normaldouble);
		
        double t2;
		if (Function<double>::IsZero(t2 = Dot(normaldouble, lineVectdouble)))
			return false;

		double d = Dot(triangledouble.mPointA, normaldouble);
		double t = -(Dot(normaldouble, linePointdouble) - d) / t2;
		outIntersectiondouble = linePointdouble + (lineVectdouble * t);

		outIntersection[0] = (Real)outIntersectiondouble[0];
		outIntersection[1] = (Real)outIntersectiondouble[1];
		outIntersection[2] = (Real)outIntersectiondouble[2];
		return true;
	}


	//! Get the normal of the triangle.
	/** Please note: The normal is not always normalized. */
	Vector3<Real> GetNormal() const
	{
		return Cross(mPointB - mPointA, mPointC - mPointA);
	}

	//! Test if the triangle would be front or backfacing from any point.
	/** Thus, this method assumes a camera position from which the
	triangle is definitely visible when looking at the given direction.
	Do not use this method with points as it will give wrong results!
	\param lookDirection Look direction.
	\return True if the plane is front facing and false if it is backfacing. */
	bool IsFrontFacing(const Vector3<Real>& lookDirection) const
	{
        Vector3<Real> n = GetNormal();
        Normalize(n);
		const float d = Dot(n, lookDirection);
		return d <= 0.f;
	}

	//! Get the area of the triangle
	Real GetArea() const
	{
		return Cross((mPointB - mPointA), Length(mPointC - mPointA) * 0.5f);

	}

	//! sets the triangle's points
	void Set(const Vector3<Real>& a, const Vector3<Real>& b, const Vector3<Real>& c)
	{
		mPointA = a;
		mPointB = b;
		mPointC = c;
	}

	//! the three points of the triangle
	Vector3<Real> mPointA;
	Vector3<Real> mPointB;
	Vector3<Real> mPointC;

private:
	// Using double instead of <Real> to avoid integer overflows when T=int (maybe also less floating point troubles).
	bool IsOnSameSide(
        const Vector3<double>& p1, const Vector3<double>& p2,
		const Vector3<double>& a, const Vector3<double>& b) const
	{
		Vector3<double> bminusa = b - a;
		Vector3<double> cp1 = Cross(bminusa, (p1 - a));
		Vector3<double> cp2 = Cross(bminusa, (p2 - a));
		double res = Dot(cp1, cp2);
		if ( res < 0 )
		{
			// This catches some floating point troubles.
			// Unfortunately slightly expensive and we don't really know the best epsilon for iszero.
            Vector3<double> pminusa = p1 - a;
            Normalize(bminusa);
            Normalize(pminusa);
			Vector3<double> cp1 = Cross(bminusa, pminusa);
			if (Function<double>::IsZero(cp1[0], (double)GE_ROUNDING_ERROR) && 
                Function<double>::Iszero(cp1[1], (double)GE_ROUNDING_ERROR) && 
                Function<double>::Iszero(cp1[2], (double)GE_ROUNDING_ERROR) )
			{
				res = 0.f;
			}
		}
		return (res >= 0.0f);
	}
};

#endif

