// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "ParticleSphereEmitter.h"

#include "Graphic/Renderer/Renderer.h"
#include "Graphic/Effect/Material.h"

#include "Core/OS/OS.h"

#include "Graphic/Scene/Scene.h"

//! constructor
ParticleSphereEmitter::ParticleSphereEmitter(const Vector3<float>& center, float radius,
	const Vector3<float>& direction, unsigned int minParticlesPerSecond, unsigned int maxParticlesPerSecond, 
	const SColorF& minStartColor, const SColorF& maxStartColor, unsigned int lifeTimeMin, unsigned int lifeTimeMax,
	int maxAngleDegrees, const Vector2<float>& minStartSize, const Vector2<float>& maxStartSize )
:	mCenter(center), mRadius(radius), mDirection(direction),
	mMinStartSize(minStartSize), mMaxStartSize(maxStartSize),
	mMinParticlesPerSecond(minParticlesPerSecond),
	mMaxParticlesPerSecond(maxParticlesPerSecond),
	mMinStartColor(minStartColor), mMaxStartColor(maxStartColor),
	mMinLifeTime(lifeTimeMin), mMaxLifeTime(lifeTimeMax),
	mTime(0), mEmitted(0), mMaxAngleDegrees(maxAngleDegrees)
{

}

//! Prepares an array with new particles to emitt into the system
//! and returns how much new particles there are.
int ParticleSphereEmitter::Emitt(unsigned int now, unsigned int timeSinceLastCall, Particle*& outArray)
{
	mTime += timeSinceLastCall;

	const unsigned int pps = (mMaxParticlesPerSecond - mMinParticlesPerSecond);
	const float perSecond = pps ? ((float)mMinParticlesPerSecond + Randomizer::FRand() * pps) : mMinParticlesPerSecond;
	const float everyWhatMillisecond = 1000.0f / perSecond;

	if(mTime > everyWhatMillisecond)
	{
		unsigned int amount = (unsigned int)((mTime / everyWhatMillisecond) + 0.5f);
		mTime = 0;
		Particle particle;

		if(amount > mMaxParticlesPerSecond*2)
			amount = mMaxParticlesPerSecond * 2;

		for(unsigned int i=0; i<amount; ++i)
		{
			// Random distance from center
			const float distance = Randomizer::FRand() * mRadius;

			// Random direction from center
			particle.mPosition = Vector3<float>{ mCenter[0] + distance, mCenter[1], mCenter[2] + distance };
			Quaternion<float> tgt = Rotation<3, float>(
				AxisAngle<3, float>(Vector3<float>::Unit(2),Randomizer::FRand() * 360 * (float)GE_C_DEG_TO_RAD));
			particle.mPosition = HProject(Rotate(tgt, HLift(particle.mPosition, 0.f)));
			tgt = Rotation<3, float>(
				AxisAngle<3, float>(Vector3<float>::Unit(0), Randomizer::FRand() * 360 * (float)GE_C_DEG_TO_RAD));
			particle.mPosition = HProject(Rotate(tgt, HLift(particle.mPosition, 0.f)));
			tgt = Rotation<3, float>(
				AxisAngle<3, float>(Vector3<float>::Unit(1), Randomizer::FRand() * 360 * (float)GE_C_DEG_TO_RAD));
			particle.mPosition = HProject(Rotate(tgt, HLift(particle.mPosition, 0.f)));

			particle.mStartTime = now;
			particle.mVector = mDirection;

			if(mMaxAngleDegrees)
			{
				particle.mVector = mDirection;

				Quaternion<float> tgt = Rotation<3, float>(
					AxisAngle<3, float>(Vector3<float>::Unit(2), Randomizer::FRand() * mMaxAngleDegrees * (float)GE_C_DEG_TO_RAD));
				particle.mVector = HProject(Rotate(tgt, HLift(particle.mVector, 0.f)));
				tgt = Rotation<3, float>(
					AxisAngle<3, float>(Vector3<float>::Unit(0), Randomizer::FRand() * mMaxAngleDegrees * (float)GE_C_DEG_TO_RAD));
				particle.mVector = HProject(Rotate(tgt, HLift(particle.mVector, 0.f)));
				tgt = Rotation<3, float>(
					AxisAngle<3, float>(Vector3<float>::Unit(1), Randomizer::FRand() * mMaxAngleDegrees * (float)GE_C_DEG_TO_RAD));
				particle.mVector = HProject(Rotate(tgt, HLift(particle.mVector, 0.f)));
			}

			particle.mEndTime = now + mMinLifeTime;
			if (mMaxLifeTime != mMinLifeTime)
				particle.mEndTime += Randomizer::Rand() % (mMaxLifeTime - mMinLifeTime);

			if (mMinStartColor==mMaxStartColor)
				particle.mColor=mMinStartColor;
			else
				particle.mColor = SColorF(
                    Function<float>::Lerp(mMinStartColor.ToArray(), mMaxStartColor.ToArray(), Randomizer::FRand()));

			particle.mStartColor = particle.mColor;
			particle.mStartVector = particle.mVector;

			if (mMinStartSize==mMaxStartSize)
				particle.mStartSize = mMinStartSize;
			else
				particle.mStartSize = Function<float>::Lerp(mMinStartSize, mMaxStartSize, Randomizer::FRand());
			particle.mSize = particle.mStartSize;

			mParticles.push_back(particle);
		}

		outArray = mParticles.data();

		return (int)mParticles.size();
	}

	return 0;
}