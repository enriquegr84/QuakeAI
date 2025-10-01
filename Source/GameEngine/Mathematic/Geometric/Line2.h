// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef LINE2_H
#define LINE2_H

#include "Mathematic/Algebra/Vector2.h"
#include "Mathematic/Function/Functions.h"

//! 2D line between two points with intersection methods.
template <typename Real>
class Line2
{
	public:
		//! Default constructor for line going from (0,0) to (1,1).
		Line2() : mStart(0,0), mEnd(1,1) {}
		//! Constructor for line between the two points.
		Line2(Real xa, Real ya, Real xb, Real yb) : mStart(xa, ya), mEnd(xb, yb) {}
		//! Constructor for line between the two points given as vectors.
		Line2(const Vector2<Real>& start, const Vector2<Real>& end) : mStart(start), mEnd(end) {}
		//! Copy constructor.
		Line2(const Line2<Real>& other) : mStart(other.mStart), mEnd(other.mEnd) {}

		// operators
		Line2<Real> operator+(const Vector2<Real>& point) const 
        { return Line2<Real>(mStart + point, mEnd + point); }
		Line2<Real>& operator+=(const Vector2<Real>& point) 
        { mStart += point; mEnd += point; return *this; }

		Line2<Real> operator-(const Vector2<Real>& point) const 
        { return Line2<Real>(mStart - point, mEnd - point); }
		Line2<Real>& operator-=(const Vector2<Real>& point) 
        { mStart -= point; mEnd -= point; return *this; }

		bool operator==(const Line2<Real>& other) const
		{ return (mStart==other.mStart && mEnd==other.mEnd) || 
            (mEnd==other.mStart && mStart==other.mEnd);}
		bool operator!=(const Line2<Real>& other) const
		{ return !(mStart==other.mStart && mEnd==other.mEnd) || 
            (mEnd==other.mStart && mStart==other.mEnd);}

		// functions
		//! Set this line to new line going through the two points.
		void SetLine(const Real& xa, const Real& ya, const Real& xb, const Real& yb)
        {mStart.Set(xa, ya); mEnd.Set(xb, yb);}
		//! Set this line to new line going through the two points.
		void SetLine(const Vector2<Real>& nstart, const Vector2<Real>& nend)
        {mStart.Set(nstart); mEnd.Set(nend);}
		//! Set this line to new line given as parameter.
		void SetLine(const Line2<Real>& line)
        {mStart.Set(line.mStart); mEnd.Set(line.mEnd);}

		//! Get length of line
		/** \return Length of the line. */
		Real GetLength() const { return Length(mStart, mEnd); }

		//! Get middle of the line
		/** \return center of the line. */
		Vector2<Real> GetMiddle() const { return (mStart + mEnd)/(Real)2; }

		//! Get the vector of the line.
		/** \return The vector of the line. */
		Vector2<Real> GetVector() const { return Vector2<Real>(mEnd[0] - mStart[0], mEnd[1] - mStart[1]); }

		//! Tests if this line intersects with another line.
		/** \param l: Other line to test intersection with.
		\param CheckOnlySegments: Default is to check intersection between the begin and mEndpoints.
		When set to false the function will check for the first intersection point when extmEnding the lines.
		\param out: If there is an intersection, the location of the
		intersection will be stored in this vector.
		\return True if there is an intersection, false if not. */
		bool Intersect(const Line2<Real>& l, Vector2<Real>& out, bool checkOnlySegments=true) const
		{
			// Uses the method given at:
			// http://local.wasp.uwa.edu.au/~pbourke/geometry/lineLine2/
			const float commonDenominator = (float)(l.mEnd[1] - l.mStart[1])*(mEnd[0] - mStart[0]) -
											       (l.mEnd[0] - l.mStart[0])*(mEnd[1] - mStart[1]);

			const float numeratorA = (float)(l.mEnd[0] - l.mStart[0])*(mStart[1] - l.mStart[1]) -
											(l.mEnd[1] - l.mStart[1])*(mStart[0] -l.mStart[0]);

			const float numeratorB = (float)(mEnd[0] - mStart[0])*(mStart[1] - l.mStart[1]) -
											(mEnd[1] - mStart[1])*(mStart[0] -l.mStart[0]);

			if(Function<float>::Equals(commonDenominator, 0.f))
			{
				// The lines are either coincident or parallel
				// if both numerators are 0, the lines are coincident
				if(Function<float>::Equals(numeratorA, 0.f) && 
                    Function<float>::Equals(numeratorB, 0.f))
				{
					// Try and find a common mEndpoint
					if(l.mStart == mStart || l.mEnd == mStart)
						out = mStart;
					else if(l.mEnd == mEnd || l.mStart == mEnd)
						out = mEnd;
					// now check if the two segments are disjunct
					else if (l.mStart[0]>mStart[0] && l.mEnd[0]>mStart[0] && l.mStart[0]>mEnd[0] && l.mEnd[0]>mEnd[0])
						return false;
					else if (l.mStart[1]>mStart[1] && l.mEnd[1]>mStart[1] && l.mStart[1]>mEnd[1] && l.mEnd[1]>mEnd[1])
						return false;
					else if (l.mStart[0]<mStart[0] && l.mEnd[0]<mStart[0] && l.mStart[0]<mEnd[0] && l.mEnd[0]<mEnd[0])
						return false;
					else if (l.mStart[1]<mStart[1] && l.mEnd[1]<mStart[1] && l.mStart[1]<mEnd[1] && l.mEnd[1]<mEnd[1])
						return false;
					// else the lines are overlapping to some extent
					else
					{
						// find the points which are not contributing to the
						// common part
						Vector2<Real> maxp;
						Vector2<Real> minp;
						if ((mStart[0]>l.mStart[0] && mStart[0]>l.mEnd[0] && mStart[0]>mEnd[0]) || 
                            (mStart[1]>l.mStart[1] && mStart[1]>l.mEnd[1] && mStart[1]>mEnd[1]))
							maxp=mStart;
						else if ((mEnd[0]>l.mStart[0] && mEnd[0]>l.mEnd[0] && mEnd[0]>mStart[0]) || 
                            (mEnd[1]>l.mStart[1] && mEnd[1]>l.mEnd[1] && mEnd[1]>mStart[1]))
							maxp=mEnd;
						else if ((l.mStart[0]>mStart[0] && l.mStart[0]>l.mEnd[0] && l.mStart[0]>mEnd[0]) || 
                            (l.mStart[1]>mStart[1] && l.mStart[1]>l.mEnd[1] && l.mStart[1]>mEnd[1]))
							maxp=l.mStart;
						else
							maxp=l.mEnd;
						if (maxp != mStart && ((mStart[0]<l.mStart[0] && mStart[0]<l.mEnd[0] && mStart[0]<mEnd[0]) || 
                            (mStart[1]<l.mStart[1] && mStart[1]<l.mEnd[1] && mStart[1]<mEnd[1])))
							minp=mStart;
						else if (maxp != mEnd && ((mEnd[0]<l.mStart[0] && mEnd[0]<l.mEnd[0] && mEnd[0]<mStart[0]) || 
                            (mEnd[1]<l.mStart[1] && mEnd[1]<l.mEnd[1] && mEnd[1]<mStart[1])))
							minp=mEnd;
						else if (maxp != l.mStart && ((l.mStart[0]<mStart[0] && l.mStart[0]<l.mEnd[0] && l.mStart[0]<mEnd[0]) || 
                            (l.mStart[1]<mStart[1] && l.mStart[1]<l.mEnd[1] && l.mStart[1]<mEnd[1])))
							minp=l.mStart;
						else
							minp=l.mEnd;

						// one line is contained in the other. Pick the center
						// of the remaining points, which overlap for sure
						out = Vector2<Real>();
						if (mStart != maxp && mStart != minp)
							out += mStart;
						if (mEnd != maxp && mEnd != minp)
							out += mEnd;
						if (l.mStart != maxp && l.mStart != minp)
							out += l.mStart;
						if (l.mEnd != maxp && l.mEnd != minp)
							out += l.mEnd;
						out[0] = (Real)(out[0]/2);
						out[1] = (Real)(out[1]/2);
					}

					return true; // coincident
				}

				return false; // parallel
			}

			// Get the point of intersection on this line, checking that
			// it is within the line segment.
			const float uA = numeratorA / commonDenominator;
			if(checkOnlySegments && (uA < 0.f || uA > 1.f) )
				return false; // Outside the line segment

			const float uB = numeratorB / commonDenominator;
			if(checkOnlySegments && (uB < 0.f || uB > 1.f))
				return false; // Outside the line segment

			// Calculate the intersection point.
			out[0] = (T)(mStart[0] + uA * (mEnd[0] - mStart[0]));
			out[1] = (T)(mStart[1] + uA * (mEnd[1] - mStart[1]));
			return true;
		}

		//! Get unit vector of the line.
		/** \return Unit vector of this line. */
		Vector2<Real> GetUnitVector() const
		{
			Real len = (Real)(1.0 / getLength());
			return Vector2<Real>((mEnd[0] - mStart[0]) * len, (mEnd[1] - mStart[1]) * len);
		}

		//! Get angle between this line and given line.
		/** \param l Other line for test.
		\return Angle in degrees. */
		double GetAngle(const Line2<Real>& l) const
		{
			Vector2<Real> vect = getVector();
			Vector2<Real> vect2 = l.getVector();
			return vect.GetAngle(vect2);
		}

		//! Tells us if the given point lies to the left, right, or on the line.
		/** \return 0 if the point is on the line
		<0 if to the left, or >0 if to the right. */
		Real GetPointOrientation(const Vector2<Real>& point) const
		{
			return ( (mEnd[0] - mStart[0]) * (point[1] - mStart[1]) -
					(point[0] - mStart[0]) * (mEnd[1] - mStart[1]) );
		}

		//! Check if the given point is a member of the line
		/** \return True if point is between start and mEnd, else false. */
		bool IsPointOnLine(const Vector2<Real>& point) const
		{
			Real d = GetPointOrientation(point);
			return (d == 0 && point.isBetweenPoints(mStart, mEnd));
		}

		//! Check if the given point is between start and mEnd of the line.
		/** Assumes that the point is already somewhere on the line. */
		bool IsPointBetweenStartAndEnd(const Vector2<Real>& point) const
		{
			return point.IsBetweenPoints(mStart, mEnd);
		}

		//! Get the closest point on this line to a point
		/** \param CheckOnlySegments: Default (true) is to return a point on the line-segment (between begin and mEnd) of the line.
		When set to false the function will check for the first the closest point on the the line even when outside the segment. */
		Vector2<Real> GetClosestPoint(const Vector2<Real>& point, bool checkOnlySegments=true) const
		{
			Vector2<double> c((double)(point[0]-mStart[0]), (double)(point[1]- mStart[1]));
			Vector2<double> v((double)(mEnd[0]-mStart[0]), (double)(mEnd[1]-mStart[1]));
			double d = Length(v);
			if ( d == 0 )	// can't tell much when the line is just a single point
				return mStart;
			v /= d;
			double t = Dot(v,c);

			if ( checkOnlySegments )
			{
				if (t < 0) return Vector2<Real>((Real)mStart[0], (Real)mStart[1]);
				if (t > d) return Vector2<Real>((Real)mEnd[0], (Real)mEnd[1]);
			}

			v *= t;
			return Vector2<Real>((Real)(mStart[0] + v[0]), (Real)(mStart[1] + v[1]));
		}

		//! Start point of the line.
		Vector2<Real> mStart;
		//! mEnd point of the line.
		Vector2<Real> mEnd;
};

// partial specialization to optimize <float> lines (avoiding casts)
template <>
inline Vector2<float> Line2<float>::GetClosestPoint(const Vector2<float>& point, bool checkOnlySegments) const
{
	Vector2<float> c = point - mStart;
	Vector2<float> v = mEnd - mStart;
	float d = Length(v);
	if ( d == 0 )	// can't tell much when the line is just a single point
		return mStart;
	v /= d;
	float t = Dot(v,c);

	if (checkOnlySegments)
	{
		if (t < 0) return mStart;
		if (t > d) return mEnd;
	}

	v *= t;
	return mStart + v;
}

#endif

