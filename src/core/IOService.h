#pragma once

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <cassert>
#include <thread>
#include <iostream>
#include <vector>
#include <sys/epoll.h>

#include "Delegate.h"
#include "TypeUtils.h"

namespace amm {

class IOService
{
public:
	static const int max_events = 10;

	typedef Delegate<size_t(int, struct epoll_event&)> DataCallback;

	struct Channel
	{
		template <typename CallbackT>
		Channel(int fd, CallbackT& callback)
		:_fd(fd)
		,onPollEvent(CREATE_DELEGATE_TEMPLATE(&CallbackT::onPollEvent, &callback))
		{}

		int _fd;
		DataCallback onPollEvent;
	};

	IOService()
	:_running(false)
	{
		 epollfd = epoll_create(1);
		 if (epollfd == -1) {
		   perror("epoll_create error");
		   exit(EXIT_FAILURE);
		 }

		 _events = (struct epoll_event *)calloc(max_events, sizeof(struct epoll_event));
	}

	template <typename ChannelT>
	void add_channel(ChannelT* c, uint32_t events )
	{
		for (auto& it : _chans)
		{
			if (it._fd == c->fd())
			{
				LOGE("This channel is already registered, do nothing, fd(", c->fd(), ").");
				return;
			}
		}

		struct epoll_event event;
		event.data.fd = c->fd();
		event.events = events;
		auto s = epoll_ctl (epollfd, EPOLL_CTL_ADD, event.data.fd, &event);
		if (s == -1)
		{
			LOGE("EPOLL error adding fd ",c->fd());
			return;
		}

		_chans.emplace_back(Channel(c->fd(), *c));
	}

	void remove_channel(int fd)
	{
		for (auto it = _chans.begin(); it!=_chans.end();++it)
		{
			if (it->_fd == fd)
			{
				if (epoll_ctl (epollfd, EPOLL_CTL_DEL, fd, NULL) < 0)
				{
					LOGE("EPOLL_CTL_DEL failed ", fd, " error: ", errno);
					//return;
				}
				_chans.erase(it);
				return;
			}
		}
	}

	void stop() { _running = false; }

	void run_once()
	{
		int n = epoll_wait(epollfd, _events, max_events, 0);
		if (n == -1)
		{
			perror("epoll_pwait");
			exit(EXIT_FAILURE);
		}

		for (int i = 0; i < n; ++i)
		{
			auto& evt = _events[i];
			for (auto it = _chans.begin(); it < _chans.end(); ++it)
			{
				if (evt.data.fd == it->_fd)
				{
		//			LOGE("event on chan:", it->_fd, ";evt fd:" ,evt.data.fd, ";events:", evt.events, ";nevents:", n, ";idx:", i, ";chan size:", _chans.size())
					it->onPollEvent(epollfd, evt);
					break;
				}
//				else
	//				LOGE("event not on chan:", it->_fd, ";evt fd:" ,evt.data.fd, ";events:", evt.events, ";nevents:", n, ";idx:", i, ";chan size:", _chans.size())

			}
		}
	}

	void run()
	{
		_running = true;
		while (_running)
			run_once();
	}

private:
	int epollfd;
	bool _running;
	std::vector<Channel> _chans;
	struct epoll_event *_events;
};

} //end of namespace
