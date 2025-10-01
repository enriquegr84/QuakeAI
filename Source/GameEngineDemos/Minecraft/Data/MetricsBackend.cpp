/*
Minetest
Copyright (C) 2013-2020 Minetest core developers team

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

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
