/*
Minetest
Copyright (C) 2013 sapier <sapier AT gmx DOT net>

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

#ifndef CONDITIONVARIABLE_H
#define CONDITIONVARIABLE_H

#include "GameEngineStd.h"

#include <condition_variable>

/** A syncronization primitive that will wake up one waiting thread when signaled.
 * Calling @c signal() multiple times before a waiting thread has had a chance
 * to notice the signal will wake only one thread.  Additionally, if no threads
 * are waiting on the event when it is signaled, the next call to @c wait()
 * will return (almost) immediately.
 */
class ConditionVariable
{
public:

	void Wait();
	void Signal();

private:
	std::condition_variable mConditionVariable;
	std::mutex mMutex;
	bool mNotified = false;
};

#endif