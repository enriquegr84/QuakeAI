#ifndef METRICSBACKEND_H
#define METRICSBACKEND_H

#include "GameEngineStd.h"

#include "Core/Threading/MutexAutolock.h"
#include "Core/Threading/Thread.h"

class MetricCounter
{
public:
	MetricCounter() = default;

	virtual ~MetricCounter() {}

	virtual void Increment(double number = 1.0) = 0;
	virtual double Get() const = 0;
};

typedef std::shared_ptr<MetricCounter> MetricCounterPtr;

class SimpleMetricCounter : public MetricCounter
{
public:
	SimpleMetricCounter() = delete;

	virtual ~SimpleMetricCounter() {}

	SimpleMetricCounter(const std::string& name, const std::string& helpStr) :
			MetricCounter(), mName(name), mHelpStr(helpStr), mCounter(0.0)
	{

	}

	virtual void Increment(double number)
	{
		MutexAutoLock lock(mMutex);
		mCounter += number;
	}
	virtual double Get() const
	{
		MutexAutoLock lock(mMutex);
		return mCounter;
	}

private:
    std::string mName;
    std::string mHelpStr;

	mutable std::mutex mMutex;
	double mCounter;
};

class MetricGauge
{
public:
	MetricGauge() = default;
	virtual ~MetricGauge() {}

	virtual void Increment(double number = 1.0) = 0;
	virtual void Decrement(double number = 1.0) = 0;
	virtual void Set(double number) = 0;
	virtual double Get() const = 0;
};

typedef std::shared_ptr<MetricGauge> MetricGaugePtr;

class SimpleMetricGauge : public MetricGauge
{
public:
	SimpleMetricGauge() = delete;

	SimpleMetricGauge(const std::string& name, const std::string& helpStr) :
			MetricGauge(), mName(name), mHelpStr(helpStr), mGauge(0.0)
	{
	}

	virtual ~SimpleMetricGauge() {}

	virtual void Increment(double number)
	{
		MutexAutoLock lock(mMutex);
		mGauge += number;
	}
	virtual void Decrement(double number)
	{
		MutexAutoLock lock(mMutex);
		mGauge -= number;
	}
	virtual void Set(double number)
	{
		MutexAutoLock lock(mMutex);
		mGauge = number;
	}
	virtual double Get() const
	{
		MutexAutoLock lock(mMutex);
		return mGauge;
	}

private:
    std::string mName;
    std::string mHelpStr;

	mutable std::mutex mMutex;
	double mGauge;
};

class MetricsBackend
{
public:
	MetricsBackend() = default;

	virtual ~MetricsBackend() {}

	virtual MetricCounterPtr AddCounter(
			const std::string& name, const std::string& helpStr);
	virtual MetricGaugePtr AddGauge(
			const std::string& name, const std::string& helpStr);
};

#endif