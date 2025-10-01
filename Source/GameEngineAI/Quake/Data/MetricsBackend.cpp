#include "MetricsBackend.h"

MetricCounterPtr MetricsBackend::AddCounter(
		const std::string& name, const std::string& helpStr)
{
	return std::make_shared<SimpleMetricCounter>(name, helpStr);
}

MetricGaugePtr MetricsBackend::AddGauge(
		const std::string& name, const std::string& helpStr)
{
	return std::make_shared<SimpleMetricGauge>(name, helpStr);
}
