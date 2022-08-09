#pragma once
#include <time.h>
#include "IOService.h"
#include "SysUtils.h"
#include "UDPChannel.h"
#include "TCPChannel.h"
#include "StringUtils.h"
namespace amm {

struct alignas(64) TimeContext {
	enum {
		TimeLen = 21
	};

	TimeContext()
	:_fixTimeUpdated(false)
	{
		memset(_tbuf, 0, sizeof(_tbuf));
		update();
		getFixTime(true);
	}

	void update()
	{
		clock_gettime(CLOCK_REALTIME, &_ts);
		_fixTimeUpdated = false;
	}

	const char* getFixTime(bool full = false)
	{
		if (!_fixTimeUpdated)
		{
			auto tm = gmtime((time_t *)&_ts.tv_sec);
			if (full)
			{
				uint2str(tm->tm_year + 1900, _tbuf);
				_tbuf[8] = '-';
				_tbuf[11] = ':';
				_tbuf[14] = ':';
				_tbuf[17] = '.';
			}

			uint2str<2>(tm->tm_mon + 1, &_tbuf[4], '0');
			uint2str<2>(tm->tm_mday, &_tbuf[6], '0');
			uint2str<2>(tm->tm_hour, &_tbuf[9], '0');
			uint2str<2>(tm->tm_min, &_tbuf[12], '0');
			uint2str<2>(tm->tm_sec, &_tbuf[15], '0');
			uint2str<3>(_ts.tv_nsec/1000000, &_tbuf[18], '0');

			//sprintf(_tbuf, "%04d%02d%02d-%02d:%02d:%02d.%03d",
			//   tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, (int)_ts.tv_nsec/1000000);
			_fixTimeUpdated = true;
		}

		return _tbuf;
	}

	struct timespec _ts;
	bool _fixTimeUpdated;
	char _tbuf[64];
};

template <
typename EventListenerT,
typename ConfigT,
typename TaskDerivedT>
class IOTaskBase
{
public:
	typedef IOService IOServiceT;

	IOTaskBase(ConfigT& conf)
	:_running(false)
	,_thread(NULL)
	,_cpuCore(-1)
	,_eventListener(NULL)
	{

	}

	void start()
	{
		std::cout << "Starting IOTask at cpu [" << _cpuCore << "].."<< std::endl;
		_running = true;
		_thread = new std::thread([this] { this->run();});
	}

	void join()
	{
		_thread->join();
	}

	void stop()  { _running = false; }

	void run()
	{
		setCpuAffinity(_cpuCore);
		while (_running)
		{
			_timeContext.update();
			timerEval();
			_io.run_once();
			if (!static_cast<TaskDerivedT *>(this)->iterate())
				return;
		}

		std::cout << "Stopping IO Thread at [" << _cpuCore << "].."<< std::endl;
	}

	void registerEventListener(EventListenerT* eventListener)
	{
		_eventListener = eventListener;
	}
	EventListenerT& eventDispatcher() {  return _eventDispatcher; }

	void setCPUCore(int coreid) { _cpuCore = coreid; }
	IOServiceT& io() { return _io; }
	TimeContext& timeContext() { return _timeContext; }


	//timer
	typedef Delegate<void (uint64_t refTime)> TimerCallback;

	struct TimerObj
	{
		template <typename CallbackT>
		TimerObj(uint64_t interval, CallbackT& callback, uint64_t refTime)
		:_interval(interval)
		,_nextEvalTime(refTime + interval)
		,_onTimerEvent(CREATE_DELEGATE_TEMPLATE(&CallbackT::timerCallback, &callback))
		{}


		void eval(uint64_t refTime)
		{
			if (refTime < _nextEvalTime)
				return;
			_nextEvalTime += _interval;
			_onTimerEvent(refTime);
		}

		uint64_t _interval;
		uint64_t _nextEvalTime;
		TimerCallback _onTimerEvent;
	};

	typedef std::vector<TimerObj> TimerListT;

	template <typename CallbackT>
	void addTimer(CallbackT* c, uint64_t interval )
	{
		_timers.emplace_back(TimerObj(interval, *c, _timeContext._ts.tv_sec));
	}

	void timerEval()
	{
		for (auto& it : _timers)
			it.eval(_timeContext._ts.tv_sec);
	}


protected:
	bool _running;
	IOServiceT _io;
	TimeContext _timeContext;
	std::thread *_thread;
	int _cpuCore;
	EventListenerT* _eventListener;
	EventListenerT _eventDispatcher;
	TimerListT _timers;
};

} // end of name space
