// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef LINE3_H
#define LINE3_H

#include "Mathematic/Algebra/Vector3.h"
#include "Mathematic/Function/Functions.h"

//! 3D line between two points with intersection methods.
template <typename Real>
class Line3
{
	public:

		//! Default constructor
		/** line from (0,0,0) to (1,1,1) */
        Line3() : mStart{ 0,0,0 }, mEnd{ 1, 1, 1 } {}
		//! Constructor with two points
		Line3(Real xa, Real ya, Real za, Real xb, Real yb, Real zb) : mStart(xa, ya, za), mEnd(xb, yb, zb) {}
		//! Constructor with two points as vectors
		Line3(const Vector3<Real>& mStart, const Vector3<Real>& mEnd) : mStart(mStart), mEnd(mEnd) {}

		// operators
		Line3<Real> operator+(const Vector3<Real>& point) const { return Line3<Real>(mStart + point, mEnd + point); }
		Line3<Real>& operator+=(const Vector3<Real>& point) { mStart += point; mEnd += point; return *this; }

		Line3<Real> operator-(const Vector3<Real>& point) const { return Line3<Real>(mStart - point, mEnd - point); }
		Line3<Real>& operator-=(const Vector3<Real>& point) { mStart -= point; mEnd -= point; return *this; }

		bool operator==(const Line3<Real>& other) const
		{ return (mStart==other.mStart && mEnd==other.mEnd) || (mEnd==other.mStart && mStart==other.mEnd);}
		bool operator!=(const Line3<Real>& other) const
		{ return !(mStart==other.mStart && mEnd==other.mEnd) || (mEnd==other.mStart && mStart==other.mEnd);}

		// functions
		//! Set this line to a new line going through the two points.
		void SetLine(const Real& xa, const Real& ya, const Real& za, const Real& xb, const Real& yb, const Real& zb)
		{mStart.Set(xa, ya, za); mEnd.Set(xb, yb, zb);}
		//! Set this line to a new line going through the two points.
		void SetLine(const Vector3<Real>& start, const Vector3<Real>& end)
		{mStart.Set(start); mEnd.Set(end);}
		//! Set this line to new line given as parameter.
		void SetLine(const Line3<Real>& line) {mStart.Set(line.mStart); mEnd.Set(line.mEnd);}

		//! Get length of line
		/** \return Length of line. */
		Real GetLength() const { return Length(mEnd - mStart); }

		//! Get middle of line
		/** \return Center of line. */
		Vector3<Real> GetMiddle() const { return (mStart + mEnd)/(Real)2; }

		//! Get vector of line
		/** \return vector of line. */
		Vector3<Real> GetVector() const { return mEnd - mStart; }

		//! Check if the given point is between mStart and mEnd of the line.
		/** Assumes that the point is already somewhere on the line.
		\param point The point to test.
		\return True if point is on the line between mStart and mEnd, else false.
		*/
		bool IsPointBetweenStartAndEnd(const Vector3<Real>& point) const
		{
			return point.IsBetweenPoints(mStart, mEnd);
		}

		//! Get the closest point on this line to a point
		/** \param point The point to compare to.
		\return The nearest point which is part of the line. */
		Vector3<Real> GetClosestPoint(const Vector3<Real>& point) const
		{
			Vector3<Real> c = point - mStart;
			Vector3<Real> v = mEnd - mStart;
			Real d = Length(v);
			v /= d;
			Real t = Dot(v,c);

			if (t < (Real)0.0)
				return mStart;
			if (t > d)
				return mEnd;

			v *= t;
			return mStart + v;
		}

		//! Check if the line intersects with a shpere
		/** \param sorigin: Origin of the shpere.
		\param sradius: Radius of the sphere.
		\param outdistance: The distance to the first intersection point.
		\return True if there is an intersection.
		If there is one, the distance to the first intersection point
		is stored in outdistance. */
		bool Intersect(Vector3<Real> sorigin, Real sradius, double& outdistance) const
		{
			const Vector3<Real> q = sorigin - mStart;
            Real c = Length(q);
            Real v = Dot(q, GetVector());
            Normalize(v);
            Real d = sradius * sradius - (c*c - v*v);

			if (d < 0.0)
				return false;

			outdistance = v - Function<Real>::Sqrt(d);
			return true;
		}

		// member variables

		//! mStart point of line
		Vector3<Real> mStart;
		//! mEnd point of line
		Vector3<Real> mEnd;
};


#endif

